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

#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/flow-monitor-module.h"

// Default Network Topology
//
//   Wifi 10.1.2.0
//                   AP				   AP
//  *    *    *  *   *
//  |    |    |  |   |    10.1.1.0	   
// n2   n3   n4  n5  n0 -------------- n1   n6   n7   n8  n9
//                   point-to-point    |    |    |    |   |
//                                     *    *    *    *   *
//                                       Wifi 10.1.3.0

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

class MyApp : public Application
{
public:
  MyApp ();
  virtual ~MyApp ();

  /**
   * Register this type.
   * \return The TypeId.
   */
  static TypeId GetTypeId (void);
  void Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate);

private:
  virtual void StartApplication (void);
  virtual void StopApplication (void);

  void ScheduleTx (void);
  void SendPacket (void);

  Ptr<Socket>     m_socket;
  Address         m_peer;
  uint32_t        m_packetSize;
  uint32_t        m_nPackets;
  DataRate        m_dataRate;
  EventId         m_sendEvent;
  bool            m_running;
  uint32_t        m_packetsSent;
};

MyApp::MyApp ()
  : m_socket (0),
    m_peer (),
    m_packetSize (0),
    m_nPackets (0),
    m_dataRate (0),
    m_sendEvent (),
    m_running (false),
    m_packetsSent (0)
{
}

MyApp::~MyApp ()
{
  m_socket = 0;
}

/* static */
TypeId MyApp::GetTypeId (void)
{
  static TypeId tid = TypeId ("MyApp")
    .SetParent<Application> ()
    .SetGroupName ("Tutorial")
    .AddConstructor<MyApp> ()
    ;
  return tid;
}

void
MyApp::Setup (Ptr<Socket> socket, Address address, uint32_t packetSize, uint32_t nPackets, DataRate dataRate)
{
  m_socket = socket;
  m_peer = address;
  m_packetSize = packetSize;
  m_nPackets = nPackets;
  m_dataRate = dataRate;
}

void
MyApp::StartApplication (void)
{
  m_running = true;
  m_packetsSent = 0;
  if (InetSocketAddress::IsMatchingType (m_peer))
    {
      m_socket->Bind ();
    }
  else
    {
      m_socket->Bind6 ();
    }
  m_socket->Connect (m_peer);
  SendPacket ();
}

void
MyApp::StopApplication (void)
{
  m_running = false;

  if (m_sendEvent.IsRunning ())
    {
      Simulator::Cancel (m_sendEvent);
    }

  if (m_socket)
    {
      m_socket->Close ();
    }
}

void
MyApp::SendPacket (void)
{
  Ptr<Packet> packet = Create<Packet> (m_packetSize);
  m_socket->Send (packet);

  if (++m_packetsSent < m_nPackets)
    {
      ScheduleTx ();
    }
}

void
MyApp::ScheduleTx (void)
{
  if (m_running)
    {
      Time tNext (Seconds (m_packetSize * 8 / static_cast<double> (m_dataRate.GetBitRate ())));
      m_sendEvent = Simulator::Schedule (tNext, &MyApp::SendPacket, this);
    }
}

