#ifndef NETWORK_TOPOLOGY_CREATOR_H
#define NETWORK_TOPOLOGY_CREATOR_H

#include "json.hpp"

#include "ns3/core-module.h"
#include "ns3/internet-module.h"
#include "ns3/mpi-interface.h"
#include "ns3/ping-helper.h"
#include "ns3/point-to-point-module.h"

#include <random>

using namespace ns3;
using json = nlohmann::json;

/**
 * \brief Creates a network using a connected countries approach with a multi-level topology
 *
 */
class NetworkTopologyCreator
{
  public:
    /**
     * \brief Struct that holds all information about a specific country
     */
    struct Country
    {
        std::string name;  //!< Name of the country
        uint32_t nodes;    //!< Number of backbone nodes
        double area;       //!< Area of the country in sqkm
        double population; //!< Population of the country
        bool enablePcap;   //!< Is pcap recording enabled for backbone nodes of the country
        double
            attackTrafficFactor; //!< How much of the clients produce attack traffic in this country
        std::vector<Country> neighbors; //!< What are the neighboring countries (without loops)

        /**
         *  \brief Convert a json to the struct
         */
        friend void from_json(const json& j, Country& c)
        {
            j.at("name").get_to(c.name);
            j.at("nodes").get_to(c.nodes);
            j.at("area").get_to(c.area);
            j.at("population").get_to(c.population);
            j.at("enablePcap").get_to(c.enablePcap);
            j.at("attackTrafficFactor").get_to(c.attackTrafficFactor);

            if (j.contains("neighbors"))
            {
                for (const auto& neighbor : j["neighbors"])
                {
                    Country neighborCountry;
                    neighbor.get_to(neighborCountry);
                    c.neighbors.push_back(neighborCountry);
                }
            }
        }
    };

    NetworkTopologyCreator();
    ~NetworkTopologyCreator();

    /**
     * \brief Set the network speed factors for each level.
     *
     * \param backboneFactor The factor for the backbone network.
     * \param regionalFactor The factor for the regional network.
     * \param localFactor The factor for the local network.
     */
    void SetNetworkSpeedFactors(double backboneFactor, double regionalFactor, double localFactor);

    /**
     * \brief Set the bandwidth for each level.
     *
     * \param backboneGbps The bandwidth between backbone routers.
     * \param regionalGbps The bandwidth between regional routers.
     * \param localGbps The bandwidth between local routers.
     * \param clientGbps The bandwidth between local routers and their clients.
     */
    void SetNetworkBandwidth(double backboneGbps,
                             double regionalGbps,
                             double localGbps,
                             double clientGbps);
    /**
     * \brief Set the representation factors to determine how much of a countries infrastructure is
     * represented in the topology
     *
     * \param networkFactor The factor of how many subnet routers of a parent node are represented.
     * \param clientFactor The factor of how many clients of a local router are represented.
     */
    void SetRepresentationFactors(double networkFactor, double clientFactor);

    /**
     * \brief Enable the graphviz logging of the backbone nodes
     */
    void EnableBackboneGraphviz();
    /**
     * \brief Enable the ping logging from each client to the target
     */
    void ListPings();

    /**
     * \brief Initiate the creation of the network
     * \param country The connected countries object that defines the topology.
     */
    void CreateNetwork(Country country);

    /**
     * \brief Getter for all created legitimate client nodeset
     */
    NodeContainer GetLegitimateClients();
    /**
     * \brief Getter for all created attacking client nodes
     */
    NodeContainer GetAttackingClients();
    /**
     * \brief Getter for the created target node
     */
    Ptr<Node> GetTargetNode();
    /**
     * \brief Getter for the target IP address
     */
    Ipv4Address GetTargetAddress();

  private:
    /**
     * \brief Struct with 2D position used for placing and connecting nodes according to algorithms
     */
    struct PositionedNode
    {
        int id;
        Vector2D position;
    };

    /**
     * \brief Struct used to keep track of the amount of nodes within each level for each country
     * for statistics
     */
    struct NodesCounter
    {
        uint32_t attackingClients, legitimateClients, localRouters, regionalRouters,
            backboneRouters;
    };

    /**
     * \brief Create a country with all its nodes, connections, subnets, and clients and connect it
     * to the previous country
     *
     * \param country The country to add to the topology
     * \param previousCountryNode The border node of the previous country
     * \param previousCountryName The name of the previous country
     * \param prevCountryPcapEnabled Flag whether pcap was enabled in the previous country
     * \param address
     */
    void ConnectCountries(Country& country,
                          const std::optional<Ptr<Node>> previousCountryNode,
                          std::optional<std::string> previousCountryName,
                          bool prevCountryPcapEnabled,
                          Ipv4AddressHelper& address);

