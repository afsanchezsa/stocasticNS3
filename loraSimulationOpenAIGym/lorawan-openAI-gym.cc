/*
 *This script simulates a complex scenario with multiple gateways and end
 *devices. The metric of interest for this script is the throughput of the
 *network.
 */
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
#include "ns3/end-device-lora-phy.h"
#include "ns3/gateway-lora-phy.h"
#include "ns3/class-a-end-device-lorawan-mac.h"
#include "ns3/gateway-lorawan-mac.h"
#include "ns3/simulator.h"
#include "ns3/log.h"
#include "ns3/pointer.h"
#include "ns3/constant-position-mobility-model.h"
#include "ns3/lora-helper.h"
#include "ns3/node-container.h"
#include "ns3/mobility-helper.h"
#include "ns3/position-allocator.h"
#include "ns3/double.h"
#include "ns3/random-variable-stream.h"
#include "random-periodic-sender-helper.h"
#include "ns3/command-line.h"
#include "ns3/network-server-helper.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"
#include "ns3/building-penetration-loss.h"
#include "ns3/building-allocator.h"
#include "ns3/buildings-helper.h"
#include "ns3/forwarder-helper.h"
#include <algorithm>
#include <ctime>
#include <map>
#include <unordered_map>
using namespace ns3;
using namespace lorawan;

NS_LOG_COMPONENT_DEFINE("LorawanNetworkSimulationOpenAIGym");

Ptr<RandomPeriodicSender> senderApp;
NodeContainer endDevices;
NodeContainer gateways;
std::unordered_map<u_int32_t, u_int32_t> dataRateCorrespondence;	//map spreading factor to its correspondence in datarate

int count = 0;
int received = 0;
int noMoreReceivers = 0;
int interfered = 0;
int underSensitivity = 0;

// 
// OpenAI Gym funcions and setup
// 
Vector getPosition(Ptr<Node> node)
{
  Ptr<MobilityModel> mobility = node->GetObject<MobilityModel> ();
  Vector position = mobility->GetPosition();
  return position;
}

double distanceNode(Vector pos1, Vector pos2)
{
  return sqrt(pow(pos1.x - pos2.x, 2) + pow(pos1.y - pos2.y, 2) + pow(pos1.z - pos2.z, 2));
}

void configureNode(Ptr<Node> node, u_int8_t newWindowValue, u_int8_t newDataRate, uint8_t newSpreadingFactor)
{
 	//// end-device-lorawan-mac.h setDatarate
 	/// y las window en class-a-end-device-lorawan-mac.h
  Ptr<NetDevice> dev = node->GetDevice(0);
  Ptr<LoraNetDevice> lora_dev = DynamicCast<LoraNetDevice> (dev);
  Ptr<LorawanMac> lora_mac = lora_dev->GetMac();
  Ptr<ClassAEndDeviceLorawanMac> endDeviceMac = DynamicCast<ClassAEndDeviceLorawanMac> (lora_mac);
  Ptr<LoraPhy> phy = lora_dev->GetPhy();
  Ptr<EndDeviceLoraPhy> end_device_phy = DynamicCast<EndDeviceLoraPhy> (phy);
  end_device_phy->SetSpreadingFactor(newSpreadingFactor);
  endDeviceMac->SetSecondReceiveWindowDataRate(newWindowValue);
  endDeviceMac->SetDataRate(newDataRate);
 	//endDeviceMac->SetDataRateAdaptation(true);
 	// std::cout<<"is enabled:"<<endDeviceMac->GetDataRateAdaptation()<<std::endl;
}

/*
Define observation space
*/
Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
  uint32_t nodeNum = 1;
  float low = 0.0;
  float high = 100.0;
  std::vector<uint32_t> shape = { nodeNum,
  };
  std::string dtype = TypeNameGet<double> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND("MyGetObservationSpace: " << space);
  return space;
}

/*
Define action space
*/
Ptr<OpenGymSpace> MyGetActionSpace(void)
{
  uint32_t nodeNum = 1;
  float low = 7.0;
  float high = 12.0;
  std::vector<uint32_t> shape = { nodeNum,
  };
  std::string dtype = TypeNameGet<uint32_t> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND("MyGetActionSpace: " << space);
  return space;
}

/*
Define game over condition
*/
bool MyGetGameOver(void)
{
  bool isGameOver = false;
  NS_LOG_UNCOND("MyGetGameOver: " << isGameOver);
  return isGameOver;
}

