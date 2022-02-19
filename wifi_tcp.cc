/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
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
 */

#include <fstream>
#include <string>
#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/stats-module.h"
#include "ns3/netanim-module.h"
#include "ns3/mobility-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/on-off-helper.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor-module.h"

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("SeventhScriptExample");

// ===========================================================================
// Default Network Topology
// We are chaning this
//
//   Wifi 10.1.2.0
//                 AP               AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0    1    2    3    4
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
// 1    2    3    4 point-to-point  |    |    |    |
//                                   *    *    *    *
//                                     Wifi 10.1.3.0
//
//
// We want to look at changes in the ns-3 TCP congestion window.  We need
// to crank up a flow and hook the CongestionWindow attribute on the socket
// of the sender.  Normally one would use an on-off application to generate a
// flow, but this has a couple of problems.  First, the socket of the on-off
// application is not created until Application Start time, so we wouldn't be
// able to hook the socket (now) at configuration time.  Second, even if we
// could arrange a call after start time, the socket is not public so we
// couldn't get at it.
//
// So, we can cook up a simple version of the on-off application that does what
// we want.  On the plus side we don't need all of the complexity of the on-off
// application.  On the minus side, we don't have a helper, so we have to get
// a little more involved in the details, but this is trivial.
//
// So first, we create a socket and do the trace connect on it; then we pass
// this socket into the constructor of our simple application which we then
// install in the source node.
//
// NOTE: If this example gets modified, do not forget to update the .png figure
// in src/stats/docs/seventh-packet-byte-count.png
// ===========================================================================
//
class MyApp : public Application
{
public:
  MyApp();
  virtual ~MyApp();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId(void);
  void Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication(void);
  virtual void StopApplication(void);

  void ScheduleTx(void);
  void SendPacket(void);

  Ptr<Socket> m_socket;
  Address m_peer;
  uint32_t m_packetSize;
  uint32_t m_nPackets;
  DataRate m_dataRate;
  EventId m_sendEvent;
  bool m_running;
  uint32_t m_packetsSent;
};

MyApp::MyApp()
    : m_socket(0),
      m_peer(),
      m_packetSize(0),
      m_nPackets(0),
      m_dataRate(0),
      m_sendEvent(),
      m_running(false),
      m_packetsSent(0)
{
}

MyApp::~MyApp()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId(void)
{
  static TypeId tid = TypeId("MyApp")
                          .SetParent<Application>()
                          .SetGroupName("Tutorial")
                          .AddConstructor<MyApp>();
  return tid;
}

void MyApp::Setup(Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void MyApp::StartApplication(void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType(m_peer))
  {
    m_socket->Bind();
  }
  else
  {
    m_socket->Bind6();
  }
  m_socket->Connect(m_peer);
  SendPacket();
}

void MyApp::StopApplication(void)
{
  m_running = false;

  if (m_sendEvent.IsRunning())
  {
    Simulator::Cancel(m_sendEvent);
  }

  if (m_socket)
  {
    m_socket->Close();
  }
}

void MyApp::SendPacket(void)
{
  Ptr<Packet> packet = Create<Packet>(m_packetSize);
  m_socket->Send(packet);
  Time now = Simulator::Now();
  if (now.GetSeconds() < 20)
  {
    ScheduleTx();
  }
}

void MyApp::ScheduleTx(void)
{
  if (m_running)
  {
    Time tNext(Seconds(m_packetSize * 8 / static_cast<double>(m_dataRate.GetBitRate())));
    m_sendEvent = Simulator::Schedule(tNext, &MyApp::SendPacket, this);
  }
}

// static void
// CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
// {
//    NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd);
//   *stream->GetStream() << Simulator::Now().GetSeconds() << " " << oldCwnd << " " << newCwnd<< std::endl;
// }

ApplicationContainer sinks;      /* Pointer to the packet sink application */
uint64_t lastTotalRx[100]; /* The value of the last total received bytes */

Ptr<OutputStreamWrapper> throughputStream[100];

void CalculateThroughput()
{
  for (uint32_t i = 0; i < sinks.GetN(); i++)
  {
    Ptr<PacketSink> sink;
    sink = StaticCast<PacketSink> (sinks.Get(i));
    Time now = Simulator::Now();                                        /* Return the simulator's virtual time. */
    double cur = (sink->GetTotalRx() - lastTotalRx[i]) * (double)8 / 1024; /* Convert Application RX Packets to KBits. */
    if(cur != 0)
    *throughputStream[i]->GetStream() << Simulator::Now().GetSeconds() << "		" << cur << std::endl;
     std::cout << now.GetSeconds() << "s: \t" << cur << " Kbit/s" << std::endl;
    lastTotalRx[i] = sink->GetTotalRx();
  }

  Simulator::Schedule(MilliSeconds(500), &CalculateThroughput);
}

