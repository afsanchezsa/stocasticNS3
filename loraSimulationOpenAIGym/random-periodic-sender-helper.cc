/*-*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
  Application helper based on PeriodicSenderHelper of the lorawan module.
  This adds a new attribute to send packets following a random variable.
 */
#include "random-periodic-sender-helper.h"
#include "random-periodic-sender.h"
#include "ns3/random-variable-stream.h"
#include "ns3/double.h"
#include "ns3/string.h"
#include "ns3/trace-source-accessor.h"
#include "ns3/simulator.h"
#include "ns3/log.h"

namespace ns3
{
  namespace lorawan
  {

    NS_LOG_COMPONENT_DEFINE("RandomPeriodicSenderHelper");

    RandomPeriodicSenderHelper::RandomPeriodicSenderHelper()
    {
      m_factory.SetTypeId("ns3::RandomPeriodicSender");

      m_initialDelay = CreateObject<UniformRandomVariable>();
      m_initialDelay->SetAttribute("Min", DoubleValue(0));

      m_pktSize = 10;
      m_pktSizeRV = 0;
      m_pktPeriodRV = 0;
    }

    RandomPeriodicSenderHelper::~RandomPeriodicSenderHelper() {}

    void
    RandomPeriodicSenderHelper::SetAttribute(std::string name, const AttributeValue &value)
    {
      m_factory.Set(name, value);
    }

    ApplicationContainer
    RandomPeriodicSenderHelper::Install(Ptr<Node> node) const
    {
      return ApplicationContainer(InstallPriv(node));
    }

    ApplicationContainer
    RandomPeriodicSenderHelper::Install(NodeContainer c) const
    {
      ApplicationContainer apps;
      for (NodeContainer::Iterator i = c.Begin(); i != c.End(); ++i)
      {
        apps.Add(InstallPriv(*i));
      }

      return apps;
    }

    Ptr < Application>
      RandomPeriodicSenderHelper::InstallPriv(Ptr<Node> node) const {
        NS_LOG_FUNCTION(this << node);

        Ptr<RandomPeriodicSender> app = m_factory.Create<RandomPeriodicSender> ();

        Time interval;
        if (m_pktPeriodRV) {
          app->SetIntervalRandomVariable(m_pktPeriodRV);
          double intervalRand = m_pktPeriodRV->GetValue();
          interval = Seconds(intervalRand);
        } else {
          interval = m_period;
        }

        app->SetInterval(interval);
        NS_LOG_DEBUG("Created an application with interval = " <<
          interval.GetHours() << " hours");

        app->SetInitialDelay(Seconds(m_initialDelay->GetValue(0, interval.GetSeconds())));
        app->SetPacketSize(m_pktSize);
        if (m_pktSizeRV) {
          app->SetPacketSizeRandomVariable(m_pktSizeRV);
        }

        app->SetNode(node);
        node->AddApplication(app);

        return app;
      }

    void
    RandomPeriodicSenderHelper::SetPeriod(Time period)
    {
      m_period = period;
    }

    void
    RandomPeriodicSenderHelper::SetPacketSizeRandomVariable(Ptr<RandomVariableStream> rv)
    {
      m_pktSizeRV = rv;
    }

    void
    RandomPeriodicSenderHelper::SetPeriodRandomVariable(Ptr<RandomVariableStream> periodRand)
    {
      m_pktPeriodRV = periodRand;
    }

    void
    RandomPeriodicSenderHelper::SetPacketSize(uint8_t size)
    {
      m_pktSize = size;
    }
  }
}	// namespace ns3