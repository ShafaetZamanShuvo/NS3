
// n0 ----- n1 ----- n2 ----- n3

#include <iomanip>
#include <iostream>
#include <string>
#include <fstream>
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/traffic-control-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("My Simulation 3");

std::ofstream cwndStream;
std::ofstream DataRateStream;
static void
CwndTracer(uint32_t oldval, uint32_t newval)
{
    cwndStream << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << std::setw(12) << newval << std::endl;
}

Ptr<PacketSink> sink;     /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0; /* The value of the last total received bytes */

void CalculateThroughput()
{
    Time now = Simulator::Now();                                        /* Return the simulator's virtual time. */
    double cur = (sink->GetTotalRx() - lastTotalRx) * (double)8 / 1024; /* Convert Application RX Packets to KBits. */
    //std::cout << now.GetSeconds() << "s: \t" << cur << " Kbit/s" << std::endl;
    std::cout << now.GetSeconds() << "s: \t" << cur << " Kbit/s" << std::endl;
    if(cur != 0)
    DataRateStream << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << std::setw(12) << cur << std::endl;
    lastTotalRx = sink->GetTotalRx();
    Simulator::Schedule(MilliSeconds(500), &CalculateThroughput);
}

void ConnectSocketTraces(void)
{
    Config::ConnectWithoutContext("/NodeList/0/$ns3::TcpL4Protocol/SocketList/0/CongestionWindow", MakeCallback(&CwndTracer));
}

