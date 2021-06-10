/*
 *This is the NS-3 simulation of a Wifi Adhoc network with 20 nodes.
 */
#include "ns3/command-line.h"
#include "ns3/core-module.h"
#include "ns3/applications-module.h"
#include "ns3/mobility-module.h"
#include "ns3/wifi-module.h"
#include "ns3/internet-module.h"
#include "ns3/spectrum-module.h"
#include "ns3/stats-module.h"
#include "ns3/flow-monitor-module.h"
#include "ns3/traffic-control-module.h"
#include "ns3/node-list.h"
#include "ns3/string.h"
#include "ns3/yans-wifi-helper.h"
#include "ns3/ssid.h"
#include "ns3/mobility-helper.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/yans-wifi-channel.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/olsr-helper.h"
#include "ns3/animation-interface.h"
#include "ns3/gnuplot.h"
using namespace ns3;

//
// Define logging keyword for this file
//
NS_LOG_COMPONENT_DEFINE("WifiAdHoc");

class Experiment {
  public:
    Experiment();
    void Run(StringValue onTime, StringValue offTime, uint32_t nodes, uint32_t stopTime, uint32_t packetSizeOnOff, uint32_t radius);
};

// void Experiment::ReceivePacket(Ptr<Socket> socket) {
//   while (socket->Recv()) {
//     NS_LOG_UNCOND("Received one packet!");
//   }
// }

Experiment::Experiment() {}

void Experiment::Run(StringValue onTime, StringValue offTime, uint32_t nodes, uint32_t stopTime, uint32_t packetSizeOnOff, uint32_t radius) {
  double interval = 0.5;	// seconds
  Time interPacketInterval = Seconds(interval);

 	//
 	// First, we declare and initialize a few local variables that control some
 	// simulation parameters.
 	//
  std::string phyMode("DsssRate5_5Mbps");

 	//                                                                      	//
 	// Construct the nodes                                               	   	//
 	//                                                                      	//

 	//
 	// Create a container to manage the nodes of the adhoc (backbone) network.
 	// Later we'll create the rest of the nodes we'll need.
 	//
  NodeContainer backbone;
  backbone.Create(nodes);

 	//
 	// Create the backbone wifi network devices and install them into the nodes in
 	// our container
 	//
  WifiHelper wifi;
  wifi.SetStandard(WIFI_PHY_STANDARD_80211b);
  wifi.SetRemoteStationManager("ns3::ConstantRateWifiManager",
    "DataMode", StringValue(phyMode),
    "ControlMode", StringValue(phyMode));
  WifiMacHelper mac;
  mac.SetType("ns3::AdhocWifiMac");

  YansWifiPhyHelper wifiPhy;
  wifiPhy.SetPcapDataLinkType(WifiPhyHelper::DLT_IEEE802_11_RADIO);

  YansWifiChannelHelper wifiChannel=YansWifiChannelHelper::Default();
  

 	//Wifi channel creation
  wifiPhy.SetChannel(wifiChannel.Create());

  NetDeviceContainer backboneDevices = wifi.Install(wifiPhy, mac, backbone);

 	// We enable OLSR (which will be consulted at a higher priority than
 	// the global routing) on the backbone ad hoc nodes
  NS_LOG_INFO("Enabling OLSR routing on all backbone nodes");
  OlsrHelper olsr;
  Ipv4StaticRoutingHelper staticRouting;

  Ipv4ListRoutingHelper list;
  list.Add(staticRouting, 0);
  list.Add(olsr, 10);

 	//
 	// Add the IPv4 protocol stack to the nodes in our container
 	//
  InternetStackHelper internet;
  internet.SetRoutingHelper(list);	// has effect on the next Install ()
  internet.Install(backbone);

 	//
 	// Assign IPv4 addresses to the device drivers (actually to the associated
 	// IPv4 interfaces) we just created.
 	//
  Ipv4AddressHelper ipAddrs;
  ipAddrs.SetBase("192.168.0.0", "255.255.255.0");
  Ipv4InterfaceContainer i = ipAddrs.Assign(backboneDevices);

 	//
 	// The ad-hoc network nodes need a mobility model so we aggregate one to
 	// each of the nodes we just finished building.
 	//
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator", "rho", DoubleValue(radius),
    "X", DoubleValue(0.0), "Y", DoubleValue(0.0));
  mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
    "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)),
    "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
    "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));
  mobility.Install(backbone);

 	//
 	// OnOff Application
 	//

  uint32_t sourceOnOffNode = 0;
  uint32_t sinkOnOffNode = nodes - 1;
  uint16_t appport = 5000;
 	// Conveniently, the variable "nodes" holds this node index value
  Ptr<Node> appSourceOnOff = NodeList::GetNode(sourceOnOffNode);
 	// We want the sink to be the last node created in the topology.
 	//
  Ptr<Node> appSinkOnOff = NodeList::GetNode(sinkOnOffNode);
 	// Let's fetch the IP address of the last node, which is on Ipv4Interface 1
  Ipv4Address remoteAddr = appSinkOnOff->GetObject<Ipv4> ()->GetAddress(1, 0).GetLocal();

  OnOffHelper onoff("ns3::UdpSocketFactory", Address(InetSocketAddress(remoteAddr, appport)));
  onoff.SetAttribute("OffTime", offTime);
  onoff.SetAttribute("OnTime", onTime);
  onoff.SetAttribute("MaxBytes", UintegerValue(10989173));
  onoff.SetAttribute("DataRate", StringValue("5Mbps"));
  onoff.SetAttribute("PacketSize", UintegerValue(packetSizeOnOff));
  ApplicationContainer appsOnOff = onoff.Install(appSourceOnOff);
  appsOnOff.Start(Seconds(3));
  appsOnOff.Stop(Seconds(stopTime - 1));

 	// Create a packet sink to receive these packets
  PacketSinkHelper sinkOnOff("ns3::UdpSocketFactory", InetSocketAddress(Ipv4Address::GetAny(), appport));
  appsOnOff = sinkOnOff.Install(appSinkOnOff);
  appsOnOff.Start(Seconds(2));
  Ptr<PacketSink> packetSinkServer = DynamicCast<PacketSink>(appsOnOff.Get(0));

 	// Print node positions
  NS_LOG_UNCOND("Node Positions:");
  for (uint32_t i = 0; i < backbone.GetN(); i++) {
    Ptr<Node> node = backbone.Get(i);
    Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
    NS_LOG_UNCOND("---Node ID: " << node->GetId() << " Positions: " << mobility->GetPosition());
  }

 	//                                                                      	//
 	// Tracing configuration                                                	//
 	//                                                                      	//

  NS_LOG_INFO("Configure Tracing.");

 	//
 	// Let's set up some ns-2-like ascii traces, using another helper class
 	//
  AsciiTraceHelper ascii;
  Ptr<OutputStreamWrapper> stream = ascii.CreateFileStream("mixed-wireless.tr");
  wifiPhy.EnableAsciiAll(stream);
  internet.EnableAsciiIpv4All(stream);

 	// pcap captures on the backbone wifi devices
  wifiPhy.EnablePcap("wifi-adhoc", backboneDevices, false);

  AnimationInterface anim("wifi-adhoc.xml");

 	//                                                                      	//
 	// Run simulation                                                       	//
 	//                                                                      	//
  NS_LOG_INFO("Run Simulation.");

 	// Flow monitor
  Ptr<FlowMonitor> flowMonitor;
  FlowMonitorHelper flowHelper;
  flowMonitor = flowHelper.InstallAll();

  Simulator::Stop(Seconds(stopTime));
  Simulator::Run();
  Ptr<Ipv4FlowClassifier> classifier = DynamicCast<Ipv4FlowClassifier> (flowHelper.GetClassifier());
  std::map<FlowId, FlowMonitor::FlowStats > stats = flowMonitor->GetFlowStats();
  std::cout << std::endl << "***Flow monitor statistics ***" << std::endl;
  std::cout << "  Tx Packets:   " << stats[1].txPackets << std::endl;
  std::cout << "  Tx Bytes:   " << stats[1].txBytes << std::endl;
  std::cout << "  Offered Load: " << stats[1].txBytes *8.0 / (stats[1].timeLastTxPacket.GetSeconds() - stats[1].timeFirstTxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Rx Packets:   " << stats[1].rxPackets << std::endl;
  std::cout << "  Rx Bytes:   " << stats[1].rxBytes << std::endl;
  std::cout << "  Throughput: " << stats[1].rxBytes *8.0 / (stats[1].timeLastRxPacket.GetSeconds() - stats[1].timeFirstRxPacket.GetSeconds()) / 1000000 << " Mbps" << std::endl;
  std::cout << "  Mean delay:   " << stats[1].delaySum.GetSeconds() / stats[1].rxPackets << std::endl;
  std::cout << "  Mean jitter:   " << stats[1].jitterSum.GetSeconds() / (stats[1].rxPackets - 1) << std::endl;
  flowMonitor->SerializeToXmlFile("data.flowmon", true, true);
  std::cout << "Number of OnOffPackets received: " << packetSinkServer->GetTotalRx() / packetSizeOnOff << std::endl;
  std::cout << "% of OnOffPackets received: " << (100 * (packetSinkServer->GetTotalRx() / packetSizeOnOff) / stats[1].txPackets) << std::endl;

  Simulator::Destroy();
}

