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
// to a local LAN.  An additional set of (K-1) backboneNodes are connected to
// this backbone.  Finally, a local LAN is connected to each router
// on the backbone, with L-1 additional hosts.
//
// The backboneNodes are populated with TCP/IP stacks, and OLSR unicast routing
// on the backbone.  An example UDP transfer is shown.  The simulator
// be configured to output tcpdumps or traces from different backboneNodes.
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
// to fail (if backboneNodes become disconnected)
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/opengym-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/node-list.h"

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
NS_LOG_COMPONENT_DEFINE("MixedWireless");

//
// This function will be used below as a trace sink, if the command-line
// argument or default value "useCourseChangeCallback" is set to true
//
void ReceiveOnOffPacket(Ptr<Socket> socket)
{
  while (socket->Recv())
  {
   NS_LOG_UNCOND("OnOff Client received a packet of  bytes");
  }
   
  
}

void ReceivePacket(Ptr<Socket> socket)
{
  while (socket->Recv())
  {
    NS_LOG_UNCOND("Received one packet!");
  }
}

static void GenerateTraffic(Ptr<Socket> socket, uint32_t pktSize,
                            uint32_t pktCount, Time pktInterval)
{
  if (pktCount > 0)
  {
    socket->Send(Create<Packet>(pktSize));
    Simulator::Schedule(pktInterval, &GenerateTraffic,
                        socket, pktSize, pktCount - 1, pktInterval);
  }
  else
  {
    socket->Close();
  }
}

