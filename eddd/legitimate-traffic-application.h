#include "ns3/applications-module.h"
#include "ns3/internet-module.h"

#include <iostream>
#include <random>

namespace ns3
{
/**
 * \brief NS3 application to generate legitimate traffic to a target based on CIC flow data
 * (https://www.kaggle.com/datasets/devendra416/ddos-datasets)
 *
 */
class LegitimateTrafficApplication : public Application
{
  public:
    LegitimateTrafficApplication();
    ~LegitimateTrafficApplication() override;

    /**
     * \brief Set the seed for deterministic random number generation.
     *
     * \param seed The seed value (e.g., node ID).
     */
    void SetSeed(uint32_t seed);

    /**
     * \brief Set the attack target.
     *
     * \param address The target InetSocketAddress.
     */
    void SetRemote(Address address);

  private:
    void StartApplication() override;
    void StopApplication() override;

    /**
     * \brief Start a flow
     */
    void StartSending();
    /**
     * \brief End a flow
     */
    void StopSending();
    /**
     * \brief Send a packet
     */
    void SendPacket();

    /**
     * \brief Schedule the next flow start
     */
    void ScheduleStartEvent();
    /**
     * \brief Schedule the next flow end
     */
    void ScheduleStopEvent();
    /**
     * \brief Schedule the next packet transmission
     */
    void ScheduleNextTx();

    /**
     * \brief Connect the socket
     */
    void ConnectSocket();
    /**
     * \brief Disconnect the socket
     */
    void DisconnectSocket();
    /**
     * \brief Handle a Connection Succeed event
     * \param socket the connected socket
     */
    void ConnectionSucceeded(Ptr<Socket> socket);
    /**
     * \brief Handle a Connection Failed event
     * \param socket the not connected socket
     */
    void ConnectionFailed(Ptr<Socket> socket);

    /**
     * \brief Cancel all pending events.
     */
    void CancelEvents();

    /**
     * \brief Calculate a deterministic random number based on a distribution
     *
     * \param piecewiseProbability The probability at quantiles 0%, 10%, ..., 100%.
     * \param seed The seed value for this execution.
     */
    double GetRandomDistributionNumber(std::array<double, 11> piecewiseProbability,
                                       uint32_t seed) const;

    Ptr<Socket> m_socket;       //!< Associated socket
    Address m_remote;           //!< Attack target address
    bool m_connected;           //!< True if connected
    uint32_t m_pktSize;         //!< Size of packets
    uint32_t m_pktSent;         //!< Number of sent packets
    uint32_t m_flowsStarted;    //!< Number of started flows
    uint32_t m_seed;            //!< Seed for random distributions
    uint64_t m_totBytes;        //!< Total bytes sent so far
    EventId m_startStopEvent;   //!< Event id for next start or stop event
    EventId m_sendEvent;        //!< Event id of pending "send packet" event
    Ptr<Packet> m_unsentPacket; //!< Unsent packet cached for future attempt

    /// Traced Callback: transmitted packets.
    TracedCallback<Ptr<const Packet>> m_txTrace;

    /// Callbacks for tracing the packet Tx events, includes source and destination addresses
    TracedCallback<Ptr<const Packet>, const Address&, const Address&> m_txTraceWithAddresses;

    /// Callback for tracing the packet Tx events, includes source, destination, the packet sent,
    /// and header
    TracedCallback<Ptr<const Packet>, const Address&, const Address&, const SeqTsSizeHeader&>
        m_txTraceWithSeqTsSize;
};

} // namespace ns3