Ptr<OpenGymDataContainer> MyGetObservation(void)
{
  uint32_t nodeNum = 1;
  std::vector<uint32_t> shape = { nodeNum,
  };
  Ptr<OpenGymBoxContainer < double>> box = CreateObject<OpenGymBoxContainer < double>> (shape);
  NodeList::Iterator gat_i = gateways.Begin();
  Ptr<Node> gatewayNode = *gat_i;
  Vector gatewayPosition = getPosition(gatewayNode);
  for (NodeList::Iterator i = endDevices.Begin(); i != endDevices.End(); ++i)
  {
    Ptr<Node> node = *i;
    Vector nodePosition = getPosition(node);
    double distance = distanceNode(nodePosition, gatewayPosition);

    box->AddValue(distance);
  }
 	// double a = (count - double(received))/count;
  /*double a = double(received) / count;
  a *= 100;

  box->AddValue(floor(a));
*/
  NS_LOG_UNCOND("MyGetObservation: " << box);
  return box;
}

std::string MyGetExtraInfo(void)
{
  std::string myInfo = "linear-wireless-mesh";
  myInfo += "|123";
  NS_LOG_UNCOND("MyGetExtraInfo: " << myInfo);
  return myInfo;
}

bool MyExecuteActions(Ptr<OpenGymDataContainer> action)
{
  NS_LOG_UNCOND("MyExecuteActions: " << action);

  Ptr<OpenGymBoxContainer < uint32_t>> box = DynamicCast<OpenGymBoxContainer < uint32_t>> (action);
  std::vector<uint32_t> actionVector = box->GetData();
  int i = 0;
  u_int32_t newSpreadingFactor;
  for (NodeContainer::Iterator j = endDevices.Begin(); j != endDevices.End(); ++j)
  {
    Ptr<Node> node = *j;
    newSpreadingFactor = actionVector.at(0);
    configureNode(node, 1, dataRateCorrespondence[newSpreadingFactor], newSpreadingFactor);
    i++;
  }
 	// senderApp->SetPacketSize(actionVector.at(0));
  return true;
}

float MyGetReward(void)
{
  static float lastValue = 0.0;
  float reward = (double) received - lastValue;
  lastValue = (float) received;
  return reward;
}

void ScheduleNextStateRead(double envStepTime, Ptr<OpenGymInterface> openGymInterface)
{
  Simulator::Schedule(Seconds(envStepTime), &ScheduleNextStateRead, envStepTime, openGymInterface);
  openGymInterface->NotifyCurrentState();
}

void Tracer(Ptr <const Packet > packet)
{
  NS_LOG_UNCOND("Se perdio");
}

void printIntVector(std::vector < int>
  const &input)
{
  for (std::vector<int>::size_type i = 0; i < input.size(); i++)
  {
    std::cout << input.at(i) << ' ';
  }
  std::cout << std::endl;
}

std::vector<std::string > split(const std::string &str, const std::string &delim)
{
  std::vector<std::string > tokens;
  size_t prev = 0, pos = 0;
  do {
    pos = str.find(delim, prev);
    if (pos == std::string::npos)
      pos = str.length();
    std::string token = str.substr(prev, pos - prev);
    if (!token.empty())
      tokens.push_back(token);
    prev = pos + delim.length();
  } while (pos < str.length() && prev < str.length());
  return tokens;
}

// Network settings
int nDevices = 18;
int nGateways = 1;
double radius = 6400;	//Note that due to model updates, 7500 m is no longer the maximum distance 
double simulationTime = 600;
int packetSize = 20;

// Channel model
bool realisticChannelModel = false;

// Output control
bool print = true;

/************************/
/*Lorawan Tracker */
/************************/
namespace std
{
  enum PacketOutcome
  {
    RECEIVED,
    INTERFERED,
    NO_MORE_RECEIVERS,
    UNDER_SENSITIVITY,
    UNSET
  };
  struct PacketStatus
  {
    Ptr < Packet
    const > packet;
    uint32_t senderId;
    int outcomeNumber;
    std::vector < enum PacketOutcome > outcomes;
  };

};

std::map< Ptr < Packet
const >, std::PacketStatus > packetTracker;

