/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2020 NITK Surathkal
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation;
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Authors: Vivek Jain <jain.vivek.anand@gmail.com>
 *          Deepak Kumaraswamy <deepakkavoor99@gmail.com>
 */

// The following network topology is used in this example, and is taken from
// Figure 2 of https://homes.cs.washington.edu/~tom/pubs/pacing.pdf
//
//    n0                          n4
//    |                           |
//    |(4x Mbps, 5ms)             |(4x Mbps, 5ms)
//    |                           |
//    |                           |
//    |      (x Mbps, 40ms)       |
//    n2 ------------------------ n3
//    |                           |
//    |                           |
//    |(4x Mbps, 5ms)             |(4x Mbps, 5ms)
//    |                           |
//    n1                          n5
//
//

// This example illustrates how TCP pacing can be enabled on a socket.
// Two long-running TCP flows are instantiated at nodes n0 and n1 to
// send data over a bottleneck link (n2->n3) to sink nodes n4 and n5.
// At the end of the simulation, the IP-level flow monitor tool will
// print out summary statistics of the flows.  The flow monitor detects
// four flows, but that is because the flow records are unidirectional;
// the latter two flows reported are actually ack streams.
//
// At the end of this simulation, data files are also generated
// that track changes in Congestion Window, Slow Start threshold and
// TCP pacing rate for the first flow (n0). Additionally, a data file
// that contains information about packet transmission and reception times
// (collected through TxTrace and RxTrace respectively) is also produced.
// This transmission and reception (ack) trace is the most direct way to
// observe the effects of pacing.  All the above information is traced
// just for the single node n0.
//
// A small amount of randomness is introduced to the program to control
// the start time of the flows.
//
// This example has pacing enabled by default, which means that TCP
// does not send packets back-to-back, but instead paces them out over
// an RTT. The size of initial congestion window is set to 10, and pacing
// of the initial window is enabled. The available command-line options and
// their default values can be observed in the usual way by running the
// program to print the help info; i.e.: ./waf --run 'tcp-pacing --PrintHelp'
//
// When pacing is disabled, TCP sends eligible packets back-to-back. The
// differences in behaviour when pacing is disabled can be observed from the
// packet transmission data file. For instance, one can observe that
// packets in the initial window are sent one after the other simultaneously,
// without any inter-packet gaps. Another instance is when n0 receives a
// packet in the form of an acknowledgement, and sends out data packets without
// pacing them.
//
// Although this example serves as a useful demonstration of how pacing could
// be enabled/disabled in ns-3 TCP congestion controls, we could not observe
// significant improvements in throughput for the above topology when pacing
// was enabled. In future, one could try and incorporate models such as
// TCP Prague and ACK-filtering, which may show a stronger performance
// impact for TCP pacing.

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

NS_LOG_COMPONENT_DEFINE("My Simulation 2");

std::ofstream cwndStream;
std::ofstream DataRateStream;
static void
CwndTracer(uint32_t oldval, uint32_t newval)
{
    cwndStream << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << std::setw(12) << newval << std::endl;
}

Ptr<PacketSink> sink, sink1;     /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0, lastTotalRx1 = 0; /* The value of the last total received bytes */

