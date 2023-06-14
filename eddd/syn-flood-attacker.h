#include "ns3/application.h"
#include "ns3/internet-module.h"
#include <iostream>
#include <random>

namespace ns3
{
/**
 * \brief NS3 application to generate SYN flood traffic to a target based on
 * pkt.TCP.synflood.spoofed.pcap of the StopDDoS dataset
 * (https://github.com/StopDDoS/packet-captures)
 *
 */
class SynFloodAttacker : public Application
{
  public:
    SynFloodAttacker();
    ~SynFloodAttacker() override;

    /**
     * \brief Set the seed for deterministic random number generation.
     *
     * \param seed The seed value (e.g., node ID).
     */
    void SetSeed(uint32_t);

    /**
     * \brief Set the attack target.
     *
     * \param ip The IP address.
     * \param port The port number.
     */
    void SetRemote(Ipv4Address ip, uint16_t port);

  protected:
    void StartApplication() override;
    void StopApplication() override;

  private:
    /**
     * \brief Start attack phase 1.
     */
    void StartPhase1();

    /**
     * \brief End attack phase 1.
     */
    void EndPhase1();
    /**
     * \brief Start attack phase 2.
     */
    void StartPhase2();
    /**
     * \brief End attack phase 2.
     */
    void EndPhase2();

    /**
     * \brief Schedule new SYN packet send.
     */
    void ScheduleSend();
    /**
     * \brief Send SYN packet.
     */
    void SendSyn();

    /**
     * \brief Cancel all pending events.
     */
    void CancelEvents();

    /**
     * \brief Selection of a deterministic random port different after every packet sent
     */
    uint16_t GetRandomPort() const;
    /**
     * \brief Selection of a deterministic random port different after every packet sent
     *
     * \param piecewiseProbability The probability at quantiles 0%, 10%, ..., 100%.
     */
    Time CalculateNextTime(const std::array<double, 11>& packetPerSecondProbability) const;

    /**
     * \brief Calculate a deterministic random number based on a distribution
     *
     * \param piecewiseProbability The probability at quantiles 0%, 10%, ..., 100%.
     * \param seed The seed value for this execution.
     */
    double GetRandomDistributionNumber(std::array<double, 11> piecewiseProbability,
                                       uint32_t seed) const;

    Ipv4Address m_remoteIp;   //!< Attack target ip
    uint16_t m_remotePort;    //!< Attack target port
    bool m_phase1;            //!< Flag whether phase 1 or 2 is active
    uint32_t m_seed;          //!< Specific seed for application specific deterministic randomness
    uint32_t m_pktSent;       //!< Number of sent packets
    EventId m_sendEvent;      //!< Event id of pending "send packet" event
    EventId m_startStopEvent; //!< Event id for next start or end phase event
    Ptr<Ipv4RawSocketImpl> m_socket; //!< Associated socket
};

} // namespace ns3
