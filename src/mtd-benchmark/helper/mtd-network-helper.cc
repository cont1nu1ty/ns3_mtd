/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Network Helper implementation
 */

#include "mtd-network-helper.h"
#include "ns3/ipv4-address-helper.h"
#include "ns3/ipv4-global-routing-helper.h"
#include "ns3/log.h"
#include "ns3/mtd-domain-manager.h"
#include "ns3/mtd-score-manager.h"
#include "ns3/mtd-shuffle-controller.h"
#include "ns3/point-to-point-helper.h"
#include "ns3/string.h"

#include <sstream>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdNetworkHelper");

MtdNetworkHelper::MtdNetworkHelper()
    : m_topologyCreated(false),
      m_stackInstalled(false),
      m_ipAssigned(false)
{
    NS_LOG_FUNCTION(this);
}

MtdNetworkHelper::~MtdNetworkHelper()
{
    NS_LOG_FUNCTION(this);
}

void
MtdNetworkHelper::SetTopologyConfig(const TopologyConfig& config)
{
    NS_LOG_FUNCTION(this);
    m_config = config;
}

TopologyConfig
MtdNetworkHelper::GetTopologyConfig() const
{
    return m_config;
}

void
MtdNetworkHelper::CreateTopology()
{
    NS_LOG_FUNCTION(this);
    
    if (m_topologyCreated)
    {
        NS_LOG_WARN("Topology already created");
        return;
    }
    
    // Create nodes
    m_clientNodes.Create(m_config.numClients);
    m_proxyNodes.Create(m_config.numProxies);
    m_serverNodes.Create(m_config.numServers);
    m_attackerNodes.Create(m_config.numAttackers);
    m_controllerNode = CreateObject<Node>();
    
    NS_LOG_INFO("Created " << m_config.numClients << " clients, "
                << m_config.numProxies << " proxies, "
                << m_config.numServers << " servers, "
                << m_config.numAttackers << " attackers");
    
    // Create links
    CreateClientProxyLinks();
    CreateProxyServerLinks();
    CreateAttackerLinks();
    CreateControllerLinks();
    
    m_topologyCreated = true;
}

void
MtdNetworkHelper::InstallInternetStack()
{
    NS_LOG_FUNCTION(this);
    
    if (m_stackInstalled)
    {
        NS_LOG_WARN("Internet stack already installed");
        return;
    }
    
    InternetStackHelper internet;
    internet.Install(m_clientNodes);
    internet.Install(m_proxyNodes);
    internet.Install(m_serverNodes);
    internet.Install(m_attackerNodes);
    internet.Install(m_controllerNode);
    
    m_stackInstalled = true;
}

void
MtdNetworkHelper::AssignIpAddresses()
{
    NS_LOG_FUNCTION(this);
    
    if (m_ipAssigned)
    {
        NS_LOG_WARN("IP addresses already assigned");
        return;
    }
    
    if (!m_stackInstalled)
    {
        InstallInternetStack();
    }
    
    Ipv4AddressHelper ipv4;
    
    // Group client devices by their target proxy for proper subnet assignment
    // Each proxy gets its own subnet: 10.1.<proxyIdx+1>.0/24
    std::map<uint32_t, NetDeviceContainer> proxySubnetDevices;
    std::map<uint32_t, std::vector<uint32_t>> proxySubnetClientIds;
    
    for (uint32_t i = 0; i < m_clientNodes.GetN(); ++i)
    {
        uint32_t proxyIdx = i % m_config.numProxies;
        auto it = m_clientProxyMap.find(i);
        if (it != m_clientProxyMap.end())
        {
            proxySubnetDevices[proxyIdx].Add(it->second);
            proxySubnetClientIds[proxyIdx].push_back(i);
        }
    }
    
    // Assign addresses per subnet (all devices in same subnet at once)
    for (auto& pair : proxySubnetDevices)
    {
        uint32_t proxyIdx = pair.first;
        NetDeviceContainer& devices = pair.second;
        
        std::ostringstream subnet;
        subnet << "10.1." << (proxyIdx + 1) << ".0";
        ipv4.SetBase(Ipv4Address(subnet.str().c_str()), Ipv4Mask("255.255.255.0"));
        
        Ipv4InterfaceContainer interfaces = ipv4.Assign(devices);
        
        // Store IP addresses for each client
        const std::vector<uint32_t>& clientIds = proxySubnetClientIds[proxyIdx];
        for (uint32_t j = 0; j < interfaces.GetN(); ++j)
        {
            Ipv4Address addr = interfaces.GetAddress(j);
            std::ostringstream addrStr;
            addr.Print(addrStr);
            m_nodeIpMap[m_clientNodes.Get(clientIds[j])->GetId()] = addrStr.str();
        }
    }
    
    // Proxy addresses (10.2.0.x/24)
    ipv4.SetBase("10.2.0.0", "255.255.255.0");
    m_proxyInterfaces = ipv4.Assign(m_proxyDevices);
    
    for (uint32_t i = 0; i < m_proxyNodes.GetN(); ++i)
    {
        Ipv4Address addr = m_proxyInterfaces.GetAddress(i);
        std::ostringstream addrStr;
        addr.Print(addrStr);
        m_nodeIpMap[m_proxyNodes.Get(i)->GetId()] = addrStr.str();
    }
    
    // Server addresses (10.3.0.x/24)
    ipv4.SetBase("10.3.0.0", "255.255.255.0");
    m_serverInterfaces = ipv4.Assign(m_serverDevices);
    
    for (uint32_t i = 0; i < m_serverNodes.GetN(); ++i)
    {
        Ipv4Address addr = m_serverInterfaces.GetAddress(i);
        std::ostringstream addrStr;
        addr.Print(addrStr);
        m_nodeIpMap[m_serverNodes.Get(i)->GetId()] = addrStr.str();
    }
    
    // Attacker addresses (10.4.0.x/24)
    ipv4.SetBase("10.4.0.0", "255.255.255.0");
    m_attackerInterfaces = ipv4.Assign(m_attackerDevices);
    
    for (uint32_t i = 0; i < m_attackerNodes.GetN(); ++i)
    {
        Ipv4Address addr = m_attackerInterfaces.GetAddress(i);
        std::ostringstream addrStr;
        addr.Print(addrStr);
        m_nodeIpMap[m_attackerNodes.Get(i)->GetId()] = addrStr.str();
    }
    
    m_ipAssigned = true;
}

