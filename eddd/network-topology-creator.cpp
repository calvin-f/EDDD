#include "network-topology-creator.h"

NS_LOG_COMPONENT_DEFINE("NetworkTopologyCreator");

using namespace ns3;

constexpr double MAX_FIBER_SPEED_KM_PER_MS = 0.005;

NetworkTopologyCreator::NetworkTopologyCreator()
    : m_legitimateClients(),
      m_attackingClients(),
      m_targetNode(nullptr),
      m_targetAddress(Ipv4Address::GetZero()),
      m_backbone_speed_km_per_ms(0.005 * 1.25),
      m_regional_speed_km_per_ms(0.005 * 2.5),
      m_local_speed_km_per_ms(0.005 * 3.75),
      m_backbone_bandwidth_gbps(1000),
      m_regional_bandwidth_gbps(100),
      m_local_bandwidth_gbps(10),
      m_client_bandwidth_gbps(1),
      m_network_representation_factor(1),
      m_client_representation_factor(1),
      m_graphviz_enabled(false),
      m_ping_enabled(false),
      m_current_system_id(0)
{
    NS_LOG_FUNCTION(this);
}

NetworkTopologyCreator::~NetworkTopologyCreator()
{
    NS_LOG_FUNCTION(this);
}

void
NetworkTopologyCreator::SetNetworkSpeedFactors(double backboneFactor,
                                               double regionalFactor,
                                               double localFactor)
{ // Set network speed to latency speed on each level
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(
        backboneFactor > 1 && regionalFactor > 1 && localFactor > 1,
        "Factors must be larger than 1. Cannot achieve faster fiber optic transmission speeds.");

    m_backbone_speed_km_per_ms = MAX_FIBER_SPEED_KM_PER_MS * backboneFactor;
    m_regional_speed_km_per_ms = MAX_FIBER_SPEED_KM_PER_MS * regionalFactor;
    m_local_speed_km_per_ms = MAX_FIBER_SPEED_KM_PER_MS * localFactor;
}

void
NetworkTopologyCreator::SetNetworkBandwidth(double backboneGbps,
                                            double regionalGbps,
                                            double localGbps,
                                            double clientGbps)
{ // Set network bandwidth for all connections on each network level
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(backboneGbps > 0 && regionalGbps > 0 && localGbps > 0 && clientGbps > 0,
                  "Bandwidth must be positive.");

    m_backbone_bandwidth_gbps = backboneGbps;
    m_regional_bandwidth_gbps = regionalGbps;
    m_local_bandwidth_gbps = localGbps;
    m_client_bandwidth_gbps = clientGbps;
}

void
NetworkTopologyCreator::SetRepresentationFactors(double networkFactor, double clientFactor)
{ // Set representation factors to determine how much of a countries infrastructure is represented
  // in the topology
    NS_LOG_FUNCTION(this);

    NS_ASSERT_MSG(networkFactor <= 1 && networkFactor > 0 && clientFactor <= 1 && clientFactor > 0,
                  "Factors must be between 0 and 1.");

    m_network_representation_factor = networkFactor;
    m_client_representation_factor = clientFactor;
}

void
NetworkTopologyCreator::EnableBackboneGraphviz()
{ // Enable the graphviz logging of the backbone nodes
    NS_LOG_FUNCTION(this);

    m_graphviz_enabled = true;
}

void
NetworkTopologyCreator::ListPings()
{ // Enable the ping logging from each client to the target
    NS_LOG_FUNCTION(this);

    m_ping_enabled = true;
}