static void
CwndChange (Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
//   NS_LOG_UNCOND (Simulator::Now ().GetSeconds () << "\t" << newCwnd);
  *stream->GetStream () << Simulator::Now ().GetSeconds () << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

// static void
// RxDrop (Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
// {
//   NS_LOG_UNCOND ("RxDrop at " << Simulator::Now ().GetSeconds ());
//   file->Write (Simulator::Now (), p);
// }

uint64_t no_of_TCP_flows = 3;

ApplicationContainer sink_all;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx[3] = {0,0,0};                     /* The value of the last total received bytes */

AsciiTraceHelper graphascii;
Ptr<OutputStreamWrapper> throughputstream[3];

void
CalculateThroughput ()
{
	uint32_t sink_no = sink_all.GetN();
	for(uint32_t i = 0; i < sink_no; i++){
		Ptr<PacketSink> sink = StaticCast<PacketSink> (sink_all.Get(i));
		Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
		double cur = (sink->GetTotalRx () - lastTotalRx[i]) * (double) 8 / 1e5;     /* Convert Application RX Packets to MBits. */
		*throughputstream[i]->GetStream() << Simulator::Now().GetSeconds() << "		" << cur << std::endl;
		//std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
		lastTotalRx[i] = sink->GetTotalRx ();
	}
  
  Simulator::Schedule (MilliSeconds (100), &CalculateThroughput);
}

int 
main (int argc, char *argv[])
{
  Config::SetDefault("ns3::TcpL4Protocol::SocketType", TypeIdValue(TcpNewReno::GetTypeId()));

  for(uint32_t i = 0; i < no_of_TCP_flows; i++){
	  std::string temp = std::to_string(i);
	  throughputstream[i] = graphascii.CreateFileStream("throughput_graph_plot_" + temp);
  }


  bool verbose = true;
  uint32_t nWifi = 7;
  bool tracing = false;
  double simulationTime = 10;  

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifi", "Number of wifi STA devices1", nWifi);
  cmd.AddValue ("nWifi", "Number of wifi STA devices2", nWifi);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);
  cmd.AddValue ("tracing", "Enable pcap tracing", tracing);

  cmd.Parse (argc,argv);

  // The underlying restriction of 18 is due to the grid position
  // allocator's configuration; the grid layout will exceed the
  // bounding box if more than 18 nodes are provided.
  if (nWifi > 18)
    {
      std::cout << "nWifi should be 18 or less; otherwise grid layout exceeds the bounding box" << std::endl;
      return 1;
    }

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("2ms"));

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  // error model
  // Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  // em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  // p2pDevices.Get (1)->SetAttribute ("ReceiveErrorModel", PointerValue (em));

  NodeContainer wifiStaNodes1;
  wifiStaNodes1.Create (nWifi);
  NodeContainer wifiApNode1 = p2pNodes.Get (0);

  NodeContainer wifiStaNodes2;
  wifiStaNodes2.Create (nWifi);
  NodeContainer wifiApNode2 = p2pNodes.Get (1);

  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy1;
  phy1.SetChannel (channel1.Create ());

  YansWifiChannelHelper channel2 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy2;
  phy2.SetErrorRateModel("ns3::NistErrorRateModel");
  phy2.SetChannel (channel2.Create ());

  WifiHelper wifi1;
  wifi1.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiHelper wifi2;
  wifi2.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac1;
  Ssid ssid1 = Ssid ("ns-3-ssid");
  mac1.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));

  WifiMacHelper mac2;
  Ssid ssid2 = Ssid ("ns-3-ssid");
  mac2.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid2),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices1;
  staDevices1 = wifi1.Install (phy1, mac1, wifiStaNodes1);

  NetDeviceContainer staDevices2;
  staDevices2 = wifi2.Install (phy2, mac2, wifiStaNodes2);

  mac1.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));

  mac2.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid2));

  NetDeviceContainer apDevices1;
  apDevices1 = wifi1.Install (phy1, mac1, wifiApNode1);

  NetDeviceContainer apDevices2;
  apDevices2 = wifi2.Install (phy2, mac2, wifiApNode2);

  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (.001));

  //after wifi netdevices are created
  Config::Set("/NodeList/1/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PostReceptionErrorModel", PointerValue(em));
  Config::Set("/NodeList/6/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PostReceptionErrorModel", PointerValue(em));
  Config::Set("/NodeList/7/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PostReceptionErrorModel", PointerValue(em));
  Config::Set("/NodeList/8/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PostReceptionErrorModel", PointerValue(em));
  Config::Set("/NodeList/9/DeviceList/0/$ns3::WifiNetDevice/Phy/$ns3::YansWifiPhy/PostReceptionErrorModel", PointerValue(em));


  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (0.5),
                                 "DeltaY", DoubleValue (1.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  mobility.Install (wifiStaNodes1);
  mobility.Install (wifiStaNodes2);

  
  mobility.Install (wifiApNode1);
  mobility.Install (wifiApNode2);

  InternetStackHelper stack;
  stack.Install (wifiApNode1);
  stack.Install (wifiApNode2);
  stack.Install (wifiStaNodes1);
  stack.Install (wifiStaNodes2);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces1 = address.Assign (staDevices1);
  Ipv4InterfaceContainer apInterfaces1 = address.Assign (apDevices1);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer staInterfaces2 = address.Assign (staDevices2);
  Ipv4InterfaceContainer apInterfaces2 = address.Assign (apDevices2);

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  uint16_t sinkPort = 8080;

  // flow
  for(uint64_t i = 0; i < no_of_TCP_flows; i++) {
 
    Address sinkAddress(InetSocketAddress(staInterfaces2.GetAddress(i), sinkPort));
    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), sinkPort));
    ApplicationContainer sinkApps = sinkHelper.Install (wifiStaNodes2.Get(i));

    sink_all.Add(StaticCast<PacketSink> (sinkApps.Get (0)));

    sinkApps.Start (Seconds (0.));
    sinkApps.Stop (Seconds (simulationTime));

    Ptr<Socket> ns3TcpSocket = Socket::CreateSocket (wifiStaNodes1.Get(i), TcpSocketFactory::GetTypeId ());

    Ptr<MyApp> app = CreateObject<MyApp> ();
    app->Setup (ns3TcpSocket, sinkAddress, 1040, 1000, DataRate ("1Mbps"));
    wifiStaNodes1.Get(i)->AddApplication (app);
    app->SetStartTime (Seconds (1.));
    if(i == 0 || i == 2){
      app->SetStopTime (Seconds (simulationTime));
    }
    else
      app->SetStopTime (Seconds (simulationTime));
    
    Simulator::Schedule (Seconds (1.1), &CalculateThroughput);
    if(i == 0 || i == 2){
      Simulator::Stop (Seconds (simulationTime));
    }
    else
      Simulator::Stop (Seconds (simulationTime));
    
    //-------- trace ------ //

    AsciiTraceHelper asciiTraceHelper;
    Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream ("customnet_" + std::to_string(i) + ".cwnd");
    ns3TcpSocket->TraceConnectWithoutContext ("CongestionWindow", MakeBoundCallback (&CwndChange, stream));

  }

  FlowMonitorHelper flowmon;
  Ptr<FlowMonitor> monitor = flowmon.InstallAll();
    Simulator::Run ();
    //double averageThroughput = ((sink->GetTotalRx () * 8) / (1e6 * simulationTime));

  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowmon.GetClassifier() );
  FlowMonitor::FlowStatsContainer stats = monitor->GetFlowStats();


  for(auto iter = stats.begin(); iter != stats.end(); ++iter){
	  Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow (iter->first);
	  NS_LOG_UNCOND("__Flow ID: " << iter->first);
	  NS_LOG_UNCOND("src addr: " << t.sourceAddress << "-- dest addr: " << t.destinationAddress);
	  NS_LOG_UNCOND("Sent Packets=" <<iter->second. txPackets);
	  NS_LOG_UNCOND ("Received Patkets =" <<iter->second. rxPackets);
    NS_LOG_UNCOND ("Lost Packets=" <<iter->second. txPackets-iter->second. rxPackets);
    NS_LOG_UNCOND("Packet delivery ratio = " <<iter->second.rxPackets*100.0/iter->second.txPackets << "%");
	  NS_LOG_UNCOND ("Packet loss ratio =" << (iter->second.txPackets-iter->second.rxPackets)*100.0/iter->second. txPackets << "%");
    NS_LOG_UNCOND ("Delay =" <<iter->second.delaySum);
    NS_LOG_UNCOND ("Jitter =" <<iter->second. jitterSum);
    NS_LOG_UNCOND ("Throughput =" <<iter->second.rxBytes * 8.0/(iter->second.timeLastRxPacket.GetSeconds()-iter->second.timeFirstTxPacket.GetSeconds()));
  }

  Simulator::Destroy ();
  //std::cout << "\nAverage throughput: " << averageThroughput << " Mbit/s" << std::endl;
  return 0;
}
