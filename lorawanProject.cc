/*
 * This script simulates a complex scenario with multiple gateways and end
 * devices. The metric of interest for this script is the throughput of the
 * network.
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
#include "ns3/periodic-sender-helper.h"
#include "ns3/command-line.h"
#include "ns3/network-server-helper.h"
#include "ns3/correlated-shadowing-propagation-loss-model.h"
#include "ns3/building-penetration-loss.h"
#include "ns3/building-allocator.h"
#include "ns3/buildings-helper.h"
#include "ns3/forwarder-helper.h"
#include <algorithm>
#include <ctime>

using namespace ns3;
using namespace lorawan;
//
 // Define logging keyword for this file
 //
NS_LOG_COMPONENT_DEFINE ("ComplexLorawanNetworkExample");
 /*
Define observation space
*/
Ptr<OpenGymSpace> MyGetObservationSpace(void)
{
  uint32_t nodeNum = NodeList::GetNNodes ();
  float low = 0.0;
  float high = 100.0;
  std::vector<uint32_t> shape = {nodeNum,};
  std::string dtype = TypeNameGet<uint32_t> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND ("MyGetObservationSpace: " << space);
  return space;
}

/*
Define action space
*/
Ptr<OpenGymSpace> MyGetActionSpace(void)
{
  uint32_t nodeNum = NodeList::GetNNodes ();
  float low = 0.0;
  float high = 100.0;
  std::vector<uint32_t> shape = {nodeNum,};
  std::string dtype = TypeNameGet<uint32_t> ();
  Ptr<OpenGymBoxSpace> space = CreateObject<OpenGymBoxSpace> (low, high, shape, dtype);
  NS_LOG_UNCOND ("MyGetActionSpace: " << space);
  return space;
}

/*
Define game over condition
*/
bool MyGetGameOver(void)
{
  bool isGameOver = false;
  NS_LOG_UNCOND ("MyGetGameOver: " << isGameOver);
  return isGameOver;
}
void Tracer(Ptr< const Packet > packet)
{  NS_LOG_UNCOND ("Se perdio" );
  }
void ConfigureTracing(Ptr<Node> node,u_int8_t windowValue,u_int8_t dataRate)
{//// end-device-lorawan-mac.h setDatarate
/// y las window en class-a-end-device-lorawan-mac.h 
  Ptr<NetDevice> dev = node->GetDevice (0);
  Ptr<LoraNetDevice> lora_dev = DynamicCast<LoraNetDevice> (dev);
  Ptr<LorawanMac> lora_mac = lora_dev->GetMac ();
  Ptr<ClassAEndDeviceLorawanMac>endDeviceMac=DynamicCast<ClassAEndDeviceLorawanMac>(lora_mac);
  
  //lora_mac->TraceConnectWithoutContext ("CannotSendBecauseDutyCycle", MakeCallback(&Tracer));
  endDeviceMac->SetSecondReceiveWindowDataRate(windowValue);
  endDeviceMac->SetDataRate(dataRate);
  
  
}


 //
 // This function will be used below as a trace sink, if the command-line
 // argument or default value "useCourseChangeCallback" is set to true
 //



//NS_LOG_COMPONENT_DEFINE ("ComplexLorawanNetworkExample");

// Network settings
int nDevices = 20;
int nGateways = 3;
double radius = 64; //Note that due to model updates, 7500 m is no longer the maximum distance 
double simulationTime = 600;

// Channel model
bool realisticChannelModel = false;

int appPeriodSeconds = 1;

// Output control
bool print = true;