void
NetworkTopologyCreator::CreateNetwork(Country country)
{ // Initiate the creation of the network
    NS_LOG_FUNCTION(this);

    // Assign IP addresses
    Ipv4AddressHelper address;
    address.SetBase("240.0.0.0", "255.255.255.0");

    // Build the connected countries structure
    std::optional<Ptr<Node>> noPreviousNode;
    std::optional<std::string> noPreviousCountryName;
    ConnectCountries(country, noPreviousNode, noPreviousCountryName, false, address);

    NS_ASSERT_MSG(m_targetAddress != Ipv4Address::GetZero(), "No target defined");

    MPILog("\nTotal attacking clients:  ",
           m_attackingClients.GetN(),
           "\n",
           "Total legitimate clients: ",
           m_legitimateClients.GetN(),
           "\n");

    // Populate routing tables
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

NodeContainer
NetworkTopologyCreator::GetLegitimateClients()
{ // Getter for all created legitimate client nodes
    return m_legitimateClients;
}

NodeContainer
NetworkTopologyCreator::GetAttackingClients()
{ // Getter for all created attacking client nodes
    return m_attackingClients;
}

Ptr<Node>
NetworkTopologyCreator::GetTargetNode()
{ // Getter for the created target node
    return m_targetNode;
}

Ipv4Address
NetworkTopologyCreator::GetTargetAddress()
{ // Getter for the target IP address
    return m_targetAddress;
}

void
TracePingRtt(std::string countryName, uint16_t, Time rtt)
{ // Log the ping from a node of a country to the target
    NS_LOG_INFO(countryName << ": " << rtt.GetMilliSeconds() << " ms");
}

void
NetworkTopologyCreator::ConnectCountries(Country& country,
                                         const std::optional<Ptr<Node>> previousCountryNode,
                                         const std::optional<std::string> previousCountryName,
                                         bool prevCountryPcapEnabled,
                                         Ipv4AddressHelper& address)
{ // Create a country with all its nodes, connections, subnets, and clients and connect it to the
  // previous
    NS_LOG_FUNCTION(this);

    NodesCounter nodesCounter = {0, 0, 0, 0, country.nodes};

    // position nodes and make connections
    std::vector<PositionedNode> nodes =
        PositionNodesRandomly(country.nodes, country.area, country.population);
    std::vector<std::pair<int, int>> edges = CreateGabrielGraph(nodes);

    // border nodes
    std::vector<PositionedNode> allBorderNodes = FindBorderNodes(nodes);
    bool hasPreviousCountryNode = previousCountryNode.has_value();
    std::vector<PositionedNode> connectingBorderNodes =
        FindFurthestApartNodes(allBorderNodes,
                               country.neighbors.size() + (hasPreviousCountryNode ? 1 : 0));

    NodeContainer backboneNodes;
    backboneNodes.Create(nodes.size(), m_current_system_id);

    if (m_graphviz_enabled)
    { // log backbone routers of countries in graphviz
        MPILog("\n", country.name);

        for (auto& node : nodes)
        {
            MPILog(backboneNodes.Get(node.id)->GetId(),
                   "[pos=\"",
                   node.position.x / 100,
                   ",",
                   node.position.y / 100,
                   "!\"]");
        }

        for (auto& edge : edges)
        {
            MPILog(backboneNodes.Get(edge.first)->GetId(),
                   " -- ",
                   backboneNodes.Get(edge.second)->GetId());
        }
    }

    InternetStackHelper internetStackHelper;
    internetStackHelper.Install(backboneNodes);

    // P2P connection setup with backbone bandwidth
    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("Mtu", UintegerValue(1500));
    std::stringstream dataRate;
    dataRate << m_backbone_bandwidth_gbps << "Gbps";
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate.str()));

    NetDeviceContainer backboneDevices;

    double totalDistance = 0.0; // Initialize a variable to store the total distance

    for (auto& edge : edges)
    { // calculate distance between nodes and set the latency (delay) of the connections
      // appropriately
        double dist =
            CalculateDistance(nodes.at(edge.first).position, nodes.at(edge.second).position);
        totalDistance += dist;
        std::stringstream delay;
        delay << (dist * m_backbone_speed_km_per_ms) << "ms";
        p2pHelper.SetChannelAttribute("Delay", StringValue(delay.str()));

        // make connection and assign ip addresses
        NetDeviceContainer devices;
        devices.Add(
            p2pHelper.Install(backboneNodes.Get(edge.first), backboneNodes.Get(edge.second)));
        address.Assign(devices);
        address.NewNetwork();
        backboneDevices.Add(devices);
    }

    // Calculate the average distance by dividing the total distance by the number of edges (1 as
    // fallback)
    double averageDistance = edges.empty() ? 1 : totalDistance / static_cast<double>(edges.size());

    const bool pcapEnabled =
        country.enablePcap && MpiInterface::GetSystemId() == m_current_system_id;

    if (hasPreviousCountryNode)
    { // make the connection to the previous country

        // set the latency (delay) according to the average distance of the connections in th
        // current country
        std::stringstream delay;
        delay << (averageDistance * m_backbone_speed_km_per_ms) << "ms";
        p2pHelper.SetChannelAttribute("Delay", StringValue(delay.str()));

        // make connection and assign ip addresses
        NetDeviceContainer countryConnectionDevices =
            (p2pHelper.Install(backboneNodes.Get(connectingBorderNodes.at(0).id),
                               previousCountryNode.value()));

        address.Assign(countryConnectionDevices);
        address.NewNetwork();

        if (m_graphviz_enabled)
        { // log country connections in graphviz
            MPILog("\n", "Country Connection: ", country.name, " - ", previousCountryName.value());
            MPILog(backboneNodes.Get(connectingBorderNodes.at(0).id)->GetId(),
                   " -- ",
                   previousCountryNode.value()->GetId());
        }

        if (pcapEnabled)
        { // enable pcap on the net device, that is in the current country
            p2pHelper.EnablePcap("backbone-node-" + country.name, countryConnectionDevices.Get(0));
        }

        if (prevCountryPcapEnabled && previousCountryName.has_value())
        { // enable pcap on the net device, that is in the previous country
            p2pHelper.EnablePcap("backbone-node-" + previousCountryName.value(),
                                 countryConnectionDevices.Get(1));
        }
    }

    if (pcapEnabled)
    { // enable pcap on all backbone devices of the current country
        p2pHelper.EnablePcap("backbone-node-" + country.name, backboneDevices);
    }

    for (uint32_t i = 0; i < country.nodes; ++i)
    { // build a regional subnet for each of the backbone nodes
        NodesCounter res = BuildSubnet(country,
                                       country.area / country.nodes,
                                       2,
                                       900,
                                       country.population / country.area,
                                       backboneNodes.Get(i),
                                       address);

        nodesCounter = {nodesCounter.attackingClients + res.attackingClients,
                        nodesCounter.legitimateClients + res.legitimateClients,
                        nodesCounter.localRouters + res.localRouters,
                        nodesCounter.regionalRouters + res.regionalRouters,
                        nodesCounter.backboneRouters + res.backboneRouters};
    }

    MPILog("\n",
           country.name,
           "\nBackbone Routers:   ",
           nodesCounter.backboneRouters,
           "\nRegional Routers:   ",
           nodesCounter.regionalRouters,
           "\nLocal Routers:      ",
           nodesCounter.localRouters,
           "\nAttacking Clients:  ",
           nodesCounter.attackingClients,
           "\nLegitimate Clients: ",
           nodesCounter.legitimateClients);

    // increment MPI rank for next country
    if (m_current_system_id == MpiInterface::GetSize() - 1)
    {
        m_current_system_id = 0;
    }
    else
    {
        m_current_system_id += 1;
    }

    for (ulong i = (hasPreviousCountryNode ? 1 : 0); i < connectingBorderNodes.size(); ++i)
    { // connect a country to each neighboring country
        ConnectCountries(country.neighbors.at(i - (hasPreviousCountryNode ? 1 : 0)),
                         backboneNodes.Get(connectingBorderNodes.at(i).id),
                         country.name,
                         pcapEnabled,
                         address);
    }
}

