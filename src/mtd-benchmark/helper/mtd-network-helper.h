/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Network Helper for setting up MTD topology
 */

#ifndef MTD_NETWORK_HELPER_H
#define MTD_NETWORK_HELPER_H

#include "ns3/object.h"
#include "ns3/node-container.h"
#include "ns3/net-device-container.h"
#include "ns3/internet-stack-helper.h"
#include "ns3/ipv4-interface-container.h"
#include "ns3/ipv4-address.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/application-container.h"
#include "ns3/ptr.h"

#include <string>
#include <vector>
#include <map>

namespace ns3 {
namespace mtd {

class DomainManager;
class ShuffleController;
class ScoreManager;

/**
 * \brief Network topology configuration
 */
struct TopologyConfig {
    uint32_t numClients;
    uint32_t numProxies;
    uint32_t numServers;
    uint32_t numAttackers;
    uint32_t defaultNumDomains;    ///< Default number of domains for initialization
    std::string clientDataRate;
    std::string proxyDataRate;
    std::string serverDataRate;
    std::string attackerDataRate;
    std::string clientDelay;
    std::string proxyDelay;
    std::string serverDelay;
    std::string attackerDelay;
    
    TopologyConfig()
        : numClients(100),
          numProxies(10),
          numServers(5),
          numAttackers(1),
          defaultNumDomains(3),
          clientDataRate("100Mbps"),
          proxyDataRate("1Gbps"),
          serverDataRate("10Gbps"),
          attackerDataRate("1Gbps"),
          clientDelay("5ms"),
          proxyDelay("1ms"),
          serverDelay("1ms"),
          attackerDelay("5ms") {}
};

/**
 * \ingroup mtd
 * \brief Helper for creating MTD network topology
 *
 * This helper creates the network topology consisting of:
 * - Client nodes
 * - Proxy nodes
 * - Server nodes
 * - Attacker nodes
 * - Controller node
 */
class MtdNetworkHelper : public Object
{
public:
    static TypeId GetTypeId();

    MtdNetworkHelper();
    ~MtdNetworkHelper() override;
    
    /**
     * \brief Set topology configuration
     * \param config Topology configuration
     */
    void SetTopologyConfig(const TopologyConfig& config);
    
    /**
     * \brief Get topology configuration
     * \return Current topology configuration
     */
    TopologyConfig GetTopologyConfig() const;
    
    /**
     * \brief Create the network topology
     * Creates all nodes and network devices.
     */
    void CreateTopology();
    
    /**
     * \brief Install Internet stack on all nodes
     */
    void InstallInternetStack();
    
    /**
     * \brief Assign IP addresses to all interfaces
     */
    void AssignIpAddresses();
    
    /**
     * \brief Get client nodes
     * \return NodeContainer with client nodes
     */
    NodeContainer GetClientNodes() const;
    
    /**
     * \brief Get proxy nodes
     * \return NodeContainer with proxy nodes
     */
    NodeContainer GetProxyNodes() const;
    
    /**
     * \brief Get server nodes
     * \return NodeContainer with server nodes
     */
    NodeContainer GetServerNodes() const;
    
    /**
     * \brief Get attacker nodes
     * \return NodeContainer with attacker nodes
     */
    NodeContainer GetAttackerNodes() const;
    
    /**
     * \brief Get controller node
     * \return Pointer to controller node
     */
    Ptr<Node> GetControllerNode() const;
    
    /**
     * \brief Get all nodes
     * \return NodeContainer with all nodes
     */
    NodeContainer GetAllNodes() const;
    
    /**
     * \brief Get client IP addresses
     * \return IP interface container for clients
     */
    Ipv4InterfaceContainer GetClientInterfaces() const;
    
    /**
     * \brief Get proxy IP addresses
     * \return IP interface container for proxies
     */
    Ipv4InterfaceContainer GetProxyInterfaces() const;
    
    /**
     * \brief Get server IP addresses
     * \return IP interface container for servers
     */
    Ipv4InterfaceContainer GetServerInterfaces() const;
    
    /**
     * \brief Get client-to-proxy device mapping
     * \return Map of client ID to proxy device
     */
    std::map<uint32_t, Ptr<NetDevice>> GetClientProxyMapping() const;
    
    /**
     * \brief Enable packet capture (PCAP)
     * \param prefix File prefix for PCAP files
     */
    void EnablePcap(const std::string& prefix);
    
    /**
     * \brief Set up routing tables
     */
    void SetupRouting();
    
    /**
     * \brief Initialize the MTD controller
     * \param domainManager Pointer to domain manager
     * \param shuffleController Pointer to shuffle controller
     * \param scoreManager Pointer to score manager
     */
    void InitializeMtdController(Ptr<DomainManager> domainManager,
                                  Ptr<ShuffleController> shuffleController,
                                  Ptr<ScoreManager> scoreManager);
    
    /**
     * \brief Get node by ID
     * \param nodeId Node ID
     * \return Pointer to node
     */
    Ptr<Node> GetNodeById(uint32_t nodeId) const;
    
    /**
     * \brief Get IP address for node
     * \param nodeId Node ID
     * \return IP address string
     */
    std::string GetNodeIpAddress(uint32_t nodeId) const;

    /**
     * \brief Get proxy service IP (VIPA-like) used by applications.
     *
     * This returns a stable per-proxy IP address intended to be used as the
     * destination for benign/attack traffic and for FlowMonitor aggregation.
     *
     * \param proxyNodeId Proxy node ID (also used as ProxyId in model layer)
     * \return Service IP address (Ipv4Address::GetZero() if unknown)
     */
    Ipv4Address GetServiceIp(uint32_t proxyNodeId) const;

    /**
     * \brief Reverse lookup: map a service IP back to proxy ID.
     * \param ip Service IP address
     * \return Proxy node ID, or 0 if not found
     */
    uint32_t GetProxyIdByIp(Ipv4Address ip) const;
    
    /**
     * \brief Print topology summary
     */
    void PrintTopologySummary() const;

private:
    TopologyConfig m_config;
    NodeContainer m_clientNodes;
    NodeContainer m_proxyNodes;
    NodeContainer m_serverNodes;
    NodeContainer m_attackerNodes;
    Ptr<Node> m_controllerNode;
    
    NetDeviceContainer m_clientDevices;
    NetDeviceContainer m_proxyDevices;
    NetDeviceContainer m_serverDevices;
    NetDeviceContainer m_attackerDevices;
    
    Ipv4InterfaceContainer m_clientInterfaces;
    Ipv4InterfaceContainer m_proxyInterfaces;
    Ipv4InterfaceContainer m_serverInterfaces;
    Ipv4InterfaceContainer m_attackerInterfaces;
    
    std::map<uint32_t, Ptr<NetDevice>> m_clientProxyMap;
    std::map<uint32_t, std::string> m_nodeIpMap;

    // Proxy service IP (VIPA-like) bidirectional mapping.
    std::map<uint32_t, Ipv4Address> m_proxyServiceIp;
    std::map<Ipv4Address, uint32_t> m_serviceIpToProxyId;
    
    bool m_topologyCreated;
    bool m_stackInstalled;
    bool m_ipAssigned;
    
    void CreateClientProxyLinks();
    void CreateProxyServerLinks();
    void CreateAttackerLinks();
    void CreateControllerLinks();
};

} // namespace mtd
} // namespace ns3

#endif // MTD_NETWORK_HELPER_H
