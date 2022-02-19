/* -*-  Mode: C++; c-file-style: "gnu"; indent-tabs-mode:nil; -*- */
/*
 * Copyright (c) 2015, IMDEA Networks Institute
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
 * Author: Hany Assasa <hany.assasa@gmail.com>
.*
 * This is a simple example to test TCP over 802.11n (with MPDU aggregation enabled).
 *

// Default Network Topology
//
//   Wifi 10.1.3.0
//                 AP
//  *    *    *    *
//  |    |    |    |    10.1.1.0
// n5   n6   n7   n0 -------------- n1   n2   n3   n4
//                   point-to-point  |    |    |    |
//                                   *    *    *    *
//                                     LAN 10.1.2.0

*/

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

NS_LOG_COMPONENT_DEFINE ("update");

using namespace ns3;

std::ofstream goodput("./lastFiles/goodput.txt");
Ptr<PacketSink> sink;                         /* Pointer to the packet sink application */
uint64_t lastTotalRx = 0;                     /* The value of the last total received bytes */

void
CalculateGoodput ()
{
  Time now = Simulator::Now ();                                         /* Return the simulator's virtual time. */
  double cur = (sink->GetTotalRx () - lastTotalRx) * (double) 8 / 1e5;     /* Convert Application RX Packets to MBits. */
  std::cout << now.GetSeconds () << "s: \t" << cur << " Mbit/s" << std::endl;
  goodput << now.GetSeconds () <<" "<< cur << std::endl;
  lastTotalRx = sink->GetTotalRx ();
  Simulator::Schedule (MilliSeconds (100), &CalculateGoodput);
}