NetworkTopologyCreator::NodesCounter
NetworkTopologyCreator::BuildSubnet(Country& c,
                                    ulong area,
                                    uint32_t level,
                                    uint32_t nodeServiceArea,
                                    double populationDensity,
                                    Ptr<Node> parentNode,
                                    Ipv4AddressHelper& address)
{ // build a subnet of regional (level 2) or local (level 1) routers
    NS_LOG_FUNCTION(this);

    uint32_t numberOfNodes =
        std::max(std::round((area / nodeServiceArea) * m_network_representation_factor), 1.0);
    NodesCounter nodesCounter = {0,
                                 0,
                                 level == 1 ? numberOfNodes : 0,
                                 level == 2 ? numberOfNodes : 0,
                                 0};

    // position nodes and make connections
    std::vector<PositionedNode> nodes = PositionNodesRandomly(numberOfNodes, area, level * area);
    std::vector<std::pair<int, int>> edges = CreateGabrielGraph(nodes);

    // calculate position of parent node of current subnet
    std::vector<PositionedNode> centralNodes = FindMostCentralNodes(nodes, 3);
    Vector2D parentPosition = FindCentralPoint(centralNodes);

    NodeContainer routerNodes;
    routerNodes.Create(numberOfNodes, m_current_system_id);

    InternetStackHelper internetStackHelper;
    internetStackHelper.Install(routerNodes);

    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("Mtu", UintegerValue(1500));

    if (level == 2)
    { // handle regional subnet
        std::stringstream dataRate;
        dataRate << m_regional_bandwidth_gbps << "Gbps";
        p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate.str()));

        for (uint32_t i = 0; i < numberOfNodes; ++i)
        { // for each regional router build a local (level 1) subnet
            NodesCounter res = BuildSubnet(c,
                                           area / (numberOfNodes / m_network_representation_factor),
                                           1,
                                           4,
                                           populationDensity,
                                           routerNodes.Get(i),
                                           address);
            nodesCounter = {nodesCounter.attackingClients + res.attackingClients,
                            nodesCounter.legitimateClients + res.legitimateClients,
                            nodesCounter.localRouters + res.localRouters,
                            nodesCounter.regionalRouters + res.regionalRouters,
                            nodesCounter.backboneRouters + res.backboneRouters};
        }
    }

    if (level == 1)
    { // handle local subnet
        std::stringstream dataRate;
        dataRate << m_local_bandwidth_gbps << "Gbps";
        p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate.str()));

        for (uint32_t i = 0; i < numberOfNodes; ++i)
        { // for each regional router build an access net
            NodesCounter res = BuildAccessNet(
                c,
                std::max(std::round((populationDensity * m_client_representation_factor)), 1.0),
                routerNodes.Get(i),
                address);
            nodesCounter = {nodesCounter.attackingClients + res.attackingClients,
                            nodesCounter.legitimateClients + res.legitimateClients,
                            nodesCounter.localRouters,
                            nodesCounter.regionalRouters,
                            nodesCounter.backboneRouters};
        }
    }

    NetDeviceContainer routerDevices;

    for (auto& centralNode : centralNodes)
    { // connection between central nodes of current subnet and parent node
        double dist = CalculateDistance(parentPosition, centralNode.position);
        std::stringstream delay;
        delay << (dist * (level == 2 ? m_regional_speed_km_per_ms : m_local_speed_km_per_ms))
              << "ms";
        p2pHelper.SetChannelAttribute("Delay", StringValue(delay.str()));

        // make connection and assign ip addresses
        NetDeviceContainer devices;
        devices.Add(p2pHelper.Install(parentNode, routerNodes.Get(centralNode.id)));
        address.Assign(devices);
        address.NewNetwork();
        routerDevices.Add(devices);
    }

    for (auto& edge : edges)
    { // calculate distance between nodes and set the latency (delay) of the connections
      // appropriately
        double dist =
            CalculateDistance(nodes.at(edge.first).position, nodes.at(edge.second).position);
        std::stringstream delay;
        delay << (dist * (level == 2 ? m_regional_speed_km_per_ms : m_local_speed_km_per_ms))
              << "ms";
        p2pHelper.SetChannelAttribute("Delay", StringValue(delay.str()));

        // make connection and assign ip addresses
        NetDeviceContainer devices;
        devices.Add(p2pHelper.Install(routerNodes.Get(edge.first), routerNodes.Get(edge.second)));
        address.Assign(devices);
        address.NewNetwork();
        routerDevices.Add(devices);
    }

    return {nodesCounter};
}

