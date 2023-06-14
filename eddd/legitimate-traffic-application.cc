#include "legitimate-traffic-application.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("LegitimateTrafficApplication");
// Quantile distribution of the duration in between the flows
constexpr std::array<double, 11> DISTRIBUTION_TIME_BETWEEN_FLOWS = {0.0000006,
                                                                    0.00033,
                                                                    0.000975,
                                                                    0.017524,
                                                                    0.350172,
                                                                    2.001107,
                                                                    4.049057,
                                                                    9.000303,
                                                                    42.74497,
                                                                    115.282251,
                                                                    43164.267617};
// Quantile distribution of the duration of the flows
constexpr std::array<double, 11> DISTRIBUTION_FLOW_DURATION = {0.0000001,
                                                               0.000007,
                                                               0.000295,
                                                               0.000702,
                                                               0.001327,
                                                               0.025692,
                                                               0.23079,
                                                               1.918688,
                                                               5.053007200000001,
                                                               61.84659260000008,
                                                               120.0};
// Quantile distribution of the packets per second within flows
constexpr std::array<double, 11> DISTRIBUTION_PACKETS_PER_SECOND = {0.0166667600005226,
                                                                    0.3569863295655705,
                                                                    3.125902468664978,
                                                                    7.594707521752834,
                                                                    22.883111920335995,
                                                                    127.24669953873072,
                                                                    156.945226917058,
                                                                    422.329809725158,
                                                                    763.587786259542,
                                                                    3100.7519379845,
                                                                    6000.0};
// Quantile distribution of the packet length (payload)
constexpr std::array<double, 11> DISTRIBUTION_PACKET_LENGTH =
    {0.0, 0.0, 0.0, 0.0, 25.666666666666664, 35.0, 40.0, 45.0, 68.0, 161.0, 5647.0};

// Probability of ending a tcp connection after a flow has ended (6%)
constexpr double PROBABILITY_FLOW_ENDING_CONNECTION = 0.06;

LegitimateTrafficApplication::LegitimateTrafficApplication()
    : m_socket(nullptr),
      m_connected(false),
      m_pktSent(0),
      m_flowsStarted(0),
      m_seed(0),
      m_totBytes(0),
      m_unsentPacket(nullptr)
{
    NS_LOG_FUNCTION(this);
}

LegitimateTrafficApplication::~LegitimateTrafficApplication()
{
    NS_LOG_FUNCTION(this);
}

void
LegitimateTrafficApplication::SetSeed(uint32_t seed)
{ // Set the seed for deterministic random number generation
    NS_LOG_FUNCTION(this);
    m_seed = seed;
}

void
LegitimateTrafficApplication::SetRemote(Address remote)
{ // Set the attack target
    NS_LOG_FUNCTION(this);
    m_remote = remote;
}

// Application Methods
void
LegitimateTrafficApplication::StartApplication()
{
    NS_LOG_FUNCTION(this);

    CancelEvents();

    double waitSeconds = this->GetRandomDistributionNumber(DISTRIBUTION_TIME_BETWEEN_FLOWS, m_seed);

    std::mt19937 gen(m_seed);
    std::uniform_real_distribution<> dis(0, 1);
    double waitProgress = dis(gen);

    m_startStopEvent = Simulator::Schedule(Seconds(waitSeconds * waitProgress),
                                           &LegitimateTrafficApplication::StartSending,
                                           this);
}

void
LegitimateTrafficApplication::StopApplication() // Called at time specified by Stop
{
    NS_LOG_FUNCTION(this);

    CancelEvents();
}

void
LegitimateTrafficApplication::StartSending()
{ // Start a flow
    NS_LOG_FUNCTION(this);
    if (!m_connected)
    {
        // ScheduleNextTx() and ScheduleStopEvent() are triggered once connection is established
        ConnectSocket();
    }
    else
    {
        ScheduleNextTx();
        ScheduleStopEvent();
    }

    m_flowsStarted += 1;
}

void
LegitimateTrafficApplication::StopSending()
{ // End a flow
    NS_LOG_FUNCTION(this);
    CancelEvents();

    std::mt19937 gen(Simulator::Now().GetMilliSeconds() * m_seed);
    std::bernoulli_distribution dis(PROBABILITY_FLOW_ENDING_CONNECTION);

    bool endConnection = dis(gen);
    if (endConnection)
    {
        DisconnectSocket();
    }

    ScheduleStartEvent();
}