    /**
     * \brief Build a subnet of regional or local routers
     *
     * \param country The country in which the subnet is
     * \param area The area the subnet covers
     * \param level The level of the subnet
     * \param nodeServiceArea The area a node in the subnet covers
     * \param populationDensity The population density in the subnet service area
     * \param parentNode The parent node of the subnet
     * \param address
     */
    NodesCounter BuildSubnet(Country& country,
                             ulong area,
                             uint32_t level,
                             uint32_t nodeServiceArea,
                             double populationDensity,
                             Ptr<Node> parentNode,
                             Ipv4AddressHelper& address);

    /**
     * \brief Build an access net between local routers and clients
     *
     * \param country The country in which the access net is
     * \param numberOfClients The number of clients in the access net
     * \param parentNode The local router of the clients in the access net
     * \param address
     */
    NodesCounter BuildAccessNet(Country& country,
                                uint32_t numberOfClients,
                                Ptr<Node> parentNode,
                                Ipv4AddressHelper& address);

    /**
     * \brief Position nodes randomly in a square area
     *
     * \param numberOfNodes The number of nodes to position
     * \param area The area in which the nodes are positioned
     * \param seed The seed of the deterministic random function to position the nodes
     */
    std::vector<PositionedNode> PositionNodesRandomly(uint32_t numberOfNodes,
                                                      double area,
                                                      uint32_t seed);

    /**
     * \brief Check if connection is a gabriel edge
     *
     * \param a The node a
     * \param b The node b
     * \param nodes All nodes including a and b
     */
    bool IsGabrielEdge(const PositionedNode& a,
                       const PositionedNode& b,
                       const std::vector<PositionedNode>& nodes);

    /**
     * \brief Create a gabriel graph from an array of positioned nodes
     *
     * \param nodes All nodes within the gabriel graph
     */
    std::vector<std::pair<int, int>> CreateGabrielGraph(const std::vector<PositionedNode>& nodes);

    /**
     * \brief Find border nodes of a network
     *
     * \param nodes All positioned nodes within the network
     */
    std::vector<PositionedNode> FindBorderNodes(const std::vector<PositionedNode>& nodes);

    /**
     * \brief Find the k furthest apart border nodes to connect to other countries
     *
     * \param borderNodes The border nodes
     * \param numberOfConnections The number of nodes that should be selected that are furthest
     * apart
     */
    std::vector<PositionedNode> FindFurthestApartNodes(
        const std::vector<PositionedNode>& borderNodes,
        ulong numberOfConnections);

    /**
     * \brief Find the k most central nodes
     *
     * \param nodes All positioned nodes within the network
     * \param k The number of central nodes
     */
    std::vector<PositionedNode> FindMostCentralNodes(const std::vector<PositionedNode>& nodes,
                                                     int k);

    /**
     * \brief Find central point between nodes
     *
     * \param nodes All positioned nodes from which the central point should be calculated
     */
    Vector2D FindCentralPoint(const std::vector<PositionedNode>& nodes);

    /**
     * \brief Log only if MPI rank is 0
     *
     * \param logArgs Items that should be string concatenated and logged
     */
    template <typename... Args>
    void MPILog(Args... logArgs);

    NodeContainer m_legitimateClients; //!< Clients producing legitimate traffic
    NodeContainer m_attackingClients;  //!< Clients producing attacking traffic
    Ptr<Node> m_targetNode;            //!< Attack target node
    Ipv4Address m_targetAddress;       //!< Attack target IP

    double m_backbone_speed_km_per_ms; //!< The slowing down factor of the latency between backbone
                                       //!< routers
    double m_regional_speed_km_per_ms; //!< The slowing down factor of the latency between regional
                                       //!< routers
    double
        m_local_speed_km_per_ms; //!< The slowing down factor of the latency between local routers

    double m_backbone_bandwidth_gbps; //!< The bandwidth between backbone routers in gbps
    double m_regional_bandwidth_gbps; //!< The bandwidth between regional routers in gbps
    double m_local_bandwidth_gbps;    //!< The bandwidth between local routers in gbps
    double
        m_client_bandwidth_gbps; //!< The bandwidth between local routers and their clients in gbps

    double m_network_representation_factor; //!< Factor on how much the subnet router network is
                                            //!< represented
    double m_client_representation_factor; //!< Factor on how much the clients in the access network
                                           //!< are represented

    bool m_graphviz_enabled; //!< Log graphviz nodes enabled
    bool m_ping_enabled;     //!< Log pings enabled

    uint32_t m_current_system_id; //!< System ID that is assigned to the nodes
};

#endif /* NETWORK_TOPOLOGY_CREATOR_H */