NetworkTopologyCreator::NodesCounter
NetworkTopologyCreator::BuildAccessNet(Country& c,
                                       uint32_t numberOfClients,
                                       Ptr<Node> parentNode,
                                       Ipv4AddressHelper& address)
{ // build an access net between local routers and clients
    const bool accessNetIncludesTarget = m_targetAddress == Ipv4Address::GetZero();

    NodeContainer clientNodes;
    clientNodes.Create(numberOfClients, m_current_system_id);

    InternetStackHelper internetStackHelper;
    internetStackHelper.Install(clientNodes);

    PointToPointHelper p2pHelper;
    p2pHelper.SetDeviceAttribute("Mtu", UintegerValue(1500));
    std::stringstream dataRate;
    dataRate << m_client_bandwidth_gbps << "Gbps";
    p2pHelper.SetDeviceAttribute("DataRate", StringValue(dataRate.str()));
    // constant latency (delay) independent of position as the distances are very short
    p2pHelper.SetChannelAttribute("Delay", StringValue("1ms"));

    NetDeviceContainer clientDevices;
    NetDeviceContainer routerDevices;

    Ipv4InterfaceContainer clientInterfaces;
    for (uint32_t i = 0; i < numberOfClients; ++i)
    { // make connection between clients and router and assign ip addresses
        NetDeviceContainer devices;
        // order is important as first interface may be the target
        devices.Add(p2pHelper.Install(clientNodes.Get(i), parentNode));
        clientDevices.Add(devices.Get(0));
        routerDevices.Add(devices.Get(1));
        clientInterfaces.Add(address.Assign(devices));
        address.NewNetwork();
    }

    if (accessNetIncludesTarget)
    { // setting address of first node as target address
        m_targetAddress = clientInterfaces.GetAddress(0);
        m_targetNode = clientNodes.Get(0);
        if (MpiInterface::GetSystemId() == m_current_system_id)
        { // record pcap at target
            p2pHelper.EnablePcap("attack-target-device", clientDevices.Get(0), true);
        }
    }

    // setup ping application towards target with just one ping and silent NS3 logging
    ApplicationContainer pingContainer;
    PingHelper pingHelper(m_targetAddress);
    pingHelper.SetAttribute("Count", UintegerValue(1));
    pingHelper.SetAttribute("VerboseMode", EnumValue(Ping::VerboseMode::SILENT));

    uint32_t numberOfAttackingClients = 0;
    uint32_t numberOfLegitimateClients = 0;

    std::mt19937 rng(parentNode->GetId());
    std::uniform_real_distribution<double> dist(0, 1);

    for (uint32_t i = accessNetIncludesTarget ? 1 : 0; i < numberOfClients; ++i)
    { // choose legitimate or attacking client randomly based on attackTrafficFactor
      // if the target is included in the current access net, it
        if (dist(rng) < c.attackTrafficFactor)
        {
            m_attackingClients.Add(clientNodes.Get(i));
            numberOfAttackingClients += 1;
        }
        else
        {
            m_legitimateClients.Add(clientNodes.Get(i));
            numberOfLegitimateClients += 1;
        }

        if (m_ping_enabled)
        { // enable ping on all clients if it is enabled
            pingContainer.Add(pingHelper.Install(clientNodes.Get(i)));
            Ptr<Ping> ping = pingContainer.Get(pingContainer.GetN() - 1)->GetObject<Ping>();
            ping->TraceConnectWithoutContext("Rtt", MakeBoundCallback(&TracePingRtt, c.name));
        }
    }

    return {numberOfAttackingClients, numberOfLegitimateClients, 0, 0, 0};
}