void
LegitimateTrafficApplication::SendPacket()
{ // Sends a packet
    NS_LOG_FUNCTION(this);

    NS_ASSERT(m_sendEvent.IsExpired());

    m_pktSize =
        static_cast<int>(this->GetRandomDistributionNumber(DISTRIBUTION_PACKET_LENGTH, m_pktSent));

    NS_LOG_INFO("Packet Size = " << m_pktSize);

    Ptr<Packet> packet;
    if (m_unsentPacket)
    {
        packet = m_unsentPacket;
    }
    else
    {
        packet = Create<Packet>(m_pktSize);
    }

    int actual = m_socket->Send(packet);

    if ((unsigned)actual == m_pktSize)
    {
        m_txTrace(packet);
        m_totBytes += m_pktSize;
        m_pktSent += 1;
        m_unsentPacket = nullptr;
    }
    else
    {
        NS_LOG_DEBUG("Unable to send packet; actual " << actual << " size " << m_pktSize
                                                      << "; caching for later attempt");
        m_unsentPacket = packet;
    }
    ScheduleNextTx();
}

void
LegitimateTrafficApplication::ScheduleStartEvent()
{ // Schedules the event to start a new flow
    NS_LOG_FUNCTION(this);

    double durationSeconds =
        this->GetRandomDistributionNumber(DISTRIBUTION_TIME_BETWEEN_FLOWS, m_flowsStarted);

    Time startTime = Seconds(durationSeconds);
    NS_LOG_LOGIC("start flow at " << startTime.As(Time::S));
    m_startStopEvent =
        Simulator::Schedule(startTime, &LegitimateTrafficApplication::StartSending, this);
}

void
LegitimateTrafficApplication::ScheduleStopEvent()
{ // Schedules the event to stop a flow
    NS_LOG_FUNCTION(this);

    double durationSeconds =
        this->GetRandomDistributionNumber(DISTRIBUTION_FLOW_DURATION, m_flowsStarted);

    Time stopTime = Seconds(durationSeconds);
    NS_LOG_LOGIC("stop flow at " << stopTime.As(Time::S));
    m_startStopEvent =
        Simulator::Schedule(stopTime, &LegitimateTrafficApplication::StopSending, this);
}

void
LegitimateTrafficApplication::ScheduleNextTx()
{ // Schedules the next packet transmission
    NS_LOG_FUNCTION(this);

    double packetsPerSecond =
        this->GetRandomDistributionNumber(DISTRIBUTION_PACKETS_PER_SECOND, m_pktSent);

    Time nextTime = Seconds((1.0 / packetsPerSecond));

    NS_LOG_LOGIC("next packet time = " << nextTime.As(Time::S));
    m_sendEvent = Simulator::Schedule(nextTime, &LegitimateTrafficApplication::SendPacket, this);
}

void
LegitimateTrafficApplication::ConnectSocket()
{ // Connect the socket
    NS_LOG_FUNCTION(this);
    // Create the socket if not already
    if (!m_socket)
    {
        m_socket = Socket::CreateSocket(GetNode(), TcpSocketFactory::GetTypeId());
    }

    m_socket->Connect(m_remote);

    m_socket->SetConnectCallback(
        MakeCallback(&LegitimateTrafficApplication::ConnectionSucceeded, this),
        MakeCallback(&LegitimateTrafficApplication::ConnectionFailed, this));
}

void
LegitimateTrafficApplication::ConnectionSucceeded(Ptr<Socket> socket)
{ // Handles the connection successful event and initiates the sending of packets and schedules a
  // stop flow event
    NS_LOG_FUNCTION(this << socket);
    m_connected = true;
    ScheduleNextTx();
    ScheduleStopEvent();
}

void
LegitimateTrafficApplication::ConnectionFailed(Ptr<Socket> socket)
{ // Abort as this means there is a misconfiguration
    NS_LOG_FUNCTION(this << socket);
    NS_FATAL_ERROR("Can't connect");
}

void
LegitimateTrafficApplication::DisconnectSocket()
{ // Disconnect the socket
    if (m_socket)
    {
        m_socket->Close();
        m_connected = false;
    }
    else
    {
        NS_LOG_WARN("LegitimateTrafficApplication found null socket to close");
    }
}

void
LegitimateTrafficApplication::CancelEvents()
{ // Cancel all pending events
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);
    Simulator::Cancel(m_startStopEvent);
    m_unsentPacket = nullptr;
}

double
LegitimateTrafficApplication::GetRandomDistributionNumber(
    std::array<double, 11> piecewiseProbability,
    uint32_t seed) const
{ // Calculate a deterministic random number based on a quantile distribution (piecewiseProbability)
  // and a seed
    std::mt19937 gen(seed * m_seed);
    std::uniform_real_distribution<> dis(0, 1);

    double u = dis(gen);

    int minIndex = static_cast<int>(u * 10);
    if (minIndex == 11)
    {
        minIndex = 10;
    }

    return piecewiseProbability.at(minIndex) +
           (piecewiseProbability.at(minIndex + 1) - piecewiseProbability.at(minIndex)) * u;
}

} // namespace ns3