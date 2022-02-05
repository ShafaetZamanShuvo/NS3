/*
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 * Author: Hany Assasa <hany.assasa@gmail.com>
.*
 * This is a simple example to test TCP over 802.11n (with MPDU aggregation enabled).
 *
 * Network topology:
 *
//   Wifi 10.1.2.0
//                 AP               AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0    1    2    3    4
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
// 1    2    3    4 point-to-point  |    |    |    |
//                                   *    *    *    *
//                                     Wifi 10.1.3.0
 *
 * In this example, an HT station sends TCP packets to the access point.
 * We report the total throughput received during a window of 100ms.
 * The user can specify the application data rate and choose the variant
 * of TCP i.e. congestion control algorithm to use.
 */
#include "ns3/netanim-module.h"
#include "ns3/command-line.h"
#include "ns3/config.h"
#include "ns3/string.h"
#include "ns3/log.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/mobility-model.h"
#include "ns3/packet-sink.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/tcp-westwood.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

NS_LOG_COMPONENT_DEFINE("wifi-tcp");

using namespace ns3;

Ptr<PacketSink> sink;     /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0; /* The value of the last total received bytes */

// void CalculateThroughput()
// {
//     Time now = Simulator::Now();                                       /* Return the simulator's virtual time. */
//     double cur = (sink->GetTotalRx() - lastTotalRx) * (double)8 / 1e5; /* Convert Application RX Packets to MBits. */
//     std::cout << now.GetSeconds() << "s: \t" << cur << " Mbit/s" << std::endl;
//     lastTotalRx = sink->GetTotalRx();
//     Simulator::Schedule(MilliSeconds(100), &CalculateThroughput);
// }