std::vector<NetworkTopologyCreator::PositionedNode>
NetworkTopologyCreator::PositionNodesRandomly(uint32_t numberOfNodes, double area, uint32_t seed)
{ // position nodes randomly in a square area
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> dist(0, std::sqrt(area));

    std::vector<PositionedNode> nodes(numberOfNodes);
    for (uint32_t i = 0; i < numberOfNodes; i++)
    {
        nodes[i] = {static_cast<int>(i), {dist(rng), dist(rng)}};
    }
    return nodes;
}

bool
NetworkTopologyCreator::IsGabrielEdge(const PositionedNode& a,
                                      const PositionedNode& b,
                                      const std::vector<PositionedNode>& nodes)
{ // check if connection is a gabriel edge
    double abDistance = CalculateDistance(a.position, b.position);
    Vector2D midPoint = {(a.position.x + b.position.x) / 2, (a.position.y + b.position.y) / 2};
    double radius = abDistance / 2;

    for (const auto& node : nodes)
    {
        if (node.id != a.id && node.id != b.id &&
            CalculateDistance(node.position, midPoint) <= radius)
        {
            return false;
        }
    }

    return true;
}

std::vector<std::pair<int, int>>
NetworkTopologyCreator::CreateGabrielGraph(const std::vector<PositionedNode>& nodes)
{ // create a gabriel graph from an array of positioned nodes
    std::vector<std::pair<int, int>> edges;

    for (size_t i = 0; i < nodes.size(); ++i)
    {
        for (size_t j = i + 1; j < nodes.size(); ++j)
        {
            if (IsGabrielEdge(nodes[i], nodes[j], nodes))
            {
                edges.emplace_back(nodes[i].id, nodes[j].id);
            }
        }
    }

    return edges;
}

