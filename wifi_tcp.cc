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

  if (++m_packetsSent < m_nPackets)
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

static void
CwndChange(Ptr<OutputStreamWrapper> stream, uint32_t oldCwnd, uint32_t newCwnd)
{
  NS_LOG_UNCOND(Simulator::Now().GetSeconds() << "\t" << oldCwnd<< "\t" << newCwnd);
  *stream->GetStream() << Simulator::Now().GetSeconds() << "\t" << oldCwnd << "\t" << newCwnd << std::endl;
}

// static void
// RxDrop(Ptr<PcapFileWrapper> file, Ptr<const Packet> p)
// {
//   NS_LOG_UNCOND("RxDrop at " << Simulator::Now().GetSeconds());
//   file->Write(Simulator::Now(), p);
// }

int main(int argc, char *argv[])
{
  // bool useV6 = false;
  bool verbose = true;

  CommandLine cmd(__FILE__);
  // cmd.AddValue("useIpv6", "Use Ipv6", useV6);
  cmd.Parse(argc, argv);

  uint32_t nWifi = 4;

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

  // Ptr<RateErrorModel> em = CreateObject<RateErrorModel>();
  // em->SetAttribute("ErrorRate", DoubleValue(0.00001));
  p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel", PointerValue(em));

  // Ptr<RateErrorModel> em1 = CreateObject<RateErrorModel>();
  // em1->SetAttribute("ErrorRate", DoubleValue(0.00001));
  // p2pDevices.Get(1)->SetAttribute("ReceiveErrorModel1", PointerValue(em1));

  NodeContainer wifiStaNodes0;
  wifiStaNodes0.Create(nWifi);
  NodeContainer wifiApNode0 = p2pNodes.Get(0);

  NodeContainer wifiStaNodes1;
  wifiStaNodes1.Create(nWifi);
  NodeContainer wifiApNode1 = p2pNodes.Get(1);

  YansWifiChannelHelper channel0 = YansWifiChannelHelper::Default();
  YansWifiPhyHelper phy0;
  phy0.SetChannel(channel0.Create());
  //phy0.SetErrorRateModel("ns3::YansErrorRateModel");

  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default();
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

  // mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
  //"Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiStaNodes0);
  mobility.Install(wifiStaNodes1);

  // mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(wifiApNode0);
  mobility.Install(wifiApNode1);

  InternetStackHelper stack;
  stack.Install(p2pNodes);

  // Ipv4StaticRoutingHelper staticRoutingHelper;
  // stack.SetRoutingHelper (staticRoutingHelper);

  stack.Install(wifiStaNodes0);
  stack.Install(wifiStaNodes1);

  uint16_t sinkPort = 8080;
  Address sinkAddress;
  Address anyAddress;
  std::string probeType;
  std::string tracePath;

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

  int num_half_flows = 1;
  for (int i = 0; i < num_half_flows; i++){

    sinkAddress = InetSocketAddress(wifiInterfaces0.GetAddress(i), sinkPort);
  anyAddress = InetSocketAddress(Ipv4Address::GetAny(), sinkPort);
  probeType = "ns3::Ipv4PacketProbe";
  tracePath = "/NodeList/*/$ns3::Ipv4L3Protocol/Tx";

  PacketSinkHelper packetSinkHelper("ns3::TcpSocketFactory", anyAddress);
  ApplicationContainer sinkApps = packetSinkHelper.Install(wifiStaNodes0.Get(i));
  sinkApps.Start(Seconds(0.));
  sinkApps.Stop(Seconds(20.));

  Ptr<Socket> ns3TcpSocket = Socket::CreateSocket(wifiStaNodes1.Get(i), TcpSocketFactory::GetTypeId());

  Ptr<MyApp> app = CreateObject<MyApp>();
  app->Setup(ns3TcpSocket, sinkAddress, 1040, 1000, DataRate("1Mbps"));
  wifiStaNodes1.Get(0)->AddApplication(app);
  app->SetStartTime(Seconds(1.));
  app->SetStopTime(Seconds(20.));

  AsciiTraceHelper asciiTraceHelper;
  Ptr<OutputStreamWrapper> stream = asciiTraceHelper.CreateFileStream("seventh.cwnd");
  ns3TcpSocket->TraceConnectWithoutContext("CongestionWindow", MakeBoundCallback(&CwndChange, stream));

 

  }

  //  PcapHelper pcapHelper;
  // Ptr<PcapFileWrapper> file0 = pcapHelper.CreateFile("seventh0.pcap", std::ios::out, PcapHelper::DLT_PPP);
  // p2pDevices.Get(0)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file0));
  //  Ptr<PcapFileWrapper> file1 = pcapHelper.CreateFile("seventh1.pcap", std::ios::out, PcapHelper::DLT_PPP);
  // p2pDevices.Get(1)->TraceConnectWithoutContext("PhyRxDrop", MakeBoundCallback(&RxDrop, file1));
  
  // // Use GnuplotHelper to plot the packet byte count over time
  // GnuplotHelper plotHelper;

  // // Configure the plot.  The first argument is the file name prefix
  // // for the output files generated.  The second, third, and fourth
  // // arguments are, respectively, the plot title, x-axis, and y-axis labels
  // plotHelper.ConfigurePlot("seventh-packet-byte-count",
  //                          "Packet Byte Count vs. Time",
  //                          "Time (Seconds)",
  //                          "Packet Byte Count");

  // // Specify the probe type, trace source path (in configuration namespace), and
  // // probe output trace source ("OutputBytes") to plot.  The fourth argument
  // // specifies the name of the data series label on the plot.  The last
  // // argument formats the plot by specifying where the key should be placed.
  // plotHelper.PlotProbe(probeType,
  //                      tracePath,
  //                      "OutputBytes",
  //                      "Packet Byte Count",
  //                      GnuplotAggregator::KEY_BELOW);

  // // Use FileHelper to write out the packet byte count over time
  // FileHelper fileHelper;

  // // Configure the file to be written, and the formatting of output data.
  // fileHelper.ConfigureFile("seventh-packet-byte-count",
  //                          FileAggregator::FORMATTED);

  // // Set the labels for this formatted output file.
  // fileHelper.Set2dFormat("Time (Seconds) = %.3e\tPacket Byte Count = %.0f");

  // // Specify the probe type, trace source path (in configuration namespace), and
  // // probe output trace source ("OutputBytes") to write.
  // fileHelper.WriteProbe(probeType,
  //                       tracePath,
  //                       "OutputBytes");
  
  Simulator::Stop(Seconds(20));
  Simulator::Run();
  Simulator::Destroy();

  return 0;
}
