#include "syn-flood-attacker.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("SynFloodApplication");

// Quantile distribution of packet rates for phase 1 and phase 2 of the attack
constexpr std::array<double, 11> DISTRIBUTION_PHASE1_PACKETS_PER_SECOND = {168.0,
                                                                           592.8000000000001,
                                                                           843.8,
                                                                           847.3,
                                                                           1529.2000000000007,
                                                                           2992.0,
                                                                           3537.4,
                                                                           4065.4999999999995,
                                                                           5554.000000000001,
                                                                           6515.1,
                                                                           7452.0};
constexpr std::array<double, 11> DISTRIBUTION_PHASE2_PACKETS_PER_SECOND =
    {2.0, 4.800000000000001, 6.0, 7.0, 8.0, 8.0, 9.0, 9.599999999999994, 10.0, 11.0, 14.0};

// Assumed number of clients in the original dataset
constexpr uint32_t ORIGINAL_CLIENTS_ASSUMPTION = 400;

SynFloodAttacker::SynFloodAttacker()
    : m_seed(0),
      m_pktSent(0),
      m_socket(nullptr)
{
    NS_LOG_FUNCTION(this);
}

SynFloodAttacker::~SynFloodAttacker()
{
    NS_LOG_FUNCTION(this);
}

void
SynFloodAttacker::SetSeed(uint32_t seed)
{ // Base seed for the specific instance for all stochastic decisions
    NS_LOG_FUNCTION(this);

    m_seed = seed;
}

void
SynFloodAttacker::SetRemote(Ipv4Address ip, uint16_t port)
{ // Target of the attack
    NS_LOG_FUNCTION(this);

    m_remoteIp = ip;
    m_remotePort = port;
}

void
SynFloodAttacker::StartApplication()
{
    NS_LOG_FUNCTION(this);

    m_socket = CreateObject<Ipv4RawSocketImpl>();
    m_socket->SetNode(GetNode());
    m_socket->SetAttribute("Protocol", UintegerValue(6));

    StartPhase1();
}

void
SynFloodAttacker::StopApplication()
{
    NS_LOG_FUNCTION(this);

    if (m_socket)
    {
        m_socket->Close();
    }
    CancelEvents();
}

void
SynFloodAttacker::StartPhase1()
{ // Starting the first phase of the attack
    NS_LOG_FUNCTION(this);

    m_phase1 = true;

    Time nextTime = CalculateNextTime(DISTRIBUTION_PHASE1_PACKETS_PER_SECOND);

    std::mt19937 gen(m_seed);
    std::uniform_real_distribution<> dis(0, 1);
    double waitProgress = dis(gen);

    m_sendEvent = Simulator::Schedule(nextTime * waitProgress, &SynFloodAttacker::SendSyn, this);

    m_startStopEvent = Simulator::Schedule(Seconds(5), &SynFloodAttacker::EndPhase1, this);
}

void
SynFloodAttacker::EndPhase1()
{ // Ending the first phase of the attack
    NS_LOG_FUNCTION(this);

    CancelEvents();
    m_phase1 = false;

    m_startStopEvent = Simulator::Schedule(Seconds(10), &SynFloodAttacker::StartPhase2, this);
}

void
SynFloodAttacker::StartPhase2()
{ // Starting the second phase of the attack
    NS_LOG_FUNCTION(this);

    CancelEvents();

    Time nextTime = CalculateNextTime(DISTRIBUTION_PHASE2_PACKETS_PER_SECOND);

    std::mt19937 gen(m_seed);
    std::uniform_real_distribution<> dis(0, 1);
    double waitProgress = dis(gen);

    m_sendEvent = Simulator::Schedule(nextTime * waitProgress, &SynFloodAttacker::SendSyn, this);

    m_startStopEvent = Simulator::Schedule(Seconds(10), &SynFloodAttacker::EndPhase2, this);
}

void
SynFloodAttacker::EndPhase2()
{ // Ending the second phase of the attack
    NS_LOG_FUNCTION(this);

    CancelEvents();
}

void
SynFloodAttacker::ScheduleSend()
{ // Schedules a new send SYN packet event
    NS_LOG_FUNCTION(this);

    Time nextTime;
    if (m_phase1)
    {
        nextTime = CalculateNextTime(DISTRIBUTION_PHASE1_PACKETS_PER_SECOND);
    }
    else
    {
        nextTime = CalculateNextTime(DISTRIBUTION_PHASE2_PACKETS_PER_SECOND);
    }

    NS_LOG_LOGIC("next packet time = " << nextTime.As(Time::S));
    m_sendEvent = Simulator::Schedule(nextTime, &SynFloodAttacker::SendSyn, this);
}

void
SynFloodAttacker::SendSyn()
{ // Sends a SYN packet
    NS_LOG_FUNCTION(this);

    // Create a TCP header with the SYN flag set
    TcpHeader tcpHeader;
    tcpHeader.SetFlags(TcpHeader::SYN);
    tcpHeader.SetSourcePort(GetRandomPort());
    tcpHeader.SetDestinationPort(m_remotePort);

    // Create an empty packet
    Ptr<Packet> synPacket = Create<Packet>();

    // Add the header to the packet
    synPacket->AddHeader(tcpHeader);

    // Send the SYN packet using the raw socket
    m_socket->SendTo(synPacket, 0, InetSocketAddress(m_remoteIp, m_remotePort));

    m_pktSent += 1;

    ScheduleSend();
}

void
SynFloodAttacker::CancelEvents()
{ // Cancel all pending events
    NS_LOG_FUNCTION(this);

    Simulator::Cancel(m_sendEvent);
    Simulator::Cancel(m_startStopEvent);
}

uint16_t
SynFloodAttacker::GetRandomPort() const
{ // Selection of a deterministic random port in the range of 1024 to 65535
    std::mt19937 gen(m_pktSent * m_seed);
    std::uniform_int_distribution<uint16_t> dis(1024, 65535);

    uint16_t randomPort = dis(gen);

    return randomPort;
}

Time
SynFloodAttacker::CalculateNextTime(const std::array<double, 11>& packetPerSecondProbability) const
{ // Calculation of next time a packet should be sent based on quantile distribution of packets per
  // second at target
    double packetsPerSecond = GetRandomDistributionNumber(packetPerSecondProbability, m_pktSent);
    return Seconds((1.0 / packetsPerSecond) * ORIGINAL_CLIENTS_ASSUMPTION);
}

double
SynFloodAttacker::GetRandomDistributionNumber(std::array<double, 11> piecewiseProbability,
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