int main(int argc, char *argv[]) {

  uint32_t nodes = 20;
  uint32_t stopTime = 30;
  uint32_t packetSize = 1000;
  uint32_t radius = 10;

 	//
 	// For convenience, we add the local variables to the command line argument
 	// system so that they can be overridden with flags such as
 	// "--nodes=20"
 	//
  CommandLine cmd;
  cmd.AddValue("nodes", "Number of nodes", nodes);
  cmd.AddValue("stopTime", "Simulation stop time (seconds)", stopTime);
  cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
  cmd.AddValue("radius", "The radius of the area to simulate", radius);

 	//
 	// The system global variables and the local values added to the argument
 	// system can be overridden by command line arguments by using this call.
 	//
  cmd.Parse(argc, argv);

  if (stopTime < 10) {
    std::cout << "Use a simulation stop time >= 10 seconds" << std::endl;
    exit(1);
  }

  Experiment experiment;

  // Based on this paper: http://www.scielo.org.co/pdf/dyna/v84n202/0012-7353-dyna-84-202-00055.pdf
  NS_LOG_UNCOND("Traffic video on demand");
  StringValue offTime("ns3::LogNormalRandomVariable[Mu=0.4026|Sigma=0.0352]");
  StringValue onTime("ns3::WeibullRandomVariable[Shape=2|Scale=10]");
  experiment = Experiment();
  experiment.Run(onTime, offTime, nodes, stopTime, packetSize, radius);

  NS_LOG_UNCOND("Traffic calls");
  offTime = StringValue("ns3::ExponentialRandomVariable[Mean=2.0|Bound=10]");
  experiment = Experiment();
  experiment.Run(onTime, offTime, nodes, stopTime, packetSize, radius);

  NS_LOG_UNCOND("Traffic Uniform");
  offTime = StringValue("ns3::UniformRandomVariable[Max=30|Min=0.1]");
  experiment = Experiment();
  experiment.Run(onTime, offTime, nodes, stopTime, packetSize, radius);
}