// Helper function to calculate cross product of vectors p1-p0 and p2-p0
double
crossProduct(const Vector2D& p0, const Vector2D& p1, const Vector2D& p2)
{
    return (p1.x - p0.x) * (p2.y - p0.y) - (p1.y - p0.y) * (p2.x - p0.x);
}

// Function to find border nodes using Graham's Scan algorithm
std::vector<NetworkTopologyCreator::PositionedNode>
NetworkTopologyCreator::FindBorderNodes(const std::vector<PositionedNode>& nodes)
{
    // Find the bottom-left node as the starting point
    const PositionedNode* start = &nodes[0];
    for (const auto& node : nodes)
    {
        if (node.position.y < start->position.y ||
            (node.position.y == start->position.y && node.position.x < start->position.x))
        {
            start = &node;
        }
    }

    // Sort the nodes by polar angle with respect to the starting node
    std::vector<const PositionedNode*> sortedNodes;
    for (const auto& node : nodes)
    {
        sortedNodes.push_back(&node);
    }
    std::sort(sortedNodes.begin(),
              sortedNodes.end(),
              [start](const PositionedNode* a, const PositionedNode* b) {
                  double cp = crossProduct(start->position, a->position, b->position);
                  if (cp == 0)
                  {
                      return CalculateDistanceSquared(start->position, a->position) <
                             CalculateDistanceSquared(start->position, b->position);
                  }
                  return cp > 0;
              });

    // Find the border nodes using Graham's Scan algorithm
    std::vector<PositionedNode> borderNodes = {*start};
    for (const auto& node : sortedNodes)
    {
        while (borderNodes.size() >= 2 && crossProduct(borderNodes[borderNodes.size() - 2].position,
                                                       borderNodes.back().position,
                                                       node->position) <= 0)
        {
            borderNodes.pop_back();
        }
        borderNodes.push_back(*node);
    }

    return borderNodes;
}

