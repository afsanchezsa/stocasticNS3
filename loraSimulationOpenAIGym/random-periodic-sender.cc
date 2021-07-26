/*-*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
  Class based on PeriodicSender of the lorawan module.
  This adds a new attribute to send packets following a random variable.
 */
#include "random-periodic-sender.h"
#include "ns3/pointer.h"
#include "ns3/log.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/lora-net-device.h"

namespace ns3
{
  namespace lorawan
  {

    NS_LOG_COMPONENT_DEFINE("RandomPeriodicSender");

    NS_OBJECT_ENSURE_REGISTERED(RandomPeriodicSender);

    TypeId
    RandomPeriodicSender::GetTypeId(void)
    {
      static TypeId tid = TypeId("ns3::RandomPeriodicSender")
        .SetParent<Application> ()
        .AddConstructor<RandomPeriodicSender> ()
        .SetGroupName("lorawan")
        .AddAttribute("Interval", "The interval between packet sends of this app",
          TimeValue(Seconds(0)),
          MakeTimeAccessor(&RandomPeriodicSender::GetInterval, &RandomPeriodicSender::SetInterval),
          MakeTimeChecker());
      return tid;
    }

    RandomPeriodicSender::RandomPeriodicSender(): m_interval(Seconds(10)),
      m_initialDelay(Seconds(1)),
      m_basePktSize(10),
      m_pktSizeRV(0)

    {
      NS_LOG_FUNCTION_NOARGS();
    }

    RandomPeriodicSender::~RandomPeriodicSender()
    {
      NS_LOG_FUNCTION_NOARGS();
    }

    void
    RandomPeriodicSender::SetInterval(Time interval)
    {
      NS_LOG_FUNCTION(this << interval);
      m_interval = interval;
    }

    Time
    RandomPeriodicSender::GetInterval(void) const
    {
      NS_LOG_FUNCTION(this);
      return m_interval;
    }

    void
    RandomPeriodicSender::SetInitialDelay(Time delay)
    {
      NS_LOG_FUNCTION(this << delay);
      m_initialDelay = delay;
    }

    void
    RandomPeriodicSender::SetPacketSizeRandomVariable(Ptr<RandomVariableStream> rv)
    {
      m_pktSizeRV = rv;
    }

    void
    RandomPeriodicSender::SetIntervalRandomVariable(Ptr<RandomVariableStream> intervalRand)
    {
      m_pktIntervalRV = intervalRand;
    }

    void
    RandomPeriodicSender::SetPacketSize(uint8_t size)
    {
      m_basePktSize = size;
    }

    void
    RandomPeriodicSender::SendPacket(void)
    {
      NS_LOG_FUNCTION(this);

     	// Create and send a new packet
      Ptr<Packet> packet;
      if (m_pktSizeRV)
      {
        int randomsize = m_pktSizeRV->GetInteger();
        packet = Create<Packet> (m_basePktSize + randomsize);
      }
      else
      {
        packet = Create<Packet> (m_basePktSize);
      }
      m_mac->Send(packet);

     	// Schedule the next SendPacket event

      Time interval = m_interval;

      if (m_pktIntervalRV)
      {
        double randomInterval = m_pktIntervalRV->GetValue();
        interval = Seconds(randomInterval);
      }
      NS_LOG_DEBUG("Generated packet interval " << interval.GetSeconds());

      m_sendEvent = Simulator::Schedule(interval, &RandomPeriodicSender::SendPacket, this);

      NS_LOG_INFO("Sent a packet of size " << packet->GetSize());
    }

    void
    RandomPeriodicSender::StartApplication(void)
    {
      NS_LOG_FUNCTION(this);

     	// Make sure we have a MAC layer
      if (m_mac == 0)
      {
       	// Assumes there's only one device
        Ptr<LoraNetDevice> loraNetDevice = m_node->GetDevice(0)->GetObject<LoraNetDevice> ();

        m_mac = loraNetDevice->GetMac();
        NS_ASSERT(m_mac != 0);
      }

     	// Schedule the next SendPacket event
      Simulator::Cancel(m_sendEvent);
      NS_LOG_DEBUG("Starting up application with a first event with a " <<
        m_initialDelay.GetSeconds() << " seconds delay");
      m_sendEvent = Simulator::Schedule(m_initialDelay, &RandomPeriodicSender::SendPacket, this);
      NS_LOG_DEBUG("Event Id: " << m_sendEvent.GetUid());
    }

    void
    RandomPeriodicSender::StopApplication(void)
    {
      NS_LOG_FUNCTION_NOARGS();
      Simulator::Cancel(m_sendEvent);
    }
  }
}