void CheckReceptionByAllGWsComplete(std::map< Ptr < Packet
  const >, std::PacketStatus >::iterator it)
{
 	// Check whether this packet is received by all gateways
  if ((*it).second.outcomeNumber == nGateways)
  {
   	// Update the statistics
    std::PacketStatus status = (*it).second;
    for (int j = 0; j < nGateways; j++)
    {
      switch ((int) status.outcomes.at(j))	//por si acaso castear a entero lo del switch
      {
        case std::RECEIVED:
          {
            received += 1;
            break;
          }
        case std::INTERFERED:
          {
            interfered += 1;
            break;
          }
        case std::NO_MORE_RECEIVERS:
          {
            noMoreReceivers += 1;
            break;
          }
        case std::UNDER_SENSITIVITY:
          {
            underSensitivity += 1;
            break;
          }

        case std::UNSET:
          {
            break;
          }
        default:
          {
            break;
          }
      }
    }
   	// Remove the packet from the tracker
    packetTracker.erase(it);
  }
}

void TransmissionCallback(Ptr < Packet
  const > packet, uint32_t systemId)
{
 	//NS_LOG_DEBUG("Transmitted a packet from device " << systemId);
 	// Create a packetStatus
  std::PacketStatus status;
  status.packet = packet;
  status.senderId = systemId;
  status.outcomeNumber = 0;
  status.outcomes = std::vector<std::PacketOutcome > (UNSET);

  packetTracker.insert(std::pair<Ptr < Packet
    const >, std::PacketStatus > (packet, status));
  count = count + 1;
}
void PacketReceptionCallback(Ptr < Packet
  const > packet, uint32_t systemId)
{
 	//NS_LOG_INFO("A packet was successfully received at gateway " << systemId);
  std::map< Ptr < Packet
  const >, std::PacketStatus >::iterator it = packetTracker.find(packet);
  (*it).second.outcomes.at(systemId - nDevices) = std::RECEIVED;
  (*it).second.outcomeNumber += 1;
  CheckReceptionByAllGWsComplete(it);
}

void InterferenceCallback(Ptr < Packet
  const > packet, uint32_t systemId)
{
 	//NS_LOG_INFO("A packet was interferenced at gateway " << systemId);

  std::map< Ptr < Packet
  const >, std::PacketStatus >::iterator it = packetTracker.find(packet);
  it->second.outcomes.at(systemId - nDevices) = std::INTERFERED;
  it->second.outcomeNumber += 1;
}

void NoMoreReceiversCallback(Ptr < Packet
  const > packet, uint32_t systemId)
{
 	// NS_LOG_INFO ("A packet was lost because there were no more receivers at gateway " << systemId);

  std::map< Ptr < Packet
  const >, std::PacketStatus >::iterator it = packetTracker.find(packet);
  (*it).second.outcomes.at(systemId - nDevices) = std::NO_MORE_RECEIVERS;
  (*it).second.outcomeNumber += 1;

  CheckReceptionByAllGWsComplete(it);
}

void UnderSensitivityCallback(Ptr < Packet
  const > packet, uint32_t systemId)
{
 	// NS_LOG_INFO ("A packet arrived at the gateway under sensitivity at gateway " << systemId);

  std::map< Ptr < Packet
  const >, std::PacketStatus >::iterator it = packetTracker.find(packet);
  (*it).second.outcomes.at(systemId - nDevices) = std::UNDER_SENSITIVITY;
  (*it).second.outcomeNumber += 1;

  CheckReceptionByAllGWsComplete(it);
}

//
// Experiment Class
//
class Experiment
{
  public_colon
  Experiment();
  void Run(Ptr<RandomVariableStream> rv);

  private:
    uint32_t m_bytesTotal;
};

Experiment::Experiment() {}

