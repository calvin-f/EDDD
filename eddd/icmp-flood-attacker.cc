#include "icmp-flood-attacker.h"

namespace ns3
{

NS_LOG_COMPONENT_DEFINE("IcmpFloodApplication");

IcmpFloodAttacker::IcmpFloodAttacker(uint32_t seed)
    : m_seed(seed),
      m_pktSent(0),
      m_socket(nullptr)
{
    NS_LOG_FUNCTION(this);

    std::vector<double> start_5min_buckets_probabilities = {0.014069380302867576,
                                                            0.0003221995489206315,
                                                            0.0047255933841692625,
                                                            0.000429599398560842,
                                                            0.0001073998496402105,
                                                            0.8921705509612287,
                                                            0.0235205670712061,
                                                            0.01073998496402105,
                                                            0.018580173987756417,
                                                            0.006766190527333261,
                                                            0.006229191279132209,
                                                            0.01492857909998926,
                                                            0.005906991730211577,
                                                            0.001503597894962947};
    int startBucket = SelectProbabilityBucket(start_5min_buckets_probabilities, m_seed);

    std::mt19937 gen(m_seed * 2);
    std::uniform_real_distribution<double> interBucketTime(0, 1);

    Time clientStart = (startBucket + interBucketTime(gen)) * Minutes(5);

    std::vector<double> duration_5min_buckets_probabilities = {0.14681559445816775,
                                                               0.0735688970035442,
                                                               0.03898614541939641,
                                                               0.043819138653205886,
                                                               0.03565675008054989,
                                                               0.023842766620126733,
                                                               0.03017935774889915,
                                                               0.03705294812587263,
                                                               0.5690044033938353,
                                                               0.001073998496402105};
    int durationBucket = SelectProbabilityBucket(duration_5min_buckets_probabilities, m_seed * 3);

    Time clientEnd = clientStart + ((durationBucket + interBucketTime(gen)) * Minutes(5));

    m_startTime = clientStart;
    m_endTime = clientEnd;
}

IcmpFloodAttacker::~IcmpFloodAttacker()
{
    NS_LOG_FUNCTION(this);
}

void
IcmpFloodAttacker::SetRemote(Address address)
{ // Target of the attack
    NS_LOG_FUNCTION(this);
    m_remote = address;
}

Time
IcmpFloodAttacker::GetStartTime()
{ // Getter for the start time of the application
    return m_startTime;
}

Time
IcmpFloodAttacker::GetEndTime()
{ // Getter for the end time of the application
    return m_endTime;
}

void
IcmpFloodAttacker::StartApplication()
{ // Start application and setup socket
    NS_LOG_FUNCTION(this);
    m_socket = CreateObject<Ipv4RawSocketImpl>();
    m_socket->SetNode(GetNode());
    m_socket->SetAttribute("Protocol", UintegerValue(1)); // ICMP

    SendIcmp();
}

void
IcmpFloodAttacker::StopApplication()
{
    NS_LOG_FUNCTION(this);
    if (m_socket)
    {
        m_socket->Close();
    }
    Simulator::Cancel(m_sendEvent);
}

void
IcmpFloodAttacker::ScheduleSend()
{ // Schedules a new send ICMP packet event
    NS_LOG_FUNCTION(this);

    std::mt19937 gen(m_seed * m_pktSent);

    std::uniform_real_distribution<> dis(17, 23);

    double attackRate = dis(gen);
    Time nextTime = Seconds(1.0 / attackRate);

    NS_LOG_LOGIC("next packet time = " << nextTime.As(Time::S));

    // Schedule the next ICMP packet
    m_sendEvent = Simulator::Schedule(nextTime, &IcmpFloodAttacker::SendIcmp, this);
}

void
IcmpFloodAttacker::SendIcmp()
{ // Send ICMP packet according to ping.cc
    NS_LOG_FUNCTION(this);

    Ptr<Packet> dataPacket = Create<Packet>(56);

    // Create an empty packet
    Ptr<Packet> icmpPacket = Create<Packet>();

    // Using IPv4
    Icmpv4Echo echo;

    // In the Icmpv4Echo the payload is part of the header.
    echo.SetData(dataPacket);

    icmpPacket->AddHeader(echo);
    Icmpv4Header header;
    header.SetType(Icmpv4Header::ICMPV4_ECHO);
    header.SetCode(0);
    icmpPacket->AddHeader(header);

    // Send the Icmp packet using the raw socket
    m_socket->SendTo(icmpPacket, 0, m_remote);

    m_pktSent++;

    ScheduleSend();
}

// Selects an index based on the given probabilities
int
IcmpFloodAttacker::SelectProbabilityBucket(const std::vector<double>& probabilities, uint32_t seed)
{
    std::mt19937 gen(seed);
    std::discrete_distribution<int> dist(probabilities.begin(), probabilities.end());
    return dist(gen);
}

} // namespace ns3
