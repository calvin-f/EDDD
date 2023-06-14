#include "chrono"
#include "icmp-flood-attacker.h"
#include "json.hpp"
#include "legitimate-traffic-application.h"
#include "legitimate-traffic-helper.h"
#include "network-topology-creator.h"
#include "syn-flood-attacker.h"

#include "ns3/applications-module.h"
#include "ns3/bulk-send-application.h"
#include "ns3/callback.h"
#include "ns3/core-module.h"
#include "ns3/internet-apps-module.h"
#include "ns3/internet-module.h"
#include "ns3/mpi-interface.h"
#include "ns3/network-module.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/packet-sink.h"
#include "ns3/point-to-point-module.h"
#include "ns3/point-to-point-star.h"
#include "ns3/tag.h"
#include "ns3/v4ping-helper.h"
#include "ns3/v4ping.h"

#include <iostream>
#include <random>

using namespace ns3;

NS_LOG_COMPONENT_DEFINE("EDDDLog");

int
main(int argc, char* argv[])
{
    // Set up command-line arguments, if needed
    double networkFactor = 0.01;
    double clientFactor = 0.01;
    double backboneLatencyFactor = 1.25;
    double regionalLatencyFactor = 2.5;
    double localLatencyFactor = 3.75;
    double backboneBandwidthGbps = 1000;
    double regionalBandwidthGbps = 100;
    double localBandwidthGbps = 10;
    double clientBandwidthGbps = 1;
    std::string country;
    bool logPings = false;
    bool logGraphviz = false;
    std::string attackType = "syn-flood";

    CommandLine cmd;
    cmd.AddValue("networkFactor",
                 "How many of the subnet routers are represented [0,1]",
                 networkFactor);
    cmd.AddValue("clientFactor",
                 "How many of clients in an access net are represented [0,1]",
                 clientFactor);
    cmd.AddValue("backboneLatencyFactor",
                 "How many of clients in an access net are represented [0,1]",
                 backboneLatencyFactor);
    cmd.AddValue("regionalLatencyFactor",
                 "How many of clients in an access net are represented [0,1]",
                 regionalLatencyFactor);
    cmd.AddValue("localLatencyFactor",
                 "How many of clients in an access net are represented [0,1]",
                 localLatencyFactor);
    cmd.AddValue("backboneBandwidthGbps",
                 "Bandwidth between backbone routers",
                 backboneBandwidthGbps);
    cmd.AddValue("regionalBandwidthGbps",
                 "Bandwidth between regional routers",
                 regionalBandwidthGbps);
    cmd.AddValue("localBandwidthGbps", "Bandwidth between local routers", localBandwidthGbps);
    cmd.AddValue("clientBandwidthGbps",
                 "Bandwidth between local routers and their clients",
                 clientBandwidthGbps);
    cmd.AddValue("country", "Countries that are represented in the network", country);
    cmd.AddValue("logPings", "Log the ping from each client to the target.", logPings);
    cmd.AddValue("logGraphviz", "Log backbone nodes distribution in graphviz format.", logGraphviz);
    cmd.AddValue("attackType", "DDoS Attack Type (syn-flood; icmp-flood).", attackType);

    cmd.Parse(argc, argv);

    nlohmann::json j = nlohmann::json::parse(country);

    NetworkTopologyCreator::Country parsedCountries;
    j.get_to(parsedCountries);

    MpiInterface::Enable(&argc, &argv);

    // Set TCP socket buffer size
    Config::SetDefault("ns3::TcpSocket::SndBufSize", UintegerValue(60000));
    Config::SetDefault("ns3::TcpSocket::RcvBufSize", UintegerValue(60000));

    // Logging
    LogComponentEnable("EDDDLog", LOG_LEVEL_INFO);
    LogComponentEnable("LegitimateTrafficApplication", LOG_LEVEL_WARN);
    LogComponentEnable("NetworkTopologyCreator", LOG_LEVEL_INFO);

    NS_LOG_INFO("Start building network (rank: " << MpiInterface::GetSystemId() << ")");
    // Record the startTimeNetworkBuild time
    auto startTimeNetworkBuild = std::chrono::high_resolution_clock::now();

    // Setup network topology
    NetworkTopologyCreator topologyCreator;
    topologyCreator.SetNetworkSpeedFactors(backboneLatencyFactor,
                                           regionalLatencyFactor,
                                           localLatencyFactor);
    topologyCreator.SetNetworkBandwidth(backboneBandwidthGbps,
                                        regionalBandwidthGbps,
                                        localBandwidthGbps,
                                        clientBandwidthGbps);
    topologyCreator.SetRepresentationFactors(networkFactor, clientFactor);
    if (logPings)
    {
        topologyCreator.ListPings();
    }
    if (logGraphviz)
    {
        topologyCreator.EnableBackboneGraphviz();
    }
    topologyCreator.CreateNetwork(parsedCountries);

    NodeContainer legitimateClients = topologyCreator.GetLegitimateClients();
    NodeContainer attackingClients = topologyCreator.GetAttackingClients();

    Ptr<Node> targetNode = topologyCreator.GetTargetNode();
    Ipv4Address targetAddress = topologyCreator.GetTargetAddress();

    // Initialize the global end time to
    Time endTime = Hours(0);

    // Install attack traffic application on attacking clients
    for (uint32_t i = 0; i < attackingClients.GetN(); ++i)
    {
        if (attackType == "syn-flood")
        {
            Ptr<SynFloodAttacker> attackerApp = CreateObject<SynFloodAttacker>();
            attackerApp->SetRemote(targetAddress, 8080);
            attackerApp->SetSeed(i);
            attackerApp->SetStartTime(Seconds(0));
            attackerApp->SetStopTime(Seconds(80));

            attackingClients.Get(i)->AddApplication(attackerApp);

            endTime = Seconds(80);
        }
        else
        {
            Ptr<IcmpFloodAttacker> attackerApp = CreateObject<IcmpFloodAttacker>(i);
            attackerApp->SetRemote(InetSocketAddress(targetAddress, 8080));
            Time clientStart = attackerApp->GetStartTime();
            Time clientEnd = attackerApp->GetEndTime();
            attackerApp->SetStartTime(clientStart);
            attackerApp->SetStopTime(clientEnd);

            attackingClients.Get(i)->AddApplication(attackerApp);

            if (clientEnd > endTime)
            {
                endTime = clientEnd;
            }
        }
    }

    // Install the sink application on the target node
    PacketSinkHelper sinkHelper("ns3::TcpSocketFactory",
                                InetSocketAddress(Ipv4Address::GetAny(), 8080));
    ApplicationContainer sinkApp = sinkHelper.Install(targetNode);
    sinkApp.Start(Seconds(0));
    sinkApp.Stop(endTime);

    // Install legitimate traffic application on legitimate clients
    for (uint32_t i = 0; i < legitimateClients.GetN(); ++i)
    {
        Ptr<LegitimateTrafficApplication> legitimateTrafficApp =
            CreateObject<LegitimateTrafficApplication>();
        legitimateTrafficApp->SetRemote(InetSocketAddress(targetAddress, 8080));
        legitimateTrafficApp->SetSeed(i);

        legitimateClients.Get(i)->AddApplication(legitimateTrafficApp);

        legitimateTrafficApp->SetStartTime(Seconds(0));
        legitimateTrafficApp->SetStopTime(endTime);
    }

    auto endTimeNetworkBuild = std::chrono::high_resolution_clock::now();
    auto durationNetworkBuild = std::chrono::duration_cast<std::chrono::microseconds>(
        endTimeNetworkBuild - startTimeNetworkBuild);
    NS_LOG_INFO("Finished building network within " << (durationNetworkBuild.count() / 1000)
                                                    << " ms (rank: " << MpiInterface::GetSystemId()
                                                    << ")");

    NS_LOG_INFO("Starting simulation (rank: " << MpiInterface::GetSystemId() << ")");
    auto startTimeSimulation = std::chrono::high_resolution_clock::now();
    // Run the simulation
    Simulator::Run();
    auto endTimeSimulation = std::chrono::high_resolution_clock::now();
    auto durationSimulation = std::chrono::duration_cast<std::chrono::microseconds>(
        endTimeSimulation - startTimeSimulation);
    NS_LOG_INFO("Finished simulation within " << (durationSimulation.count() / 1000)
                                              << " ms (rank: " << MpiInterface::GetSystemId()
                                              << ")");
    Simulator::Destroy();

    MpiInterface::Disable();
    return 0;
}