void CalculateThroughput()
{
    Time now = Simulator::Now();                                        /* Return the simulator's virtual time. */
    double cur = (sink->GetTotalRx() - lastTotalRx) * (double)8 / 1024; /* Convert Application RX Packets to KBits. */
    double cur2 = (sink1->GetTotalRx() - lastTotalRx1) * (double)8 / 1024;
    //std::cout << now.GetSeconds() << "s: \t" << cur << " Kbit/s" << std::endl;
    std::cout << now.GetSeconds() << "s: \t" << (cur + cur2)/2 << " Kbit/s" << std::endl;
    if(((cur+cur2)/2) != 0)
    DataRateStream << std::fixed << std::setprecision(6) << Simulator::Now().GetSeconds() << std::setw(12) << (cur+cur2)/2 << std::endl;
    lastTotalRx = sink->GetTotalRx();
    lastTotalRx1 = sink1->GetTotalRx();
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
    c.Create(6);

    NS_LOG_INFO("Create channels.");
    NodeContainer n0n2 = NodeContainer(c.Get(0), c.Get(2));
    NodeContainer n1n2 = NodeContainer(c.Get(1), c.Get(2));

    NodeContainer n2n3 = NodeContainer(c.Get(2), c.Get(3));

    NodeContainer n3n4 = NodeContainer(c.Get(3), c.Get(4));
    NodeContainer n3n5 = NodeContainer(c.Get(3), c.Get(5));

    // Define Node link properties
    PointToPointHelper regLink;
    regLink.SetDeviceAttribute("DataRate", DataRateValue(regLinkBandwidth));
    regLink.SetChannelAttribute("Delay", TimeValue(regLinkDelay));

    NetDeviceContainer d0d2 = regLink.Install(n0n2);
    NetDeviceContainer d1d2 = regLink.Install(n1n2);
    NetDeviceContainer d3d4 = regLink.Install(n3n4);
    NetDeviceContainer d3d5 = regLink.Install(n3n5);

    PointToPointHelper bottleNeckLink;
    bottleNeckLink.SetDeviceAttribute("DataRate", DataRateValue(bottleneckBandwidth));
    bottleNeckLink.SetChannelAttribute("Delay", TimeValue(bottleneckDelay));

    NetDeviceContainer d2d3 = bottleNeckLink.Install(n2n3);

    // Install Internet stack
    InternetStackHelper stack;
    stack.Install(c);

    // Install traffic control
    if (useQueueDisc)
    {
        TrafficControlHelper tchBottleneck;
        tchBottleneck.SetRootQueueDisc("ns3::FqCoDelQueueDisc");
        tchBottleneck.Install(d2d3);
    }

    NS_LOG_INFO("Assign IP Addresses.");
    Ipv4AddressHelper ipv4;
    ipv4.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer regLinkInterface0 = ipv4.Assign(d0d2);

    ipv4.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer regLinkInterface1 = ipv4.Assign(d1d2);

    ipv4.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer bottleneckInterface = ipv4.Assign(d2d3);

    ipv4.SetBase("10.1.4.0", "255.255.255.0");
    Ipv4InterfaceContainer regLinkInterface4 = ipv4.Assign(d3d4);

    ipv4.SetBase("10.1.5.0", "255.255.255.0");
    Ipv4InterfaceContainer regLinkInterface5 = ipv4.Assign(d3d5);

    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    NS_LOG_INFO("Create Applications.");

    // Two Sink Applications at n4 and n5
    uint16_t sinkPort = 8080;
    Address sinkAddress4(InetSocketAddress(regLinkInterface4.GetAddress(1), sinkPort)); // interface of n4
    Address sinkAddress5(InetSocketAddress(regLinkInterface5.GetAddress(1), sinkPort)); // interface of n5
    Address sinkAddress3(InetSocketAddress(bottleneckInterface.GetAddress(1), sinkPort)); // interface of n5
    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), sinkPort));
    ApplicationContainer sinkApps4 = packetSinkHelper.Install(c.Get(4)); // n4 as sink
    ApplicationContainer sinkApps5 = packetSinkHelper.Install(c.Get(5)); // n5 as sink
    ApplicationContainer sinkApps3 = packetSinkHelper.Install(c.Get(3)); // n5 as sink
    sink = StaticCast<PacketSink>(sinkApps4.Get(0));                     // this is to monitor the data rate
     sink1 = StaticCast<PacketSink>(sinkApps5.Get(0));                     // this is to monitor the data rate

    sinkApps4.Start(Seconds(0));
    sinkApps4.Stop(simulationEndTime);
    sinkApps5.Start(Seconds(0));
    sinkApps5.Stop(simulationEndTime);

    // Randomize the start time between 0 and 1ms
    Ptr<UniformRandomVariable> uniformRv = CreateObject<UniformRandomVariable>();
    uniformRv->SetStream(0);

    // Two Source Applications at n0 and n1
    BulkSendHelper source0("ns3::TcpSocketFactory", sinkAddress4);
    BulkSendHelper source1("ns3::TcpSocketFactory", sinkAddress5);
    // Set the amount of data to send in bytes.  Zero is unlimited.
    source0.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    source1.SetAttribute("MaxBytes", UintegerValue(maxBytes));
    ApplicationContainer sourceApps0 = source0.Install(c.Get(0));
    ApplicationContainer sourceApps1 = source1.Install(c.Get(1));

    sourceApps0.Start(MicroSeconds(uniformRv->GetInteger(0, 1000)));
    sourceApps0.Stop(simulationEndTime);
    sourceApps1.Start(MicroSeconds(uniformRv->GetInteger(0, 1000)));
    sourceApps1.Stop(simulationEndTime);

    if (tracing)
    {
        AsciiTraceHelper ascii;
        regLink.EnableAsciiAll(ascii.CreateFileStream("tcp-dynamic-pacing.tr"));
        regLink.EnablePcapAll("tcp-dynamic-pacing", false);
    }

    cwndStream.open("AIMDcwnd.dat", std::ios::out);
    DataRateStream.open("AIMDdata.dat", std::ios::out);

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


    // double averageThroughput = (((sink->GetTotalRx() * 8) / (1024 * simulationEndTime.GetSeconds())) + ((sink1->GetTotalRx() * 8) / (1024 * simulationEndTime.GetSeconds())))/2;
    // std::cout << "\nAverage throughput: " << averageThroughput << " Kbit/s" << std::endl;

    cwndStream.close();
    DataRateStream.close();
    Simulator::Destroy();
}
