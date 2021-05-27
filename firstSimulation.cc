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
  *
  */
 
 //
 // This ns-3 example demonstrates the use of helper functions to ease
 // the construction of simulation scenarios.
 //
 // The simulation topology consists of a mixed wired and wireless
 // scenario in which a hierarchical mobility model is used.
 //
 // The simulation layout consists of N backbone routers interconnected
 // by an ad hoc wifi network.
 // Each backbone router also has a local 802.11 network and is connected
 // to a local LAN.  An additional set of (K-1) nodes are connected to
 // this backbone.  Finally, a local LAN is connected to each router
 // on the backbone, with L-1 additional hosts.
 //
 // The nodes are populated with TCP/IP stacks, and OLSR unicast routing
 // on the backbone.  An example UDP transfer is shown.  The simulator
 // be configured to output tcpdumps or traces from different nodes.
 //
 //
 //          +--------------------------------------------------------+
 //          |                                                        |
 //          |              802.11 ad hoc, ns-2 mobility              |
 //          |                                                        |
 //          +--------------------------------------------------------+
 //                   |       o o o (N backbone routers)       |
 //               +--------+                               +--------+
 //     wired LAN | mobile |                     wired LAN | mobile |
 //    -----------| router |                    -----------| router |
 //               ---------                                ---------
 //                   |                                        |
 //          +----------------+                       +----------------+
 //          |     802.11     |                       |     802.11     |
 //          |   infra net    |                       |   infra net    |
 //          |   K-1 hosts    |                       |   K-1 hosts    |
 //          +----------------+                       +----------------+
 //
 // We'll send data from the first wired LAN node on the first wired LAN
 // to the last wireless STA on the last infrastructure net, thereby
 // causing packets to traverse CSMA to adhoc to infrastructure links
 //
 // Note that certain mobility patterns may cause packet forwarding
 // to fail (if nodes become disconnected)
 
 #include "ns3/command-line.h"
 #include "ns3/string.h"
 #include "ns3/yans-wifi-helper.h"
 #include "ns3/ssid.h"
 #include "ns3/mobility-helper.h"
 #include "ns3/internet-stack-helper.h"
 #include "ns3/ipv4-address-helper.h"
 #include "ns3/on-off-helper.h"
 #include "ns3/yans-wifi-channel.h"
 #include "ns3/qos-txop.h"
 #include "ns3/packet-sink-helper.h"
 #include "ns3/olsr-helper.h"
 #include "ns3/csma-helper.h"
 #include "ns3/animation-interface.h"
 
 using namespace ns3;
 
 //
 // Define logging keyword for this file
 //
 NS_LOG_COMPONENT_DEFINE ("MixedWireless");
 
 //
 // This function will be used below as a trace sink, if the command-line
 // argument or default value "useCourseChangeCallback" is set to true
 //
 static void
 CourseChangeCallback (std::string path, Ptr<const MobilityModel> model)
 {
   Vector position = model->GetPosition ();
   std::cout << "CourseChange " << path << " x=" << position.x << ", y=" << position.y << ", z=" << position.z << std::endl;
 }
 
 int
 main (int argc, char *argv[])
 {
   //
   // First, we declare and initialize a few local variables that control some
   // simulation parameters.
   //
   uint32_t backboneNodes = 10;

   uint32_t stopTime = 20;
   bool useCourseChangeCallback = false;
 
   //
   // Simulation defaults are typically set next, before command line
   // arguments are parsed.
   //
   Config::SetDefault ("ns3::OnOffApplication::PacketSize", StringValue ("1472"));
   Config::SetDefault ("ns3::OnOffApplication::DataRate", StringValue ("100kb/s"));
 
   //
   // For convenience, we add the local variables to the command line argument
   // system so that they can be overridden with flags such as
   // "--backboneNodes=20"
   //
   CommandLine cmd (__FILE__);
   cmd.AddValue ("backboneNodes", "number of backbone nodes", backboneNodes);
   //cmd.AddValue ("infraNodes", "number of leaf nodes", infraNodes);
   //cmd.AddValue ("lanNodes", "number of LAN nodes", lanNodes);
   cmd.AddValue ("stopTime", "simulation stop time (seconds)", stopTime);
   cmd.AddValue ("useCourseChangeCallback", "whether to enable course change tracing", useCourseChangeCallback);
 
   //
   // The system global variables and the local values added to the argument
   // system can be overridden by command line arguments by using this call.
   //
   cmd.Parse (argc, argv);
 
   if (stopTime < 10)
     {
       std::cout << "Use a simulation stop time >= 10 seconds" << std::endl;
       exit (1);
     }
   //                                                                       //
   // Construct the backbone                                                //
   //                                                                       //
 
   //
   // Create a container to manage the nodes of the adhoc (backbone) network.
   // Later we'll create the rest of the nodes we'll need.
   //
   NodeContainer backbone;
   backbone.Create (backboneNodes);
   //
   // Create the backbone wifi net devices and install them into the nodes in
   // our container
   //
   WifiHelper wifi;
   WifiMacHelper mac;
   mac.SetType ("ns3::AdhocWifiMac");
   wifi.SetRemoteStationManager ("ns3::ConstantRateWifiManager",
                                 "DataMode", StringValue ("OfdmRate54Mbps"));
   YansWifiPhyHelper wifiPhy;
   wifiPhy.SetPcapDataLinkType (WifiPhyHelper::DLT_IEEE802_11_RADIO);
   YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default ();
   wifiPhy.SetChannel (wifiChannel.Create ());
   NetDeviceContainer backboneDevices = wifi.Install (wifiPhy, mac, backbone);
 
   // We enable OLSR (which will be consulted at a higher priority than
   // the global routing) on the backbone ad hoc nodes
   NS_LOG_INFO ("Enabling OLSR routing on all backbone nodes");
   OlsrHelper olsr;
   //
   // Add the IPv4 protocol stack to the nodes in our container
   //
   InternetStackHelper internet;
   internet.SetRoutingHelper (olsr); // has effect on the next Install ()
   internet.Install (backbone);
 
   //
   // Assign IPv4 addresses to the device drivers (actually to the associated
   // IPv4 interfaces) we just created.
   //
   Ipv4AddressHelper ipAddrs;
   ipAddrs.SetBase ("192.168.0.0", "255.255.255.0");
   ipAddrs.Assign (backboneDevices);
 
   //
   // The ad-hoc network nodes need a mobility model so we aggregate one to
   // each of the nodes we just finished building.
   //
   MobilityHelper mobility;
   mobility.SetPositionAllocator ("ns3::GridPositionAllocator",
                                  "MinX", DoubleValue (20.0),
                                  "MinY", DoubleValue (20.0),
                                  "DeltaX", DoubleValue (20.0),
                                  "DeltaY", DoubleValue (20.0),
                                  "GridWidth", UintegerValue (5),
                                  "LayoutType", StringValue ("RowFirst"));
   mobility.SetMobilityModel ("ns3::RandomDirection2dMobilityModel",
                              "Bounds", RectangleValue (Rectangle (-500, 500, -500, 500)),
                              "Speed", StringValue ("ns3::ConstantRandomVariable[Constant=2]"),
                              "Pause", StringValue ("ns3::ConstantRandomVariable[Constant=0.2]"));
   mobility.Install (backbone);
 
   //                                                                       //
   // Tracing configuration                                                 //
   //                                                                       //
 
   NS_LOG_INFO ("Configure Tracing.");
   CsmaHelper csma;
 
   //
   // Let's set up some ns-2-like ascii traces, using another helper class
   //
   AsciiTraceHelper ascii;
   Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream ("mixed-wireless.tr");
   wifiPhy.EnableAsciiAll (stream);
   csma.EnableAsciiAll (stream);
   internet.EnableAsciiIpv4All (stream);
 
   // Csma captures in non-promiscuous mode
   csma.EnablePcapAll ("mixed-wireless", false);
   // pcap captures on the backbone wifi devices
   wifiPhy.EnablePcap ("mixed-wireless", backboneDevices, false);
   // pcap trace on the application data sink
   //wifiPhy.EnablePcap ("mixed-wireless", appSink->GetId (), 0);
 
   if (useCourseChangeCallback == true)
     {
       Config::Connect ("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback (&CourseChangeCallback));
     }
 
   AnimationInterface anim ("mixed-wireless.xml");
 
   //                                                                       //
   // Run simulation                                                        //
   //                                                                       //
 
   NS_LOG_INFO ("Run Simulation.");
   Simulator::Stop (Seconds (stopTime));
   Simulator::Run ();
   Simulator::Destroy ();
 }