// Function to find numberOfConnections furthest apart border nodes
std::vector<NetworkTopologyCreator::PositionedNode>
NetworkTopologyCreator::FindFurthestApartNodes(const std::vector<PositionedNode>& borderNodes,
                                               ulong numberOfConnections)
{
    if (numberOfConnections <= 0)
    {
        return {};
    }

    ulong borderNodeCount = borderNodes.size();
    std::vector<PositionedNode> selectedNodes;

    // If numberOfConnections is greater than borderNodeCount, select all nodes, then repeat the
    // selection process
    while (numberOfConnections > borderNodeCount)
    {
        selectedNodes.insert(selectedNodes.end(), borderNodes.begin(), borderNodes.end());
        numberOfConnections -= borderNodeCount;
    }

    std::vector<bool> used(borderNodeCount, false);
    int currentNodeIndex = 0;
    used[currentNodeIndex] = true;
    selectedNodes.push_back(borderNodes[currentNodeIndex]);
    numberOfConnections--;

    while (numberOfConnections > 0)
    {
        double maxDistance = -1;
        int maxIndex = -1;

        for (ulong i = 0; i < borderNodeCount; i++)
        {
            if (!used[i])
            {
                double minDistance = std::numeric_limits<double>::max();
                for (const auto& selectedNode : selectedNodes)
                {
                    double currentDistance =
                        CalculateDistance(borderNodes[i].position, selectedNode.position);
                    if (currentDistance < minDistance)
                    {
                        minDistance = currentDistance;
                    }
                }

                if (minDistance > maxDistance)
                {
                    maxDistance = minDistance;
                    maxIndex = i;
                }
            }
        }

        used[maxIndex] = true;
        selectedNodes.push_back(borderNodes[maxIndex]);
        numberOfConnections--;
    }

    return selectedNodes;
}

// Function to find the k most central nodes
std::vector<NetworkTopologyCreator::PositionedNode>
NetworkTopologyCreator::FindMostCentralNodes(const std::vector<PositionedNode>& nodes, int k)
{
    if (k <= 0 || nodes.empty())
    {
        return {};
    }

    int nodeCount = static_cast<int>(nodes.size());
    k = std::min(k, nodeCount);

    // Calculate the centroid
    double centroidX = 0, centroidY = 0;
    for (const auto& node : nodes)
    {
        centroidX += node.position.x;
        centroidY += node.position.y;
    }
    centroidX /= nodeCount;
    centroidY /= nodeCount;

    Vector2D centroid{centroidX, centroidY};

    // Sort the nodes based on their distance to the centroid
    std::vector<PositionedNode> sortedNodes = nodes;
    std::sort(sortedNodes.begin(),
              sortedNodes.end(),
              [&centroid](const PositionedNode& a, const PositionedNode& b) {
                  return CalculateDistance(a.position, centroid) <
                         CalculateDistance(b.position, centroid);
              });

    // Select the k most central nodes
    std::vector<PositionedNode> centralNodes(sortedNodes.begin(), sortedNodes.begin() + k);
    return centralNodes;
}

// Function to find central point between nodes
Vector2D
NetworkTopologyCreator::FindCentralPoint(const std::vector<PositionedNode>& nodes)
{
    double xSum = 0;
    double ySum = 0;

    // Sum the x, y, and z coordinates of each node
    for (const PositionedNode& node : nodes)
    {
        xSum += node.position.x;
        ySum += node.position.y;
    }

    return {xSum / nodes.size(), ySum / nodes.size()};
}

// Function to log only if MPI rank is 0
template <typename... Args>
void
NetworkTopologyCreator::MPILog(Args... logArgs)
{
    if (MpiInterface::GetSystemId() == 0)
    {
        std::stringstream logStream;
        (logStream << ... << logArgs); // fold expression
        NS_LOG_INFO(logStream.str());
    }
}