static void
CourseChangeCallback(std::string path, Ptr<const MobilityModel> model)
{
  Vector position = model->GetPosition();
  std::cout << "CourseChange " << path << " x=" << position.x << ", y=" << position.y << ", z=" << position.z << std::endl;
}
uint64_t g_rxPktNum = 0;
void DestRxPkt(Ptr<const Packet> packet)
{
  NS_LOG_UNCOND("Client received a packet of " << packet->GetSize() << " bytes");
  g_rxPktNum++;
}
int main(int argc, char *argv[])
{

  double interval = 1.0; // seconds
  Time interPacketInterval = Seconds(interval);
  //
  // First, we declare and initialize a few local variables that control some
  // simulation parameters.
  //
  std::string phyMode("DsssRate1Mbps");
  uint32_t backboneNodes = 20;
  uint32_t stopTime = 30;
  bool useCourseChangeCallback = false;
  uint32_t pktPerSec = 10;
  uint32_t payloadSize = 1500;

  //
  // Simulation defaults are typically set next, before command line
  // arguments are parsed.
  //
  Config::SetDefault("ns3::OnOffApplication::PacketSize", StringValue("1472"));
  Config::SetDefault("ns3::OnOffApplication::DataRate", StringValue("100kb/s"));

  //
  // For convenience, we add the local variables to the command line argument
  // system so that they can be overridden with flags such as
  // "--backboneNodes=20"
  //
  CommandLine cmd(__FILE__);
  cmd.AddValue("backboneNodes", "number of backbone backboneNodes", backboneNodes);
  //cmd.AddValue ("infraNodes", "number of leaf backboneNodes", infraNodes);
  //cmd.AddValue ("lanNodes", "number of LAN backboneNodes", lanNodes);
  cmd.AddValue("stopTime", "simulation stop time (seconds)", stopTime);
  cmd.AddValue("useCourseChangeCallback", "whether to enable course change tracing", useCourseChangeCallback);

  //
  // The system global variables and the local values added to the argument
  // system can be overridden by command line arguments by using this call.
  //
  cmd.Parse(argc, argv);

  if (stopTime < 10)
  {
    std::cout << "Use a simulation stop time >= 10 seconds" << std::endl;
    exit(1);
  }
  //                                                                       //
  // Construct the backbone                                                //
  //                                                                       //

  //
  // Create a container to manage the backboneNodes of the adhoc (backbone) network.
  // Later we'll create the rest of the backboneNodes we'll need.
  //
  NodeContainer backbone;
  backbone.Create(backboneNodes);
  //
  // Create the backbone wifi net devices and install them into the backboneNodes in
  // our container
  //
  WifiHelper wifi;
  wifi.SetStandard(WIFI_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
                               "DataMode", StringValue(phyMode),
                               "ControlMode", StringValue(phyMode));
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel = YansWifiChannelHelper::Default();
  wifiPhy.SetChannel(wifiChannel.Create());

  NetDeviceContainer backboneDevices = wifi.Install(wifiPhy, mac, backbone);

  // We enable OLSR (which will be consulted at a higher priority than
  // the global routing) on the backbone ad hoc backboneNodes
  NS_LOG_INFO("Enabling OLSR routing on all backbone backboneNodes");
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add(staticRouting, 0);
  list.Add(olsr, 10);
  //
  // Add the IPv4 protocol stack to the backboneNodes in our container
  //
  InternetStackHelper internet;
  internet.SetRoutingHelper(list); // has effect on the next Install ()
  internet.Install(backbone);

  //
  // Assign IPv4 addresses to the device drivers (actually to the associated
  // IPv4 interfaces) we just created.
  //
  Ipv4AddressHelper ipAddrs;
  ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipAddrs.Assign(backboneDevices);

  //
  // The ad-hoc network backboneNodes need a mobility model so we aggregate one to
  // each of the backboneNodes we just finished building.
  //
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::GridPositionAllocator",
                                "MinX", DoubleValue(20.0),
                                "MinY", DoubleValue(20.0),
                                "DeltaX", DoubleValue(10.0),
                                "DeltaY", DoubleValue(10.0),
                                "GridWidth", UintegerValue(5),
                                "LayoutType", StringValue("RowFirst"));
  mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
                            "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)),
                            "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
                            "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
  //mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");
  mobility.Install(backbone);

  /// recurso: https://github.com/xianliangjiang/NS3-Gym/blob/master/scratch/linear-mesh/sim.cc
  // Traffic
  // Create a BulkSendApplication and install it on node 0
  Ptr<UniformRandomVariable> startTimeRng = CreateObject<UniformRandomVariable>();
  startTimeRng->SetAttribute("Min", DoubleValue(0.0));
  startTimeRng->SetAttribute("Max", DoubleValue(1.0));

  uint32_t port = 9;
  uint32_t srcNodeId = 0;
  uint32_t destNodeId = backbone.GetN() - 1;
  Ptr<Node> srcNode = backbone.Get(srcNodeId);
  Ptr<Node> dstNode = backbone.Get(destNodeId);

  Ptr<Ipv4> destIpv4 = dstNode->GetObject<Ipv4>();
  Ipv4InterfaceAddress dest_ipv4_int_addr = destIpv4->GetAddress(1, 0);
  Ipv4Address dest_ip_addr = dest_ipv4_int_addr.GetLocal();

  InetSocketAddress destAddress(dest_ip_addr, port);
  destAddress.SetTos(0x70); //AC_BE
  UdpClientHelper source(destAddress);
  source.SetAttribute("MaxPackets", UintegerValue(pktPerSec * stopTime));
  source.SetAttribute("PacketSize", UintegerValue(payloadSize));
  Time interPacketIntervalp = Seconds(1.0 / pktPerSec);
  source.SetAttribute("Interval", TimeValue(interPacketIntervalp)); //packets/s

  ApplicationContainer sourceApps = source.Install(srcNode);
  sourceApps.Start(Seconds(0.0));
  sourceApps.Stop(Seconds(stopTime));

  // Create a packet sink to receive these packets
  UdpServerHelper sink(port);
  ApplicationContainer sinkApps = sink.Install(dstNode);
  sinkApps.Start(Seconds(0.0));
  sinkApps.Stop(Seconds(stopTime));

  Ptr<UdpServer> udpServer = DynamicCast<UdpServer>(sinkApps.Get(0));
  udpServer->TraceConnectWithoutContext("Rx", MakeCallback(&DestRxPkt));

  uint16_t sourceOnOffNode = 3;
  uint16_t appport = 5000;
  uint32_t sinkOnOffNode = 14;
  uint32_t packetSizeOnOff = 128;
  // Conveniently, the variable "backboneNodes" holds this node index value
  Ptr<Node> appSourceOnOff = NodeList::GetNode(sourceOnOffNode);
  // We want the sink to be the last node created in the topology.
  //;
  Ptr<Node> appSinkOnOff = NodeList::GetNode(sinkOnOffNode);
  // Let's fetch the IP address of the last node, which is on Ipv4Interface 1
  Ipv4Address remoteAddr = appSinkOnOff->GetObject<Ipv4>()->GetAddress(1, 0).GetLocal();

  OnOffHelper onoff("ns3::UdpSocketFactory",
                    Address(InetSocketAddress(remoteAddr, appport)));
  onoff.SetAttribute ("PacketSize", UintegerValue (packetSizeOnOff));
       // onoff.SetAttribute ("OnTime",  StringValue ("ns3::ConstantRandomVariable[Constant=1]"));
       // onoff.SetAttribute ("OffTime", StringValue ("ns3::ConstantRandomVariable[Constant=0]"));
  ApplicationContainer appsOnOff = onoff.Install(appSourceOnOff);
  appsOnOff.Start(Seconds(8));
  appsOnOff.Stop(Seconds(stopTime - 1));

  // Create a packet sink to receive these packets
  PacketSinkHelper sinkOnOff("ns3::UdpSocketFactory",
                        InetSocketAddress(Ipv4Address::GetAny(), appport));
  //appSinkOnOff->TraceConnectWithoutContext ("Rx",MakeCallback(&ReceiveOnOffPacket));
  appsOnOff = sinkOnOff.Install(appSinkOnOff);
  appsOnOff.Start(Seconds(3));
