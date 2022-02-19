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
#include "ns3/netanim-module.h"
#include "ns3/core-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/network-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"

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

using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("ThirdScriptExample");

int 
main (int argc, char *argv[])
{
  bool verbose = true;
//   uint32_t nCsma = 3;
  uint32_t nWifi = 3;
  bool tracing = true;

  CommandLine cmd (__FILE__);
//   cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("nWifi", "Number of wifi STA devices", nWifi);
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

  NodeContainer wifiStaNodes0;
  wifiStaNodes0.Create (nWifi);
  NodeContainer wifiApNode0 = p2pNodes.Get (0);

  NodeContainer wifiStaNodes1;
  wifiStaNodes1.Create (nWifi);
  NodeContainer wifiApNode1 = p2pNodes.Get (1);

  YansWifiChannelHelper channel = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy;
  phy.SetChannel (channel.Create ());

  YansWifiChannelHelper channel1 = YansWifiChannelHelper::Default ();
  YansWifiPhyHelper phy2;
  phy2.SetChannel (channel1.Create ());

  WifiHelper wifi;
  wifi.SetRemoteStationManager ("ns3::AarfWifiManager");

  WifiMacHelper mac0;
  WifiMacHelper mac1;
  Ssid ssid0 = Ssid ("ns-3-ssid");
  mac0.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid0),
               "ActiveProbing", BooleanValue (false));

  Ssid ssid1 = Ssid ("ns-3-ssid");
  mac1.SetType ("ns3::StaWifiMac",
               "Ssid", SsidValue (ssid1),
               "ActiveProbing", BooleanValue (false));

  NetDeviceContainer staDevices0;
  staDevices0 = wifi.Install (phy, mac0, wifiStaNodes0);

  NetDeviceContainer staDevices1;
  staDevices1 = wifi.Install (phy2, mac1, wifiStaNodes1);

  mac0.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid0));

  mac1.SetType ("ns3::ApWifiMac",
               "Ssid", SsidValue (ssid1));

  NetDeviceContainer apDevices0;
  apDevices0 = wifi.Install (phy, mac0, wifiApNode0);

  NetDeviceContainer apDevices1;
  apDevices1 = wifi.Install (phy2, mac1, wifiApNode1);

  MobilityHelper mobility;

  mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                 "MinX", DoubleValue (0.0),
                                 "MinY", DoubleValue (0.0),
                                 "DeltaX", DoubleValue (5.0),
                                 "DeltaY", DoubleValue (10.0),
                                 "GridWidth", UintegerValue (3),
                                 "LayoutType", StringValue ("RowFirst"));

  mobility.SetMobilityModel ("ns3::RandomWalk2dMobilityModel",
                             "Bounds", RectangleValue (Rectangle (-50, 50, -50, 50)));
  mobility.Install (wifiStaNodes0);
  mobility.Install (wifiStaNodes1);

  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install (wifiApNode0);
  mobility.Install (wifiApNode1);

  InternetStackHelper stack;
  stack.Install(p2pNodes);

  // Ipv4StaticRoutingHelper staticRoutingHelper;
  // stack.SetRoutingHelper (staticRoutingHelper);
// stack.Install (wifiApNode0);
  stack.Install (wifiStaNodes0);

 //stack.Install (wifiApNode1);
  stack.Install (wifiStaNodes1);

  Ipv4AddressHelper address;

  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer p2pInterfaces;
  p2pInterfaces = address.Assign (p2pDevices);

//   address.SetBase ("10.1.2.0", "255.255.255.0");
//   Ipv4InterfaceContainer csmaInterfaces;
//   csmaInterfaces = address.Assign (csmaDevices);

  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces0, apInterfaces0;
  wifiInterfaces0 = address.Assign (staDevices0);
  apInterfaces0 = address.Assign (apDevices0);

  // for (NodeContainer::Iterator iter = wifiStaNodes0.Begin (); iter != wifiStaNodes0.End (); iter++)
  // {//Tommaso suggestion
  //   Ptr<Ipv4StaticRouting> staticRouting;
  //   staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> ((*iter)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  //   staticRouting->SetDefaultRoute (apInterfaces0.GetAddress(0), 1 );
  // }

  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer wifiInterfaces1, apInterfaces1;
  wifiInterfaces1 = address.Assign (staDevices1);
  apInterfaces1 = address.Assign (apDevices1);

  // for (NodeContainer::Iterator iter = wifiStaNodes1.Begin (); iter != wifiStaNodes1.End (); iter++)
  // {//Tommaso suggestion
  //   Ptr<Ipv4StaticRouting> staticRouting;
  //   staticRouting = Ipv4RoutingHelper::GetRouting <Ipv4StaticRouting> ((*iter)->GetObject<Ipv4> ()->GetRoutingProtocol ());
  //   staticRouting->SetDefaultRoute (apInterfaces1.GetAddress(0), 1 );
  // }
  
  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (wifiStaNodes1.Get (0));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

//   UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma), 9);
//   UdpEchoClientHelper echoClient ("10.1.2.4", 9);
  UdpEchoClientHelper echoClient (wifiInterfaces1.GetAddress(0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = 
    echoClient.Install (wifiStaNodes0.Get (2));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();

  Simulator::Stop (Seconds (10.0));

  if (tracing)
    {
      phy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
      pointToPoint.EnablePcapAll ("cto");
      phy.EnablePcap ("cto", apDevices0.Get (0));
      phy2.EnablePcap ("cto2", apDevices1.Get (0));
    //   csma.EnablePcap ("third", csmaDevices.Get (0), true);
    }

  AnimationInterface anim ("wifi_topology.xml");
  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}