int main(int argc, char *argv[])
{
    bool tracing = false;

    uint32_t maxBytes = 0; // value of zero corresponds to unlimited send
    std::string transportProtocol = "ns3::TcpNewAIMD";
    //std::string transportProtocol = "ns3::TcpNewReno";

    uint32_t SentPackets = 0;
    uint32_t ReceivedPackets = 0;
    uint32_t LostPackets = 0;

    Time simulationEndTime = Seconds(20);
    DataRate bottleneckBandwidth("5Mbps"); // value of x as shown in the above network topology
    Time bottleneckDelay = MilliSeconds(30);
    DataRate regLinkBandwidth = DataRate(bottleneckBandwidth.GetBitRate());
    Time regLinkDelay = MilliSeconds(30);

    bool useQueueDisc = true;

    // Configure defaults that are not based on explicit command-line arguments
    // They may be overridden by general attribute configuration of command line
    Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(transportProtocol)));
    Config::SetDefault("ns3::TcpSocket::InitialCwnd", UintegerValue(5));

    NS_LOG_INFO("Create nodes.");
    NodeContainer c;
    c.Create(4);

    NS_LOG_INFO("Create channels.");
    NodeContainer n0n1 = NodeContainer(c.Get(0), c.Get(2));

    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));// will work as bottleneck

    NodeContainer n2n3 = NodeContainer(c.Get(2), c.Get(3)); 

    // Define Node link properties
    PointToPointHelper regLink;
    regLink.SetDeviceAttribute("DataRate", DataRateValue(regLinkBandwidth));
    regLink.SetChannelAttribute("Delay", TimeValue(regLinkDelay));

    NetDeviceContainer d0d1 = regLink.Install(n0n1);
    NetDeviceContainer d2d3 = regLink.Install(n2n3);


    PointToPointHelper bottleNeckLink;
    bottleNeckLink.SetDeviceAttribute("DataRate", DataRateValue(bottleneckBandwidth));
    bottleNeckLink.SetChannelAttribute("Delay", TimeValue(bottleneckDelay));

    NetDeviceContainer d1d2 = bottleNeckLink.Install(n1n2);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(c);

    // Install traffic control
    if (useQueueDisc)
    {
        TrafficControlHelper tchBottleneck;
        tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
        tchBottleneck.Install(d1d2);
    }

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer regLinkInterface0 = ipv4.Assign(d0d1);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterface = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer regLinkInterface1  = ipv4.Assign(d2d3);



    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");

    // one Sink Applications at n3
    uint16_t sinkPort = 8080;
    Address sinkAddress(InetSocketAddress(regLinkInterface1.GetAddress(1), sinkPort)); // interface of n4
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps = packetSinkHelper.Install(c.Get(3)); // n4 as sink
    sink = StaticCast<PacketSink>(sinkApps.Get(0));                     // this is to monitor the data rate                   // this is to monitor the data rate

    sinkApps.Start(Seconds(0));
    sinkApps.Stop(simulationEndTime);
   

    // Randomize the start time between 0 and 1ms
    Ptr<UniformRandomVariable> uniformRv = CreateObject<UniformRandomVariable>();
    uniformRv->SetStream(0);

    // Two Source Applications at n0 and n1
    BulkSendHelper source0("ns3::TcpSocketFactory", sinkAddress);
    // Set the amount of data to send in bytes.  Zero is unlimited.
    source0.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApps0 = source0.Install(c.Get(0));

    sourceApps0.Start(MicroSeconds(uniformRv->GetInteger(0, 1000)));
    sourceApps0.Stop(simulationEndTime);

    if (tracing)
    {
        AsciiTraceHelper ascii;
        regLink.EnableAsciiAll(ascii.CreateFileStream("tcp-dynamic-pacing.tr"));
        regLink.EnablePcapAll("tcp-dynamic-pacing", false);
    }

    cwndStream.open("AIMDcwnd2.dat", std::ios::out);
    DataRateStream.open("AIMDdata2.dat", std::ios::out);

    // cwndStream << "#Time(s) Congestion Window (B)" << std::endl;

    Simulator::Schedule(MicroSeconds(1001), &ConnectSocketTraces);
    Simulator::Schedule(Seconds(1.1), &CalculateThroughput);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    NS_LOG_INFO("Run Simulation.");
    Simulator::Stop(simulationEndTime);
    Simulator::Run();

    int j = 0;
    float AvgThroughput = 0;

    monitor->CheckForLostPackets();
    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();
    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator i = stats.begin(); i != stats.end(); ++i)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(i->first);

        std::cout << "Flow " << i->first << " (" << t.sourceAddress << " -> " << t.destinationAddress << ")\n";
        std::cout << "  Tx Packets: " << i->second.txPackets << "\n";
        std::cout << "  Tx Bytes:   " << i->second.txBytes << "\n";
        std::cout << "  TxOffered:  " << i->second.txBytes * 8.0 / simulationEndTime.GetSeconds() / 1000 / 1000 << " Mbps\n";
        std::cout << "  Rx Packets: " << i->second.rxPackets << "\n";
        std::cout << "  Rx Bytes:   " << i->second.rxBytes << "\n";
        std::cout << "Lost Packets =" << i->second.txPackets - i->second.rxPackets << "\n";
        std::cout << "Packet delivery ratio =" << i->second.rxPackets * 100 / i->second.txPackets << "%\n";
        std::cout << "Packet loss ratio =" << (i->second.txPackets - i->second.rxPackets) * 100 / i->second.txPackets << "%\n";
        std::cout <<  "Throughput: " << i->second.rxBytes * 8.0 / simulationEndTime.GetSeconds() / 1000 / 1000 << " Mbps\n";
        AvgThroughput = AvgThroughput + i->second.rxBytes * 8.0 / simulationEndTime.GetSeconds() / 1000 / 1000;
        SentPackets = SentPackets + (i->second.txPackets);
        ReceivedPackets = ReceivedPackets + (i->second.rxPackets);
        LostPackets = LostPackets + (i->second.txPackets - i->second.rxPackets);
        j = j + 1;
    }

    AvgThroughput = AvgThroughput / j;
    std::cout<<"--------Total Results of the simulation----------" << std::endl;
    std::cout<<"Total sent packets  =" << SentPackets<<"\n";
    std::cout<<"Total Received Packets =" << ReceivedPackets;
    std::cout<<"Total Lost Packets =" << LostPackets;
    std::cout<<"Packet Loss ratio =" << ((LostPackets * 100) / SentPackets) << "%\n";
    std::cout<<"Packet delivery ratio =" << ((ReceivedPackets * 100) / SentPackets) << "%\n";
    std::cout<<"Average Throughput =" << AvgThroughput << "Mbps\n";

    // double averageThroughput = (((sink->GetTotalRx() * 8) / (1024 * simulationEndTime.GetSeconds())) + ((sink->GetTotalRx() * 8) / (1024 * simulationEndTime.GetSeconds())))/2;
    // std::cout << "\nAverage throughput: " << averageThroughput << " Kbit/s" << std::endl;

    cwndStream.close();
    DataRateStream.close();
    Simulator::Destroy();
}