Ptr<PacketSink> packetSinkServer = DynamicCast<PacketSink>(appsOnOff.Get(0));
  //p->TraceConnectWithoutContext("Rx", MakeCallback(&ReceiveOnOffPacket));
 //Ptr<Socket> listen=p->GetListeningSocket();
 

      uint32_t packetSize = 1000; // bytes
  uint32_t numPackets = 40;
  //uint32_t numNodes = 25;  // by default, 5x5
  uint32_t sinkNode = 18;
  uint32_t sourceNode = 1;
  TypeId tid = TypeId::LookupByName("ns3::UdpSocketFactory");
  Ptr<Socket> recvSink = Socket::CreateSocket(backbone.Get(sinkNode), tid);
  InetSocketAddress local = InetSocketAddress(Ipv4Address::GetAny(), 80);
  recvSink->Bind(local);
  recvSink->SetRecvCallback(MakeCallback(&ReceivePacket));

  Ptr<Socket> sourceS = Socket::CreateSocket(backbone.Get(sourceNode), tid);
  InetSocketAddress remote = InetSocketAddress(i.GetAddress(sinkNode, 0), 80);
  sourceS->Connect(remote);

  // Give OLSR time to converge-- 30 seconds perhaps
  Simulator::Schedule(Seconds(3.0), &GenerateTraffic,
                      sourceS, packetSize, numPackets, interPacketInterval);
  // Print node positions
  NS_LOG_UNCOND("Node Positions:");
  for (uint32_t i = 0; i < backbone.GetN(); i++)
  {
    Ptr<Node> node = backbone.Get(i);
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel>();
    NS_LOG_UNCOND("---Node ID: " << node->GetId() << " Positions: " << mobility->GetPosition());
  }

  // fin recurso: https://github.com/xianliangjiang/NS3-Gym/blob/master/scratch/linear-mesh/sim.cc

  //                                                                       //
  // Tracing configuration                                                 //
  //                                                                       //

  NS_LOG_INFO("Configure Tracing.");
  CsmaHelper csma;

  //
  // Let's set up some ns-2-like ascii traces, using another helper class
  //
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("mixed-wireless.tr");
  wifiPhy.EnableAsciiAll(stream);
  csma.EnableAsciiAll(stream);
  internet.EnableAsciiIpv4All(stream);

  // Csma captures in non-promiscuous mode
  csma.EnablePcapAll("mixed-wireless", false);
  // pcap captures on the backbone wifi devices
  wifiPhy.EnablePcap("mixed-wireless", backboneDevices, false);
  // pcap trace on the application data sink
  //wifiPhy.EnablePcap ("mixed-wireless", appSink->GetId (), 0);

  if (useCourseChangeCallback == true)
  {
    Config::Connect("/NodeList/*/$ns3::MobilityModel/CourseChange", MakeCallback(&CourseChangeCallback));
  }

  AnimationInterface anim("mixed-wireless.xml");

  //                                                                       //
  // Run simulation                                                        //
  //                                                                       //

  NS_LOG_INFO("Run Simulation.");
  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  std::cout << "Number of Udp Packets received by UdpServer:" << g_rxPktNum << std::endl;
  std::cout<<"Number of OnOffPackets received:"<<packetSinkServer->GetTotalRx()/packetSizeOnOff<<std::endl; 	
  Simulator::Destroy();
}