void Experiment::Run(Ptr<RandomVariableStream> trafficDistribution)
{
  /***********
   *Setup  *
   ***********/

 	// Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator("ns3::UniformDiscPositionAllocator", "rho", DoubleValue(radius),
    "X", DoubleValue(0.0), "Y", DoubleValue(0.0));
  mobility.SetMobilityModel("ns3::RandomDirection2dMobilityModel",
    "Bounds", RectangleValue(Rectangle(-500, 500, -500, 500)),
    "Speed", StringValue("ns3::ConstantRandomVariable[Constant=1]"),
    "Pause", StringValue("ns3::ConstantRandomVariable[Constant=0.2]"));

  /************************
   *Create the channel  *
   ************************/

 	// Create the lora channel object
  Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel> ();
  loss->SetPathLossExponent(3.76);
  loss->SetReference(1, 7.7);

  if (realisticChannelModel)
  {
   	// Create the correlated shadowing component
    Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
      CreateObject<CorrelatedShadowingPropagationLossModel> ();

   	// Aggregate shadowing to the logdistance loss
    loss->SetNext(shadowing);

   	// Add the effect to the channel propagation loss
    Ptr<BuildingPenetrationLoss> buildingLoss = CreateObject<BuildingPenetrationLoss> ();

    shadowing->SetNext(buildingLoss);
  }

  Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel> ();

  Ptr<LoraChannel> channel = CreateObject<LoraChannel> (loss, delay);

  /************************
   *Create the helpers  *
   ************************/

 	// Create the LoraPhyHelper
  LoraPhyHelper phyHelper = LoraPhyHelper();
  phyHelper.SetChannel(channel);

 	// Create the LorawanMacHelper
  LorawanMacHelper macHelper = LorawanMacHelper();

 	// Create the LoraHelper
  LoraHelper helper = LoraHelper();
  helper.EnablePacketTracking();	// Output filename
 	// helper.EnableSimulationTimePrinting ();

 	//Create the NetworkServerHelper
  NetworkServerHelper nsHelper = NetworkServerHelper();

 	//Create the ForwarderHelper
  ForwarderHelper forHelper = ForwarderHelper();

  /************************
   *Create End Devices  *
   ************************/

 	// Create a set of nodes
  endDevices.Create(nDevices);

 	// Assign a mobility model to each node
  mobility.Install(endDevices);

 	// Make it so that nodes are at a certain height > 0
  for (NodeContainer::Iterator j = endDevices.Begin(); j != endDevices.End(); ++j)
  {
    Ptr<MobilityModel> mobility = (*j)->GetObject<MobilityModel> ();
    Vector position = mobility->GetPosition();
    position.z = 1.2;
    mobility->SetPosition(position);
  }

 	// Create the LoraNetDevices of the end devices
  uint8_t nwkId = 54;
  uint32_t nwkAddr = 1864;
  Ptr<LoraDeviceAddressGenerator> addrGen =
    CreateObject<LoraDeviceAddressGenerator> (nwkId, nwkAddr);

 	// Create the LoraNetDevices of the end devices
  macHelper.SetAddressGenerator(addrGen);
  phyHelper.SetDeviceType(LoraPhyHelper::ED);
  macHelper.SetDeviceType(LorawanMacHelper::ED_A);
  helper.Install(phyHelper, macHelper, endDevices);

  dataRateCorrespondence[12] = 0;
  dataRateCorrespondence[11] = 1;
  dataRateCorrespondence[10] = 2;
  dataRateCorrespondence[9] = 3;
  dataRateCorrespondence[8] = 4;
  dataRateCorrespondence[7] = 6;

 	// Now end devices are connected to the channel

 	// Connect trace sources
  for (NodeContainer::Iterator j = endDevices.Begin(); j != endDevices.End(); ++j)
  {
    Ptr<Node> node = *j;
    Ptr<LoraNetDevice> loraNetDevice = node->GetDevice(0)->GetObject<LoraNetDevice> ();
    Ptr<LoraPhy> phy = loraNetDevice->GetPhy();
   	// Trace phy
    phy->TraceConnectWithoutContext("StartSending", MakeCallback(&TransmissionCallback));

    configureNode(node, 1, 0, 12);
  }

  /*********************
   *Create Gateways  *
   *********************/

 	// Create the gateway nodes (allocate them uniformely on the disc)
  gateways.Create(nGateways);

  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
 	// Make it so that nodes are at a certain height > 0
  allocator->Add(Vector(0.0, 0.0, 15.0));
  mobility.SetPositionAllocator(allocator);
  mobility.SetMobilityModel("ns3::ConstantPositionMobilityModel");
  mobility.Install(gateways);

 	// Create a netdevice for each gateway
  phyHelper.SetDeviceType(LoraPhyHelper::GW);
  macHelper.SetDeviceType(LorawanMacHelper::GW);
  helper.Install(phyHelper, macHelper, gateways);

 	// Install traces

  for (NodeContainer::Iterator j = gateways.Begin(); j != gateways.End(); j++)
  {
    Ptr<Node> gNode = *j;
    Ptr<NetDevice> gNetDevice = gNode->GetDevice(0);
    Ptr<LoraNetDevice> gLoraNetDevice = gNetDevice->GetObject<LoraNetDevice> ();
    NS_ASSERT(gLoraNetDevice != 0);
    Ptr<GatewayLoraPhy> gwPhy = gLoraNetDevice->GetPhy()->GetObject<GatewayLoraPhy> ();
    gwPhy->TraceConnectWithoutContext("ReceivedPacket",
      MakeCallback(&PacketReceptionCallback));
    gwPhy->TraceConnectWithoutContext("LostPacketBecauseInterference",
      MakeCallback(&InterferenceCallback));
    gwPhy->TraceConnectWithoutContext("LostPacketBecauseNoMoreReceivers",
      MakeCallback(&NoMoreReceiversCallback));
    gwPhy->TraceConnectWithoutContext("LostPacketBecauseUnderSensitivity",
      MakeCallback(&UnderSensitivityCallback));
  }

  /**********************
   *Handle buildings  *
   **********************/

  double xLength = 130;
  double deltaX = 32;
  double yLength = 64;
  double deltaY = 17;
  int gridWidth = 2 *radius / (xLength + deltaX);
  int gridHeight = 2 *radius / (yLength + deltaY);
  if (realisticChannelModel == false)
  {
    gridWidth = 0;
    gridHeight = 0;
  }
  Ptr<GridBuildingAllocator> gridBuildingAllocator;
  gridBuildingAllocator = CreateObject<GridBuildingAllocator> ();
  gridBuildingAllocator->SetAttribute("GridWidth", UintegerValue(gridWidth));
  gridBuildingAllocator->SetAttribute("LengthX", DoubleValue(xLength));
  gridBuildingAllocator->SetAttribute("LengthY", DoubleValue(yLength));
  gridBuildingAllocator->SetAttribute("DeltaX", DoubleValue(deltaX));
  gridBuildingAllocator->SetAttribute("DeltaY", DoubleValue(deltaY));
  gridBuildingAllocator->SetAttribute("Height", DoubleValue(6));
  gridBuildingAllocator->SetBuildingAttribute("NRoomsX", UintegerValue(2));
  gridBuildingAllocator->SetBuildingAttribute("NRoomsY", UintegerValue(4));
  gridBuildingAllocator->SetBuildingAttribute("NFloors", UintegerValue(2));
  gridBuildingAllocator->SetAttribute("MinX", DoubleValue(-gridWidth *(xLength + deltaX) / 2 + deltaX / 2));
  gridBuildingAllocator->SetAttribute("MinY", DoubleValue(-gridHeight *(yLength + deltaY) / 2 + deltaY / 2));
  BuildingContainer bContainer = gridBuildingAllocator->Create(gridWidth *gridHeight);

  BuildingsHelper::Install(endDevices);
  BuildingsHelper::Install(gateways);

 	// Print the buildings
  if (print)
  {
    std::ofstream myfile;
    myfile.open("buildings.txt");
    std::vector<Ptr<Building>>::const_iterator it;
    int j = 1;
    for (it = bContainer.Begin(); it != bContainer.End(); ++it, ++j)
    {
      Box boundaries = (*it)->GetBoundaries();
      myfile << "set object " << j << " rect from " << boundaries.xMin << "," << boundaries.yMin <<
        " to " << boundaries.xMax << "," << boundaries.yMax << std::endl;
    }
    myfile.close();
  }

  /**********************************************
   *Set up the end device's spreading factor  *
   **********************************************/

 	// macHelper.SetSpreadingFactorsUp(endDevices, gateways, channel);

  NS_LOG_DEBUG("Completed configuration");

  /*********************************************
   *Install applications on the end devices  *
   *********************************************/

  Time appStopTime = Seconds(simulationTime);
  RandomPeriodicSenderHelper appHelper = RandomPeriodicSenderHelper();
  appHelper.SetPeriodRandomVariable(trafficDistribution);
  appHelper.SetPacketSize(packetSize);
  ApplicationContainer appContainer = appHelper.Install(endDevices);

  senderApp = DynamicCast<RandomPeriodicSender> (appContainer.Get(0));

  appContainer.Start(Seconds(0));
  appContainer.Stop(appStopTime);

  /**************************
   *Create Network Server  *
   ***************************/

 	// Create the NS node
  NodeContainer networkServer;
  networkServer.Create(1);

 	// Create a NS for the network
  nsHelper.SetEndDevices(endDevices);
  nsHelper.SetGateways(gateways);
  nsHelper.Install(networkServer);

 	//Create a forwarder for each gateway
  forHelper.Install(gateways);

 	// OpenGym Env
  uint16_t port = 5555;
  double envStepTime = 10;
  Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (port);
  openGymInterface->SetGetActionSpaceCb(MakeCallback(&MyGetActionSpace));
  openGymInterface->SetGetObservationSpaceCb(MakeCallback(&MyGetObservationSpace));
  openGymInterface->SetGetGameOverCb(MakeCallback(&MyGetGameOver));
  openGymInterface->SetGetObservationCb(MakeCallback(&MyGetObservation));
  openGymInterface->SetGetRewardCb(MakeCallback(&MyGetReward));
  openGymInterface->SetGetExtraInfoCb(MakeCallback(&MyGetExtraInfo));
  openGymInterface->SetExecuteActionsCb(MakeCallback(&MyExecuteActions));

  Simulator::Schedule(Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGymInterface);

 	////////////////
 	// Simulation	//
 	////////////////

 	// Flow monitor
  Simulator::Stop(appStopTime + Seconds(180));

  NS_LOG_INFO("Running simulation...");
  Simulator::Run();

  openGymInterface->NotifySimulationEnd();

  Simulator::Destroy();

 	///////////////////////////
 	// Print results to file	//
 	///////////////////////////
  NS_LOG_INFO("Computing performance metrics...");

  std::cout << "\n/////////////////////////////\n" <<
    std::endl;
  NS_LOG_INFO("Computing performance metrics...");
  std::cout << "\n/////////////////////////////\n" <<
    std::endl;
  double receivedProb = double(received) / count;
  double packetLost = count - double(received);
  double interferedProb = double(interfered) / nDevices;
  double noMoreReceiversProb = double(noMoreReceivers) / nDevices;
  double underSensitivityProb = double(underSensitivity) / nDevices;

  double receivedProbGivenAboveSensitivity = double(received) / (count - underSensitivity);
  double interferedProbGivenAboveSensitivity = double(interfered) / (nDevices - underSensitivity);
  double noMoreReceiversProbGivenAboveSensitivity = double(noMoreReceivers) / (nDevices - underSensitivity);
  std::cout << "Numero de End Devices:" << nDevices <<
    "\nPaquetes Enviados:" << count <<
    "\nPaquetes Perdidos:" << packetLost <<
    "\nPaquetes Recibidos:" << received <<
    "\nProbabilidad de Recepcion satisfactoria:" << receivedProb <<
    "\nThrougput:" << double(received) *8 *packetSize / double(simulationTime) << " bps" <<
    "\nProbabilidad de Interferencia:" << interferedProb <<
    "\nProbabilidad de No Recepcion:" << noMoreReceiversProb <<
    "\nProbabilidad de Baja Sensibilidad:" << underSensitivityProb <<
    "\nProbabilidad de No Recepcion:" << noMoreReceiversProb <<
    "\nProbabilidad de Recepcion dada una alta Sensibilidad:" << receivedProbGivenAboveSensitivity <<
    "\nProbabilidad de Interferencia dada una alta Sensibilidad:" << interferedProbGivenAboveSensitivity <<
    "\nProbabilidad de No Recepcion dada una alta Sensibilidad:" << noMoreReceiversProbGivenAboveSensitivity << "\n\n";

  LoraPacketTracker &tracker = helper.GetPacketTracker();
  std::cout << "Tx Packets\tRxPackets\n";
  std::cout << tracker.CountMacPacketsGlobally(Seconds(0), appStopTime + Hours(1)) << std::endl;
}