NodeContainer
MtdNetworkHelper::GetClientNodes() const
{
    return m_clientNodes;
}

NodeContainer
MtdNetworkHelper::GetProxyNodes() const
{
    return m_proxyNodes;
}

NodeContainer
MtdNetworkHelper::GetServerNodes() const
{
    return m_serverNodes;
}

NodeContainer
MtdNetworkHelper::GetAttackerNodes() const
{
    return m_attackerNodes;
}

Ptr<Node>
MtdNetworkHelper::GetControllerNode() const
{
    return m_controllerNode;
}

NodeContainer
MtdNetworkHelper::GetAllNodes() const
{
    NodeContainer all;
    all.Add(m_clientNodes);
    all.Add(m_proxyNodes);
    all.Add(m_serverNodes);
    all.Add(m_attackerNodes);
    all.Add(m_controllerNode);
    return all;
}

Ipv4InterfaceContainer
MtdNetworkHelper::GetClientInterfaces() const
{
    return m_clientInterfaces;
}

Ipv4InterfaceContainer
MtdNetworkHelper::GetProxyInterfaces() const
{
    return m_proxyInterfaces;
}

Ipv4InterfaceContainer
MtdNetworkHelper::GetServerInterfaces() const
{
    return m_serverInterfaces;
}

std::map<uint32_t, Ptr<NetDevice>>
MtdNetworkHelper::GetClientProxyMapping() const
{
    return m_clientProxyMap;
}

void
MtdNetworkHelper::EnablePcap(const std::string& prefix)
{
    NS_LOG_FUNCTION(this << prefix);
    
    PointToPointHelper p2p;
    p2p.EnablePcapAll(prefix);
}

void
MtdNetworkHelper::SetupRouting()
{
    NS_LOG_FUNCTION(this);
    
    Ipv4GlobalRoutingHelper::PopulateRoutingTables();
}

void
MtdNetworkHelper::InitializeMtdController(Ptr<DomainManager> domainManager,
                                           Ptr<ShuffleController> shuffleController,
                                           Ptr<ScoreManager> scoreManager)
{
    NS_LOG_FUNCTION(this);
    
    // Create default domains and assign proxies/users
    if (domainManager != nullptr)
    {
        uint32_t numDomains = std::min(m_config.numProxies, m_config.defaultNumDomains);
        
        for (uint32_t d = 0; d < numDomains; ++d)
        {
            std::ostringstream name;
            name << "Domain_" << d;
            uint32_t domainId = domainManager->CreateDomain(name.str());
            
            // Assign proxies to domains
            uint32_t proxiesPerDomain = m_config.numProxies / numDomains;
            for (uint32_t p = d * proxiesPerDomain; p < (d + 1) * proxiesPerDomain; ++p)
            {
                if (p < m_proxyNodes.GetN())
                {
                    domainManager->AddProxy(domainId, m_proxyNodes.Get(p)->GetId());
                }
            }
            
            // Assign users to domains using consistent hash
            for (uint32_t u = 0; u < m_clientNodes.GetN(); ++u)
            {
                uint32_t assignedDomain = u % numDomains;
                if (assignedDomain == d)
                {
                    domainManager->AddUser(domainId, m_clientNodes.Get(u)->GetId());
                }
            }
            
            NS_LOG_INFO("Created domain " << domainId << " with " 
                        << domainManager->GetDomainProxies(domainId).size() << " proxies and "
                        << domainManager->GetDomainUsers(domainId).size() << " users");
        }
    }
    
    // Set shuffle controller references
    if (shuffleController != nullptr)
    {
        shuffleController->SetDomainManager(domainManager);
        shuffleController->SetScoreManager(scoreManager);
    }
}

