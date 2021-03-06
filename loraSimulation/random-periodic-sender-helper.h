/*-*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
  Application helper based on PeriodicSenderHelper of the lorawan module.
  This adds a new attribute to send packets following a random variable.
 */

#ifndef PERIODIC_SENDER_HELPER_H
#define PERIODIC_SENDER_HELPER_H
#include "ns3/object-factory.h"
#include "ns3/address.h"
#include "ns3/attribute.h"
#include "ns3/net-device.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "random-periodic-sender.h"
#include <stdint.h>
#include <string>

namespace ns3
{
  namespace lorawan
  {

    /**
     *This class can be used to install PeriodicSender applications on a wide
     *range of nodes.
     */
    class RandomPeriodicSenderHelper
    {
      public:
      RandomPeriodicSenderHelper();

      ~RandomPeriodicSenderHelper();

      void SetAttribute(std::string name, const AttributeValue &value);

      ApplicationContainer Install(NodeContainer c) const;

      ApplicationContainer Install(Ptr<Node> node) const;

      /**
       *Set the period to be used by the applications created by this helper.
       *
       *A value of Seconds (0) results in randomly generated periods according to
       *the model contained in the TR 45.820 document.
       *
       *\param period The period to set
       */
      void SetPeriod(Time period);

      void SetPacketSizeRandomVariable(Ptr<RandomVariableStream> rv);

      void SetPeriodRandomVariable(Ptr<RandomVariableStream> periodRand);

      void SetPacketSize(uint8_t size);

      private:
        Ptr<Application> InstallPriv(Ptr<Node> node) const;

      ObjectFactory m_factory;

      Ptr<UniformRandomVariable> m_initialDelay;

      Ptr<UniformRandomVariable> m_intervalProb;

      Time m_period;	//!< The period with which the application will be set to send
     	// messages

      Ptr<RandomVariableStream> m_pktSizeRV;	// whether or not a random component is added to the packet size

      Ptr<RandomVariableStream> m_pktPeriodRV;	// whether or not a random component is added to the packet size

      uint8_t m_pktSize;	// the packet size.

    };
  }	// namespace ns3
}

#endif /*PERIODIC_SENDER_HELPER_H */