int main(int argc, char *argv[])
{
  CommandLine cmd;
  cmd.AddValue("nDevices", "Number of end devices to include in the simulation", nDevices);
  cmd.AddValue("radius", "The radius of the area to simulate", radius);
  cmd.AddValue("simulationTime", "The time for which to simulate", simulationTime);
  cmd.AddValue("packetSize", "Packet size (bytes)", packetSize);
  cmd.AddValue("print", "Whether or not to print various informations", print);

  cmd.Parse(argc, argv);

 	// Set up logging
  LogComponentEnable("LorawanNetworkSimulationOpenAIGym", LOG_LEVEL_ALL);

  Experiment experiment;
  NS_LOG_INFO("\nDistribución Uniforme");
  experiment.Run(CreateObjectWithAttributes<UniformRandomVariable> ("Min", DoubleValue(0), "Max", DoubleValue(10)));

 	// NS_LOG_INFO("\nDistribución Exponencial");
 	// experiment.Run(CreateObjectWithAttributes<ExponentialRandomVariable>("Mean", DoubleValue(2), "Bound", DoubleValue(10)));

 	// NS_LOG_INFO("\nDistribución Video on Demand");
 	// experiment.Run(CreateObjectWithAttributes<WeibullRandomVariable>("Scale", DoubleValue(2), "Shape", DoubleValue(10)));

  return 0;
}