int main(int argc, char *argv[])
{
    uint32_t payloadSize = 1472;           /* Transport layer payload size in bytes. */
    std::string dataRate = "100Mbps";      /* Application layer datarate. */
    std::string tcpVariant = "TcpNewReno"; /* TCP variant type. */
    std::string phyRate = "HtMcs7";        /* Physical layer bitrate. */
    double simulationTime = 2;             /* Simulation time in seconds. */
    bool pcapTracing = false;              /* PCAP Tracing is enabled or not. */

    uint32_t nWifi = 5;
    int num_half_flow = 4;
    uint32_t SentPackets = 0;
    uint32_t ReceivedPackets = 0;
    uint32_t LostPackets = 0;

    /* Command line argument parser setup. */
    CommandLine cmd(__FILE__);
    cmd.AddValue("payloadSize", "Payload size in bytes", payloadSize);
    cmd.AddValue("dataRate", "Application data ate", dataRate);
    cmd.AddValue("tcpVariant", "Transport protocol to use: TcpNewReno, "
                               "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                               "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ",
                 tcpVariant);
    cmd.AddValue("phyRate", "Physical layer bitrate", phyRate);
    cmd.AddValue("simulationTime", "Simulation time in seconds", simulationTime);
    cmd.AddValue("pcap", "Enable/disable PCAP Tracing", pcapTracing);
    cmd.Parse(argc, argv);

    tcpVariant = std::string("ns3::") + tcpVariant;
    // Select TCP variant
    if (tcpVariant.compare("ns3::TcpWestwoodPlus") == 0)
    {
        // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpWestwood::GetTypeId()));
        // the default protocol type in ns3::TcpWestwood is WESTWOOD
        Config::SetDefault("ns3::TcpWestwood::ProtocolType", EnumValue(TcpWestwood::WESTWOODPLUS));
    }
    else
    {
        TypeId tcpTid;
        NS_ABORT_MSG_UNLESS(TypeId::LookupByNameFailSafe(tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
        Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TypeId::LookupByName(tcpVariant)));
    }

    /* Configure TCP Options */
    Config::SetDefault("ns3::TcpSocket::SegmentSize", UintegerValue(payloadSize));

    NodeContainer p2pNodes;
    p2pNodes.Create(2);

    PointToPointHelper pointToPoint;
    pointToPoint.SetDeviceAttribute("DataRate", StringValue("100Mbps"));
    pointToPoint.SetChannelAttribute("Delay", StringValue("30ms"));
    // pointToPoint.SetQueue("ns3::DropTailQueue");

    NetDeviceContainer p2pDevices;
    p2pDevices = pointToPoint.Install(p2pNodes);

    NodeContainer wifiStaNodes0;
    wifiStaNodes0.Create(nWifi);
    NodeContainer wifiApNode0 = p2pNodes.Get(0);

    NodeContainer wifiStaNodes1;
    wifiStaNodes1.Create(nWifi);
    NodeContainer wifiApNode1 = p2pNodes.Get(1);

    YansWifiChannelHelper channel0 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy0;
    phy0.SetChannel(channel0.Create());
    // phy0.SetErrorRateModel("ns3::YansErrorRateModel");
    phy0.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

    YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
    YansWifiPhyHelper phy1;
    phy1.SetChannel(channel1.Create());
    phy1.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
    // phy1.SetErrorRateModel("ns3::YansErrorRateModel");

    WifiHelper wifi0;
    wifi0.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiHelper wifi1;
    wifi1.SetRemoteStationManager("ns3::AarfWifiManager");

    WifiMacHelper mac0;
    WifiMacHelper mac1;
    Ssid ssid0 = Ssid("ns-3-ssid");
    mac0.SetType("ns3::StaWifiMac",
                 "Ssid", SsidValue(ssid0),
                 "ActiveProbing", BooleanValue(false));

    Ssid ssid1 = Ssid("ns-3-ssid");
    mac1.SetType("ns3::StaWifiMac",
                 "Ssid", SsidValue(ssid1),
                 "ActiveProbing", BooleanValue(false));

    NetDeviceContainer staDevices0;
    staDevices0 = wifi0.Install(phy0, mac0, wifiStaNodes0);

    NetDeviceContainer staDevices1;
    staDevices1 = wifi1.Install(phy1, mac1, wifiStaNodes1);

    mac0.SetType("ns3::ApWifiMac",
                 "Ssid", SsidValue(ssid0));

    mac1.SetType("ns3::ApWifiMac",
                 "Ssid", SsidValue(ssid1));

    NetDeviceContainer apDevices0;
    apDevices0 = wifi0.Install(phy0, mac0, wifiApNode0);

    NetDeviceContainer apDevices1;
    apDevices1 = wifi1.Install(phy1, mac1, wifiApNode1);

    MobilityHelper mobility;

    mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue(0.0),
                                  "MinY", DoubleValue(0.0),
                                  "DeltaX", DoubleValue(5.0),
                                  "DeltaY", DoubleValue(10.0),
                                  "GridWidth", UintegerValue(3),
                                  "LayoutType", StringValue("RowFirst"));

    mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
    mobility.Install(wifiStaNodes0);
    mobility.Install(wifiStaNodes1);
    mobility.Install(wifiApNode0);
    mobility.Install(wifiApNode1);

    InternetStackHelper stack;
    // stack.Install(p2pNodes);

    // Ipv4StaticRoutingHelper staticRoutingHelper;
    // stack.SetRoutingHelper (staticRoutingHelper);

    stack.Install(wifiStaNodes0);
    stack.Install(wifiApNode0);
    stack.Install(wifiStaNodes1);
    stack.Install(wifiApNode1);

    FlowMonitorHelper flowmon;
    Ptr<FlowMonitor> monitor = flowmon.InstallAll();

    Ipv4AddressHelper address;

    address.SetBase("10.1.1.0", "255.255.255.0");
    Ipv4InterfaceContainer p2pInterfaces;
    p2pInterfaces = address.Assign(p2pDevices);

    address.SetBase("10.1.2.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces0, apInterfaces0;
    wifiInterfaces0 = address.Assign(staDevices0);
    apInterfaces0 = address.Assign(apDevices0);

    address.SetBase("10.1.3.0", "255.255.255.0");
    Ipv4InterfaceContainer wifiInterfaces1, apInterfaces1;
    wifiInterfaces1 = address.Assign(staDevices1);
    apInterfaces1 = address.Assign(apDevices1);

    /* Populate routing table */
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();

    Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
    em->SetAttribute("ErrorRate", DoubleValue(0.00001));

    /* Install TCP Receiver on the access point */
    for (int i = 0; i < num_half_flow; i++)
    {

        PacketSinkHelper sinkHelper("ns3::TcpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), 9));
        ApplicationContainer sinkApp = sinkHelper.Install(wifiStaNodes0.Get(i));
        sink = StaticCast<PacketSink>(sinkApp.Get(0));

        /* Install TCP/UDP Transmitter on the station */
        OnOffHelper server("ns3::TcpSocketFactory", (InetSocketAddress(wifiInterfaces0.GetAddress(i), 9)));
        server.SetAttribute("PacketSize", UintegerValue(payloadSize));
        server.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=1]"));
        server.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=0]"));
        server.SetAttribute("DataRate", DataRateValue(DataRate(dataRate)));
        ApplicationContainer serverApp = server.Install(wifiStaNodes1.Get(i + 1));

        /* Start Applications */
        sinkApp.Start(Seconds(0.0));
        serverApp.Start(Seconds(1.0));
    }
    // Simulator::Schedule(Seconds(1.1), &CalculateThroughput);

    /* Enable Traces */
    if (pcapTracing)
    {
        phy0.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);
        phy0.EnablePcap("station0", staDevices0.Get(0));
        phy0.EnablePcap("Station1", staDevices1.Get(0));
    }

    /* Start Simulation */
    Simulator::Stop(Seconds(simulationTime + 1));
    AnimationInterface anim("wifi_tcp2.xml");
    Simulator::Run();
    int j = 0;
    float AvgThroughput = 0;
    Time Jitter;
    Time Delay;

    Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier>(flowmon.GetClassifier());
    std::map<FlowId, FlowMonitor::FlowStats> stats = monitor->GetFlowStats();

    for (std::map<FlowId, FlowMonitor::FlowStats>::const_iterator iter = stats.begin(); iter != stats.end(); ++iter)
    {
        Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(iter->first);

        NS_LOG_UNCOND("----Flow ID:" << iter->first);
        NS_LOG_UNCOND("Src Addr" << t.sourceAddress << "Dst Addr " << t.destinationAddress);
        NS_LOG_UNCOND("Sent Packets=" << iter->second.txPackets);
        NS_LOG_UNCOND("Received Packets =" << iter->second.rxPackets);
        NS_LOG_UNCOND("Lost Packets =" << iter->second.txPackets - iter->second.rxPackets);
        NS_LOG_UNCOND("Packet delivery ratio =" << iter->second.rxPackets * 100 / iter->second.txPackets << "%");
        NS_LOG_UNCOND("Packet loss ratio =" << (iter->second.txPackets - iter->second.rxPackets) * 100 / iter->second.txPackets << "%");
        NS_LOG_UNCOND("Delay =" << iter->second.delaySum);
        NS_LOG_UNCOND("Jitter =" << iter->second.jitterSum);
        NS_LOG_UNCOND("Throughput =" << iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024 << "Kbps");

        SentPackets = SentPackets + (iter->second.txPackets);
        ReceivedPackets = ReceivedPackets + (iter->second.rxPackets);
        LostPackets = LostPackets + (iter->second.txPackets - iter->second.rxPackets);
        AvgThroughput = AvgThroughput + (iter->second.rxBytes * 8.0 / (iter->second.timeLastRxPacket.GetSeconds() - iter->second.timeFirstTxPacket.GetSeconds()) / 1024);
        Delay = Delay + (iter->second.delaySum);
        Jitter = Jitter + (iter->second.jitterSum);

        j = j + 1;
    }

    AvgThroughput = AvgThroughput / j;
    NS_LOG_UNCOND("--------Total Results of the simulation----------" << std::endl);
    NS_LOG_UNCOND("Total sent packets  =" << SentPackets);
    NS_LOG_UNCOND("Total Received Packets =" << ReceivedPackets);
    NS_LOG_UNCOND("Total Lost Packets =" << LostPackets);
    NS_LOG_UNCOND("Packet Loss ratio =" << ((LostPackets * 100) / SentPackets) << "%");
    NS_LOG_UNCOND("Packet delivery ratio =" << ((ReceivedPackets * 100) / SentPackets) << "%");
    NS_LOG_UNCOND("Average Throughput =" << AvgThroughput << "Kbps");
    NS_LOG_UNCOND("End to End Delay =" << Delay);
    NS_LOG_UNCOND("End to End Jitter delay =" << Jitter);
    NS_LOG_UNCOND("Total Flow " << j);
    monitor->SerializeToXmlFile("manet-routing.xml", true, true);

    double averageThroughput = ((sink->GetTotalRx() * 8) / (1e6 * simulationTime));

    // if (averageThroughput < 50)
    // {
    //     NS_LOG_ERROR("Obtained throughput is not in the expected boundaries!");
    //     exit(1);
    // }
    std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;

    Simulator::Destroy();

    return 0;
}