int
main (int argc, char *argv[])
{
  uint32_t payloadSize = 1472;                       /* Transport layer payload size in bytes. */
  std::string dataRate = "100Mbps";                  /* Application layer datarate. */
  std::string tcpVariant = "TcpNewReno";             /* TCP variant type. */
  std::string phyRate = "HtMcs7";                    /* Physical layer bitrate. */
  double simulationTime = 2;                        /* Simulation time in seconds. */

  std::string redLinkDataRate = "1.5Mbps";
  std::string redLinkDelay = "20ms";
  
  uint32_t nWifi = 5;
  int no_of_flow = 5;


  /* Command line argument parser setup. */
  CommandLine cmd (__FILE__);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
  cmd.AddValue ("no_of_flow", "Number of flow", no_of_flow);

  cmd.AddValue ("payloadSize", "Payload size in bytes", payloadSize);
  cmd.AddValue ("dataRate", "Application data ate", dataRate);
  cmd.AddValue ("tcpVariant", "Transport protocol to use: TcpNewReno, "
                "TcpHybla, TcpHighSpeed, TcpHtcp, TcpVegas, TcpScalable, TcpVeno, "
                "TcpBic, TcpYeah, TcpIllinois, TcpWestwood, TcpWestwoodPlus, TcpLedbat ", tcpVariant);
  cmd.AddValue ("phyRate", "Physical layer bitrate", phyRate);
  cmd.AddValue ("simulationTime", "Simulation time in seconds", simulationTime);
  cmd.Parse (argc, argv);

  tcpVariant = std::string ("ns3::") + tcpVariant;
  // Select TCP variant
  if (tcpVariant.compare ("ns3::TcpWestwoodPlus") == 0)
    {
      // TcpWestwoodPlus is not an actual TypeId name; we need TcpWestwood here
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TcpWestwood::GetTypeId ()));
      // the default protocol type in ns3::TcpWestwood is WESTWOOD
      Config::SetDefault ("ns3::TcpWestwood::ProtocolType", EnumValue (TcpWestwood::WESTWOODPLUS));
    }
  else
    {
      TypeId tcpTid;
      NS_ABORT_MSG_UNLESS (TypeId::LookupByNameFailSafe (tcpVariant, &tcpTid), "TypeId " << tcpVariant << " not found");
      Config::SetDefault ("ns3::TcpL4Protocol::SocketType", TypeIdValue (TypeId::LookupByName (tcpVariant)));
    }

  /* Configure TCP Options */
  Config::SetDefault ("ns3::TcpSocket::SegmentSize", UintegerValue (payloadSize));

   
  NodeContainer p2pNodes;
  p2pNodes.Create (2);

  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("100Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("10ms"));
  pointToPoint.SetQueue ("ns3::DropTailQueue");

  NetDeviceContainer p2pDevices;
  p2pDevices = pointToPoint.Install (p2pNodes);

  NodeContainer wifiStaNodes_left;
  wifiStaNodes_left.Create (nWifi);

  NodeContainer wifiStaNodes_right;
  wifiStaNodes_right.Create (nWifi);

  NodeContainer wifiApNode_left = p2pNodes.Get (0);
  NodeContainer wifiApNode_right = p2pNodes.Get (1);

  YansWifiChannelHelper channel_left = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy_left;
  phy_left.SetChannel (channel_left.Create ());
  phy_left.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper channel_right = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy_right;
  phy_right.SetChannel (channel_right.Create ());
  phy_right.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);

  WifiHelper wifi_left;
  wifi_left.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiHelper wifi_right;
  wifi_right.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac_left;
  Ssid ssid_left = Ssid ("ns-3-ssid");
  mac_left.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid_left),
               "ActiveProbing", BooleanValue (false));

  WifiMacHelper mac_right;
  Ssid ssid_right = Ssid ("ns-3-ssid");
  mac_right.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid_right),
               "ActiveProbing", BooleanValue (false));


  NetDeviceContainer staDevices_left;
  staDevices_left = wifi_left.Install (phy_left, mac_left, wifiStaNodes_left);

  NetDeviceContainer staDevices_right;
  staDevices_right = wifi_right.Install (phy_right, mac_right, wifiStaNodes_right);


  mac_left.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid_left));

  NetDeviceContainer apDevices_left;
  apDevices_left = wifi_left.Install (phy_left, mac_left, wifiApNode_left);

  mac_right.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid_right));

  NetDeviceContainer apDevices_right;
  apDevices_right = wifi_right.Install (phy_right, mac_right, wifiApNode_right);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  //mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
  //                           "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  //mobility.Install (wifiStaNodes);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode_left);
  mobility.Install(wifiStaNodes_left);

  mobility.Install (wifiApNode_right);
  mobility.Install(wifiStaNodes_right);

  /* Internet stack */
  InternetStackHelper stack;
  //stack.Install (p2pNodes);
  //stack.Install (csmaNodes);
  stack.Install (wifiApNode_left);
  stack.Install (wifiStaNodes_left);

  stack.Install (wifiApNode_right);
  stack.Install (wifiStaNodes_right);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface_left;
  apInterface_left = address.Assign (apDevices_left);
  Ipv4InterfaceContainer staInterface_left;
  staInterface_left = address.Assign (staDevices_left);

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer apInterface_right;
  apInterface_right = address.Assign (apDevices_right);
  Ipv4InterfaceContainer staInterface_right;
  staInterface_right = address.Assign (staDevices_right);

  /* Populate routing table */
  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  /* Install TCP Receiver on the access point */
  Ptr<RateErrorModel> em = CreateObject<RateErrorModel> ();
  em->SetAttribute ("ErrorRate", DoubleValue (0.00001));
  for(int i = 0; i < no_of_flow; i++){
    //staDevices_right.Get (i)->SetAttribute ("ReceiveErrorModel", PointerValue (em));


    PacketSinkHelper sinkHelper ("ns3::TcpSocketFactory", InetSocketAddress (Ipv4Address::GetAny (), 9+i));
    ApplicationContainer sinkApp = sinkHelper.Install (wifiStaNodes_left.Get(i)); 
    sink = StaticCast<PacketSink> (sinkApp.Get (0));

    /* Install TCP/UDP Transmitter on the station */
    OnOffHelper server ("ns3::TcpSocketFactory", (InetSocketAddress (staInterface_left.GetAddress (i), 9+i)));
    server.SetAttribute ("PacketSize", UintegerValue (payloadSize));
    server.SetAttribute ("OnTime", StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
    server.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
    server.SetAttribute ("DataRate", DataRateValue (DataRate (dataRate)));
    //ApplicationContainer serverApp = server.Install (csmaNodes.Get(i+1)); // server node assign

    ApplicationContainer serverApp = server.Install (wifiStaNodes_right.Get(i)); // server node assign

    /* Start Applications */
    sinkApp.Start (Seconds (0.0));
    serverApp.Start (Seconds (1.0));
  }

  Simulator::Schedule (Seconds (1.1), &CalculateGoodput);

  /* Flow Monitor */
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor=flowHelper.InstallAll();

  flowMonitor->CheckForLostPackets ();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier ());
  std::map<FlowId, FlowMonitor::FlowStats> stats = flowMonitor->GetFlowStats ();

  /* Start Simulation */
  Simulator::Stop (Seconds (simulationTime + 1));
  Simulator::Run ();

   /* Flow Monitor File  */
  flowMonitor->SerializeToXmlFile("./lastFiles/history.flowmonitor",false,false);

  //See implementation of goodput here: https://www.nsnam.org/doxygen/traffic-control_8cc_source.html
  //delaySum: the sum of all end-to-end delays for all received packets of the flow
  //Fairness: https://github.com/urstrulymahesh/Networks-Simulator-ns3-
  //Queuing Policy: https://www.nsnam.org/docs/release/3.14/models/html/queue.html

  double averageGoodput = ((sink->GetTotalRx () * 8) / (1e6 * simulationTime));

  std::cout << "Average Goodput: "<<averageGoodput<<"Mbit/s" <<std::endl;
   

  Simulator::Destroy ();

  return 0;
}
