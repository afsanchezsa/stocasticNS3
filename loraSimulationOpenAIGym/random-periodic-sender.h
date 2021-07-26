/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
  Class based on PeriodicSender of the lorawan module.
  This adds a new attribute to send packets following a random variable.
 */

#ifndef PERIODIC_SENDER_H
#define PERIODIC_SENDER_H

#include "ns3/application.h"
#include "ns3/nstime.h"
#include "ns3/lorawan-mac.h"
#include "ns3/attribute.h"

namespace ns3 {
namespace lorawan {

class RandomPeriodicSender : public Application
{
public:
  RandomPeriodicSender ();
  ~RandomPeriodicSender ();

  static TypeId GetTypeId (void);

  /**
   * Set the sending interval
   * \param interval the interval between two packet sendings
   */
  void SetInterval(Time interval);

  /**
   * Set the sending random interval
   * \param interval the interval between two packet sendings
   */
  void SetIntervalRandomVariable(Ptr<RandomVariableStream> intervalRand);

  /**
   * Get the sending inteval
   * \returns the interval between two packet sends
   */
  Time GetInterval (void) const;

  /**
   * Set the initial delay of this application
   */
  void SetInitialDelay (Time delay);

  /**
   * Set packet size
   */
  void SetPacketSize (uint8_t size);

  /**
   * Set if using randomness in the packet size
   */
  void SetPacketSizeRandomVariable (Ptr<RandomVariableStream> rv);

  /**
   * Send a packet using the LoraNetDevice's Send method
   */
  void SendPacket (void);

  /**
   * Start the application by scheduling the first SendPacket event
   */
  void StartApplication (void);

  /**
   * Stop the application
   */
  void StopApplication (void);

private:
  /**
   * The interval between to consecutive send events
   */
  Time m_interval;

  /**
   * The initial delay of this application
   */
  Time m_initialDelay;

  /**
   * The sending event scheduled as next
   */
  EventId m_sendEvent;

  /**
   * The MAC layer of this node
   */
  Ptr<LorawanMac> m_mac;

  /**
   * The packet size.
   */
  uint8_t m_basePktSize;


  /**
   * The random variable that adds bytes to the packet size
   */
  Ptr<RandomVariableStream> m_pktSizeRV;

  /**
   * The random variable that sends packets with a random random variable distribution
   */
  Ptr<RandomVariableStream> m_pktIntervalRV;

};

} //namespace ns3

}
#endif /* SENDER_APPLICATION */