Ptr<Node>
MtdNetworkHelper::GetNodeById(uint32_t nodeId) const
{
    // Search in all node containers
    for (uint32_t i = 0; i < m_clientNodes.GetN(); ++i)
    {
        if (m_clientNodes.Get(i)->GetId() == nodeId)
        {
            return m_clientNodes.Get(i);
        }
    }
    
    for (uint32_t i = 0; i < m_proxyNodes.GetN(); ++i)
    {
        if (m_proxyNodes.Get(i)->GetId() == nodeId)
        {
            return m_proxyNodes.Get(i);
        }
    }
    
    for (uint32_t i = 0; i < m_serverNodes.GetN(); ++i)
    {
        if (m_serverNodes.Get(i)->GetId() == nodeId)
        {
            return m_serverNodes.Get(i);
        }
    }
    
    for (uint32_t i = 0; i < m_attackerNodes.GetN(); ++i)
    {
        if (m_attackerNodes.Get(i)->GetId() == nodeId)
        {
            return m_attackerNodes.Get(i);
        }
    }
    
    if (m_controllerNode != nullptr && m_controllerNode->GetId() == nodeId)
    {
        return m_controllerNode;
    }
    
    return nullptr;
}

std::string
MtdNetworkHelper::GetNodeIpAddress(uint32_t nodeId) const
{
    auto it = m_nodeIpMap.find(nodeId);
    if (it != m_nodeIpMap.end())
    {
        return it->second;
    }
    return "";
}

void
MtdNetworkHelper::PrintTopologySummary() const
{
    NS_LOG_INFO("=== MTD Network Topology Summary ===");
    NS_LOG_INFO("Clients: " << m_clientNodes.GetN());
    NS_LOG_INFO("Proxies: " << m_proxyNodes.GetN());
    NS_LOG_INFO("Servers: " << m_serverNodes.GetN());
    NS_LOG_INFO("Attackers: " << m_attackerNodes.GetN());
    NS_LOG_INFO("Controller: " << (m_controllerNode != nullptr ? "Yes" : "No"));
    NS_LOG_INFO("====================================");
}

void
MtdNetworkHelper::CreateClientProxyLinks()
{
    NS_LOG_FUNCTION(this);
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(m_config.clientDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(m_config.clientDelay));
    
    // Connect each client to a proxy (round-robin assignment)
    for (uint32_t i = 0; i < m_clientNodes.GetN(); ++i)
    {
        uint32_t proxyIdx = i % m_proxyNodes.GetN();
        
        NetDeviceContainer devices = p2p.Install(
            m_clientNodes.Get(i), m_proxyNodes.Get(proxyIdx));
        
        m_clientDevices.Add(devices.Get(0));
        m_proxyDevices.Add(devices.Get(1));
        
        m_clientProxyMap[i] = devices.Get(0);
    }
}

void
MtdNetworkHelper::CreateProxyServerLinks()
{
    NS_LOG_FUNCTION(this);
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(m_config.proxyDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(m_config.proxyDelay));
    
    // Connect each proxy to all servers (full mesh)
    for (uint32_t p = 0; p < m_proxyNodes.GetN(); ++p)
    {
        for (uint32_t s = 0; s < m_serverNodes.GetN(); ++s)
        {
            NetDeviceContainer devices = p2p.Install(
                m_proxyNodes.Get(p), m_serverNodes.Get(s));
            
            m_proxyDevices.Add(devices.Get(0));
            m_serverDevices.Add(devices.Get(1));
        }
    }
}

void
MtdNetworkHelper::CreateAttackerLinks()
{
    NS_LOG_FUNCTION(this);
    
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue(m_config.attackerDataRate));
    p2p.SetChannelAttribute("Delay", StringValue(m_config.attackerDelay));
    
    // Connect each attacker to all proxies (can target any proxy)
    for (uint32_t a = 0; a < m_attackerNodes.GetN(); ++a)
    {
        for (uint32_t p = 0; p < m_proxyNodes.GetN(); ++p)
        {
            NetDeviceContainer devices = p2p.Install(
                m_attackerNodes.Get(a), m_proxyNodes.Get(p));
            
            m_attackerDevices.Add(devices.Get(0));
            m_proxyDevices.Add(devices.Get(1));
        }
    }
}

void
MtdNetworkHelper::CreateControllerLinks()
{
    NS_LOG_FUNCTION(this);
    
    // Controller connects to all proxies for control plane
    PointToPointHelper p2p;
    p2p.SetDeviceAttribute("DataRate", StringValue("10Gbps"));
    p2p.SetChannelAttribute("Delay", StringValue("0.1ms"));
    
    for (uint32_t p = 0; p < m_proxyNodes.GetN(); ++p)
    {
        p2p.Install(m_controllerNode, m_proxyNodes.Get(p));
    }
}

} // namespace mtd
} // namespace ns3
