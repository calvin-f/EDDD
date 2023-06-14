#include "ns3/application.h"
#include "ns3/internet-module.h"

#include <iostream>
#include <random>
#include <vector>

namespace ns3
{
/**
 * \brief NS3 application to generate ICMP flood traffic to a target based on
 * the CAIDA UCSD "DDoS Attack 2007" dataset
 * (https://www.caida.org/catalog/datasets/ddos-20070804_dataset)
 *
 */
class IcmpFloodAttacker : public Application
{
  public:
    /**
     * \brief Constructor in which the start and end time of the application are set.
     *
     * \param seed the seed for deterministic random number generation (e.g., node ID)..
     */
    IcmpFloodAttacker(uint32_t seed);
    ~IcmpFloodAttacker() override;

    /**
     * \brief Set the attack target.
     *
     * \param address The InetSocketAddress of the target.
     */
    void SetRemote(Address address);

    /**
     * \brief Get the start time of the attack of the application.
     */
    Time GetStartTime();
    /**
     * \brief Get the end time of the attack of the application.
     */
    Time GetEndTime();

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    /**
     * \brief Schedule new SYN packet send.
     */
    void ScheduleSend();
    /**
     * \brief Send ICMP packet.
     */
    void SendIcmp();

    /**
     * \brief Select a bucket based on its probability.
     *
     * \param probabilities The buckets with their probabilities.
     * \param seed The seed value for this execution.
     */
    int SelectProbabilityBucket(const std::vector<double>& probabilities, uint32_t seed);

    Time m_startTime;                //!< Application start time
    Time m_endTime;                  //!< Application end time
    Address m_remote;                //!< Attack target address
    uint16_t m_seed;                 //!< Seed for random distributions
    uint32_t m_pktSent;              //!< Number of sent packets
    EventId m_sendEvent;             //!< Event id of pending "send packet" event
    Ptr<Ipv4RawSocketImpl> m_socket; //!< Associated socket
};

} // namespace ns3