int
main (int argc, char *argv[])
{

  CommandLine cmd;
  cmd.AddValue ("nDevices", "Number of end devices to include in the simulation", nDevices);
  cmd.AddValue ("radius", "The radius of the area to simulate", radius);
  cmd.AddValue ("simulationTime", "The time for which to simulate", simulationTime);
  cmd.AddValue ("appPeriod",
                "The period in seconds to be used by periodically transmitting applications",
                appPeriodSeconds);
  cmd.AddValue ("print", "Whether or not to print various informations", print);
  cmd.Parse (argc, argv);

  // Set up logging
  LogComponentEnable ("ComplexLorawanNetworkExample", LOG_LEVEL_ALL);
  // LogComponentEnable("LoraChannel", LOG_LEVEL_INFO);
  // LogComponentEnable("LoraPhy", LOG_LEVEL_ALL);
  // LogComponentEnable("EndDeviceLoraPhy", LOG_LEVEL_ALL);
  // LogComponentEnable("GatewayLoraPhy", LOG_LEVEL_ALL);
  // LogComponentEnable("LoraInterferenceHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("LorawanMac", LOG_LEVEL_ALL);
  // LogComponentEnable("EndDeviceLorawanMac", LOG_LEVEL_ALL);
  // LogComponentEnable("ClassAEndDeviceLorawanMac", LOG_LEVEL_ALL);
  // LogComponentEnable("GatewayLorawanMac", LOG_LEVEL_ALL);
  // LogComponentEnable("LogicalLoraChannelHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("LogicalLoraChannel", LOG_LEVEL_ALL);
  // LogComponentEnable("LoraHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("LoraPhyHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("LorawanMacHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("PeriodicSenderHelper", LOG_LEVEL_ALL);
  // LogComponentEnable("PeriodicSender", LOG_LEVEL_ALL);
  // LogComponentEnable("LorawanMacHeader", LOG_LEVEL_ALL);
  // LogComponentEnable("LoraFrameHeader", LOG_LEVEL_ALL);
  // LogComponentEnable("NetworkScheduler", LOG_LEVEL_ALL);
  // LogComponentEnable("NetworkServer", LOG_LEVEL_ALL);
  // LogComponentEnable("NetworkStatus", LOG_LEVEL_ALL);
  // LogComponentEnable("NetworkController", LOG_LEVEL_ALL);

  /***********
   *  Setup  *
   ***********/

  // Create the time value from the period
  Time appPeriod = Seconds (appPeriodSeconds);

  // Mobility
  MobilityHelper mobility;
  mobility.SetPositionAllocator ("ns3::UniformDiscPositionAllocator", "rho", DoubleValue (radius),
                                 "X", DoubleValue (0.0), "Y", DoubleValue (0.0));
  mobility.SetMobilityModel ("ns3::ConstantPositionMobilityModel");

  /************************
   *  Create the channel  *
   ************************/

  // Create the lora channel object
  Ptr<LogDistancePropagationLossModel> loss = CreateObject<LogDistancePropagationLossModel> ();
  loss->SetPathLossExponent (3.76);
  loss->SetReference (1, 7.7);

  if (realisticChannelModel)
    {
      // Create the correlated shadowing component
      Ptr<CorrelatedShadowingPropagationLossModel> shadowing =
          CreateObject<CorrelatedShadowingPropagationLossModel> ();

      // Aggregate shadowing to the logdistance loss
      loss->SetNext (shadowing);

      // Add the effect to the channel propagation loss
      Ptr<BuildingPenetrationLoss> buildingLoss = CreateObject<BuildingPenetrationLoss> ();

      shadowing->SetNext (buildingLoss);
    }

  Ptr<PropagationDelayModel> delay = CreateObject<ConstantSpeedPropagationDelayModel> ();

  Ptr<LoraChannel> channel = CreateObject<LoraChannel> (loss, delay);

  /************************
   *  Create the helpers  *
   ************************/

  // Create the LoraPhyHelper
  LoraPhyHelper phyHelper = LoraPhyHelper ();
  phyHelper.SetChannel (channel);

  // Create the LorawanMacHelper
  LorawanMacHelper macHelper = LorawanMacHelper ();

  // Create the LoraHelper
  LoraHelper helper = LoraHelper ();
  helper.EnablePacketTracking (); // Output filename
  // helper.EnableSimulationTimePrinting ();

  //Create the NetworkServerHelper
  NetworkServerHelper nsHelper = NetworkServerHelper ();

  //Create the ForwarderHelper
  ForwarderHelper forHelper = ForwarderHelper ();

  /************************
   *  Create End Devices  *
   ************************/

  // Create a set of nodes
  NodeContainer endDevices;
  endDevices.Create (nDevices);

  // Assign a mobility model to each node
  mobility.Install (endDevices);

  // Make it so that nodes are at a certain height > 0
  for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
    {
      Ptr<MobilityModel> mobility = (*j)->GetObject<MobilityModel> ();
      Vector position = mobility->GetPosition ();
      position.z = 1.2;
      mobility->SetPosition (position);
    }

  // Create the LoraNetDevices of the end devices
  uint8_t nwkId = 54;
  uint32_t nwkAddr = 1864;
  Ptr<LoraDeviceAddressGenerator> addrGen =
      CreateObject<LoraDeviceAddressGenerator> (nwkId, nwkAddr);

  // Create the LoraNetDevices of the end devices
  macHelper.SetAddressGenerator (addrGen);
  phyHelper.SetDeviceType (LoraPhyHelper::ED);
  macHelper.SetDeviceType (LorawanMacHelper::ED_A);
  helper.Install (phyHelper, macHelper, endDevices);

  // Now end devices are connected to the channel

  // Connect trace sources
  for (NodeContainer::Iterator j = endDevices.Begin (); j != endDevices.End (); ++j)
    {
      Ptr<Node> node = *j;
      Ptr<LoraNetDevice> loraNetDevice = node->GetDevice (0)->GetObject<LoraNetDevice> ();
      Ptr<LoraPhy> phy = loraNetDevice->GetPhy ();


      ///////////////////////
      //ConfigureTracing(node,1,250);
    }

/*NodeContainer::Iterator j=endDevices.Begin();
Ptr<Node> firstNode=*j;
ConfigureTracing(firstNode);*/
  /*********************
   *  Create Gateways  *
   *********************/

  // Create the gateway nodes (allocate them uniformely on the disc)
  NodeContainer gateways;
  gateways.Create (nGateways);

  Ptr<ListPositionAllocator> allocator = CreateObject<ListPositionAllocator> ();
  // Make it so that nodes are at a certain height > 0
  allocator->Add (Vector (0.0, 0.0, 15.0));
  mobility.SetPositionAllocator (allocator);
  mobility.Install (gateways);

  // Create a netdevice for each gateway
  phyHelper.SetDeviceType (LoraPhyHelper::GW);
  macHelper.SetDeviceType (LorawanMacHelper::GW);
  helper.Install (phyHelper, macHelper, gateways);

  /**********************
   *  Handle buildings  *
   **********************/

  double xLength = 130;
  double deltaX = 32;
  double yLength = 64;
  double deltaY = 17;
  int gridWidth = 2 * radius / (xLength + deltaX);
  int gridHeight = 2 * radius / (yLength + deltaY);
  if (realisticChannelModel == false)
    {
      gridWidth = 0;
      gridHeight = 0;
    }
  Ptr<GridBuildingAllocator> gridBuildingAllocator;
  gridBuildingAllocator = CreateObject<GridBuildingAllocator> ();
  gridBuildingAllocator->SetAttribute ("GridWidth", UintegerValue (gridWidth));
  gridBuildingAllocator->SetAttribute ("LengthX", DoubleValue (xLength));
  gridBuildingAllocator->SetAttribute ("LengthY", DoubleValue (yLength));
  gridBuildingAllocator->SetAttribute ("DeltaX", DoubleValue (deltaX));
  gridBuildingAllocator->SetAttribute ("DeltaY", DoubleValue (deltaY));
  gridBuildingAllocator->SetAttribute ("Height", DoubleValue (6));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsX", UintegerValue (2));
  gridBuildingAllocator->SetBuildingAttribute ("NRoomsY", UintegerValue (4));
  gridBuildingAllocator->SetBuildingAttribute ("NFloors", UintegerValue (2));
  gridBuildingAllocator->SetAttribute (
      "MinX", DoubleValue (-gridWidth * (xLength + deltaX) / 2 + deltaX / 2));
  gridBuildingAllocator->SetAttribute (
      "MinY", DoubleValue (-gridHeight * (yLength + deltaY) / 2 + deltaY / 2));
  BuildingContainer bContainer = gridBuildingAllocator->Create (gridWidth * gridHeight);

  BuildingsHelper::Install (endDevices);
  BuildingsHelper::Install (gateways);

  // Print the buildings
  if (print)
    {
      std::ofstream myfile;
      myfile.open ("buildings.txt");
      std::vector<Ptr<Building>>::const_iterator it;
      int j = 1;
      for (it = bContainer.Begin (); it != bContainer.End (); ++it, ++j)
        {
          Box boundaries = (*it)->GetBoundaries ();
          myfile << "set object " << j << " rect from " << boundaries.xMin << "," << boundaries.yMin
                 << " to " << boundaries.xMax << "," << boundaries.yMax << std::endl;
        }
      myfile.close ();
    }

  /**********************************************
   *  Set up the end device's spreading factor  *
   **********************************************/

  macHelper.SetSpreadingFactorsUp (endDevices, gateways, channel);

  NS_LOG_DEBUG ("Completed configuration");

  /*********************************************
   *  Install applications on the end devices  *
   *********************************************/

  Time appStopTime = Seconds (simulationTime);
  PeriodicSenderHelper appHelper = PeriodicSenderHelper ();
  appHelper.SetPeriod (Seconds (appPeriodSeconds));
  appHelper.SetPacketSize (23);
  
  Ptr<RandomVariableStream> rv = CreateObjectWithAttributes<UniformRandomVariable> (
      "Min", DoubleValue (0), "Max", DoubleValue (10));
  ApplicationContainer appContainer = appHelper.Install (endDevices);
  Ptr<PeriodicSender> senderApp=DynamicCast<PeriodicSender> (appContainer.Get(0));
  
////////////*cambio de rendimiento*///////////////
  senderApp->SetInterval(Seconds(100));///se necesita valores grandes para ver cambio
  senderApp->SetPacketSize(10);// no se necesita valores grandes para ver cambio
  //////////////////////////
  appContainer.Start (Seconds (0));
  appContainer.Stop (appStopTime);

  /**************************
   *  Create Network Server  *
   ***************************/

  // Create the NS node
  NodeContainer networkServer;
  networkServer.Create (1);

  // Create a NS for the network
  nsHelper.SetEndDevices (endDevices);
  nsHelper.SetGateways (gateways);
  nsHelper.Install (networkServer);

  //Create a forwarder for each gateway
  forHelper.Install (gateways);




/*
// OpenGym Env
uint16_t port = 5555;
double envStepTime = 0.3; 
  Ptr<OpenGymInterface> openGymInterface = CreateObject<OpenGymInterface> (port);
  openGymInterface->SetGetActionSpaceCb( MakeCallback (&MyGetActionSpace) );
  openGymInterface->SetGetObservationSpaceCb( MakeCallback (&MyGetObservationSpace) );
  openGymInterface->SetGetGameOverCb( MakeCallback (&MyGetGameOver) );
  openGymInterface->SetGetObservationCb( MakeCallback (&MyGetObservation) );
  openGymInterface->SetGetRewardCb( MakeCallback (&MyGetReward) );
  openGymInterface->SetGetExtraInfoCb( MakeCallback (&MyGetExtraInfo) );
  openGymInterface->SetExecuteActionsCb( MakeCallback (&MyExecuteActions) );

  Simulator::Schedule (Seconds(0.0), &ScheduleNextStateRead, envStepTime, openGymInterface);


*/




  ////////////////
  // Simulation //
  ////////////////


  Simulator::Stop (appStopTime );

  NS_LOG_INFO ("Running simulation...");
  Simulator::Run ();
  
  Simulator::Destroy ();

  ///////////////////////////
  // Print results to file //
  ///////////////////////////
  NS_LOG_INFO ("Computing performance metrics...");

  LoraPacketTracker &tracker = helper.GetPacketTracker ();
  std::cout << tracker.CountMacPacketsGlobally (Seconds (0), appStopTime ) << std::endl;
std::cout<<tracker.CountMacPacketsGloballyCpsr(Seconds (0), appStopTime )<<std::endl;
  return 0;
}