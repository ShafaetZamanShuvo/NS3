
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
#include "ns3/network-module.h"
#include "ns3/csma-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/ipv4-global-routing-helper.h"

//       n0 ---+      +--- n2
//             |      |
//             n4 -- n5
//             |      |
//       n1 ---+      +--- n3
//             |      |
//       n6 ---+      +--- n7
//             |      |
//       n8 ---+      +--- n9


using namespace ns3;

NS_LOG_COMPONENT_DEFINE ("SecondScriptExample");

int 
main (int argc, char *argv[])
{
  bool verbose = true;
  uint32_t nCsma = 10;

  CommandLine cmd (__FILE__);
  cmd.AddValue ("nCsma", "Number of \"extra\" CSMA nodes/devices", nCsma);
  cmd.AddValue ("verbose", "Tell echo applications to log if true", verbose);

  cmd.Parse (argc,argv);

  if (verbose)
    {
      LogComponentEnable ("UdpEchoClientApplication", LOG_LEVEL_INFO);
      LogComponentEnable ("UdpEchoServerApplication", LOG_LEVEL_INFO);
    }

  nCsma = nCsma == 0 ? 1 : nCsma;
  NS_LOG_INFO("Create Nodes");
  NodeContainer p2pNodes;
  p2pNodes.Create (nCsma);

  NodeContainer n0n4 = NodeContainer (p2pNodes.Get (0), p2pNodes.Get(4));
  NodeContainer n1n4 = NodeContainer (p2pNodes.Get (1), p2pNodes.Get(4));
  NodeContainer n6n4 = NodeContainer (p2pNodes.Get (6), p2pNodes.Get(4));
  NodeContainer n8n4 = NodeContainer (p2pNodes.Get (8), p2pNodes.Get(4));

  NodeContainer n4n5 = NodeContainer (p2pNodes.Get (4), p2pNodes.Get(5));

  NodeContainer n2n5 = NodeContainer (p2pNodes.Get (2), p2pNodes.Get(5));
  NodeContainer n3n5 = NodeContainer (p2pNodes.Get (3), p2pNodes.Get(5));
  NodeContainer n7n5 = NodeContainer (p2pNodes.Get (7), p2pNodes.Get(5));
  NodeContainer n9n5 = NodeContainer (p2pNodes.Get (9), p2pNodes.Get(5));
  
 

  InternetStackHelper stack;
  stack.Install (p2pNodes);


  PointToPointHelper pointToPoint;
  pointToPoint.SetDeviceAttribute ("DataRate", StringValue ("5Mbps"));
  pointToPoint.SetChannelAttribute ("Delay", StringValue ("30ms"));

  NetDeviceContainer d0d4 = pointToPoint.Install (n0n4);
  NetDeviceContainer d1d4 = pointToPoint.Install (n1n4);
  NetDeviceContainer d6d4 = pointToPoint.Install (n6n4);
  NetDeviceContainer d8d4 = pointToPoint.Install (n8n4);

  NetDeviceContainer d4d5 = pointToPoint.Install (n4n5);

  NetDeviceContainer d2d5 = pointToPoint.Install (n2n5);
  NetDeviceContainer d3d5 = pointToPoint.Install (n3n5);
  NetDeviceContainer d7d5 = pointToPoint.Install (n7n5);
  NetDeviceContainer d9d5 = pointToPoint.Install (n9n5);


 


  Ipv4AddressHelper address;
  // left side
  address.SetBase ("10.1.1.0", "255.255.255.0");
  Ipv4InterfaceContainer i0i4 = address.Assign (d0d4);
  address.SetBase ("10.1.2.0", "255.255.255.0");
  Ipv4InterfaceContainer i1i4 = address.Assign (d1d4); 
  address.SetBase ("10.1.6.0", "255.255.255.0");
  Ipv4InterfaceContainer i6i4 = address.Assign (d6d4);
  address.SetBase ("10.1.8.0", "255.255.255.0");
  Ipv4InterfaceContainer i8i4 = address.Assign (d8d4);
  //majher ta
  address.SetBase ("10.1.3.0", "255.255.255.0");
  Ipv4InterfaceContainer i4i5 = address.Assign (d4d5);
  //right side
  address.SetBase ("10.1.4.0", "255.255.255.0");
  Ipv4InterfaceContainer i2i5 = address.Assign (d2d5);
  address.SetBase ("10.1.5.0", "255.255.255.0");
  Ipv4InterfaceContainer i3i5 = address.Assign (d3d5);
  address.SetBase ("10.1.7.0", "255.255.255.0");
  Ipv4InterfaceContainer i7i5 = address.Assign (d7d5);
  address.SetBase ("10.1.9.0", "255.255.255.0");
  Ipv4InterfaceContainer i9i5 = address.Assign (d9d5);
  

  Ipv4GlobalRoutingHelper::PopulateRoutingTables ();
  
  

  UdpEchoServerHelper echoServer (9);

  ApplicationContainer serverApps = echoServer.Install (p2pNodes.Get(9));
  serverApps.Start (Seconds (1.0));
  serverApps.Stop (Seconds (10.0));

  UdpEchoClientHelper echoClient (i9i5.GetAddress(0), 9);
  echoClient.SetAttribute ("MaxPackets", UintegerValue (1));
  echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
  echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

  ApplicationContainer clientApps = echoClient.Install (p2pNodes.Get (8));
  clientApps.Start (Seconds (2.0));
  clientApps.Stop (Seconds (10.0));

//   int num_half_flows = 3;
//   for(int i = 0; i < num_half_flows; i++) {
//     UdpEchoServerHelper echoServer (9);

//     ApplicationContainer serverApps = echoServer.Install (csmaNodes.Get (nCsma - i));   // dynamically choose a server node
//     serverApps.Start (Seconds (1.0));
//     serverApps.Stop (Seconds (10.0));

//     UdpEchoClientHelper echoClient (csmaInterfaces.GetAddress (nCsma - i), 9);         // telling client about server app's (ip, port)
//     echoClient.SetAttribute ("MaxPackets", UintegerValue (10));
//     echoClient.SetAttribute ("Interval", TimeValue (Seconds (1.0)));
//     echoClient.SetAttribute ("PacketSize", UintegerValue (1024));

//     ApplicationContainer clientApps = echoClient.Install (csmaNodes2.Get (nCsma - i));  // dynamically choose a client node
//     clientApps.Start (Seconds (2.0));
//     clientApps.Stop (Seconds (10.0));
//   }


  

  pointToPoint.EnablePcapAll ("second");
  //pointToPoint.EnablePcap ("second", p2pNodes.Get (0));

  Simulator::Run ();
  Simulator::Destroy ();
  return 0;
}