int main(int argc, char *argv[])
{
  // Config::SetDefault ("ns3::TcpL4Protocol::SocketType", StringValue ("ns3::TcpNewAIMD"));
  //  bool useV6 = false;
  bool verbose = true;

  CommandLine cmd(__FILE__);
  // cmd.AddValue("useIpv6", "Use Ipv6", useV6);
  cmd.Parse(argc, argv);

  uint32_t txArea = 5;
  uint32_t nWifi = 8;
  uint32_t pps = 500;
  uint32_t p_size = 128;
  std::string dataRate = std::to_string((8 * pps * p_size) / 1024) + "kbps";
  uint32_t SentPackets = 0;
  uint32_t ReceivedPackets = 0;
  uint32_t LostPackets = 0;
  int num_half_flows = 8;
  AsciiTraceHelper graph;
  for(int i = 0; i < num_half_flows; i++){
	  std::string num = std::to_string(i);
	  throughputStream[i] = graph.CreateFileStream("throughput" + num);
  }

  if (nWifi > 18)
  {
    std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
    return 1;
  }

  if (verbose)
  {
    LogComponentEnable("UdpEchoClientApplication", LOG_LEVEL_INFO);
    LogComponentEnable("UdpEchoServerApplication", LOG_LEVEL_INFO);
  }

  NodeContainer p2pNodes;
  p2pNodes.Create(2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute("DataRate", StringValue("5Mbps"));
  pointToPoint.SetChannelAttribute("Delay", StringValue("2ms"));

  // NetDeviceContainer devices;
  // devices = pointToPoint.Install (nodes);

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install(p2pNodes);

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  em->SetAttribute("ErrorRate", DoubleValue(0.00001));
  p2pDevices.Get(0)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  NodeContainer wifiStaNodes0;
  wifiStaNodes0.Create(nWifi);
  NodeContainer wifiApNode0 = p2pNodes.Get(0);

  NodeContainer wifiStaNodes1;
  wifiStaNodes1.Create(nWifi);
  NodeContainer wifiApNode1 = p2pNodes.Get(1);

  YansWifiChannelHelper channel0 = YansWifiChannelHelper::Default();
  channel0.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(10.0 * txArea));
  YansWifiPhyHelper phy0;
  phy0.SetChannel(channel0.Create());
  // phy0.SetErrorRateModel("ns3::YansErrorRateModel");

  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
  channel1.AddPropagationLoss("ns3::RangePropagationLossModel", "MaxRange", DoubleValue(10.0 * txArea));
  YansWifiPhyHelper phy1;
  phy1.SetChannel(channel1.Create());
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
  stack.Install(p2pNodes);

  stack.Install(wifiStaNodes0);
  stack.Install(wifiStaNodes1);

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();

  uint16_t sinkPort = 8080;
  Address sinkAddress;
  Address anyAddress;

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

  Ipv4GlobalRoutingHelper::PopulateRoutingTables();

  for (int i = 0; i < num_half_flows; i++)
  {

    sinkAddress = InetSocketAddress(wifiInterfaces0.GetAddress(i), sinkPort);
    anyAddress = InetSocketAddress(Ipv4Address::GetAny(), sinkPort);

    PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", anyAddress);
    ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNodes0.Get(i));

    sinks.Add(StaticCast<PacketSink> (sinkApps.Get (0)));
    sinkApps.Start(Seconds(0.));
    sinkApps.Stop(Seconds(10.));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(wifiStaNodes1.Get(i), TcpSocketFactory::GetTypeId());

    Ptr<MyApp> app = CreateObject<MyApp>();
    app->Setup(ns3TcpSocket, sinkAddress, p_size, 1000, DataRate(dataRate)); // packets per second = (1040*2000)/10^6
    wifiStaNodes1.Get(i)->AddApplication(app);
    app->SetStartTime(Seconds(1.));
    app->SetStopTime(Seconds(11.));

    Simulator::Schedule(Seconds(1.1), &CalculateThroughput);
  }

  Simulator::Stop(Seconds(10));

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
  // monitor->SerializeToXmlFile("wifi_tcp.xml", true, true);
  Simulator::Destroy();

  return 0;
}
