/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Domain Manager implementation
 */

#include "mtd-domain-manager.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <functional>
#include <limits>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdDomainManager");

// ==================== DomainManager ====================

NS_OBJECT_ENSURE_REGISTERED(DomainManager);

TypeId
DomainManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::DomainManager")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<DomainManager>();
    return tid;
}

DomainManager::DomainManager()
    : m_strategy(DomainStrategy::CONSISTENT_HASH),
      m_nextDomainId(1)
{
    NS_LOG_FUNCTION(this);
}

DomainManager::~DomainManager()
{
    NS_LOG_FUNCTION(this);
}

uint32_t
DomainManager::CreateDomain(const std::string& name)
{
    NS_LOG_FUNCTION(this << name);
    
    uint32_t domainId = m_nextDomainId++;
    Domain domain(domainId, name);
    m_domains[domainId] = domain;
    
    NS_LOG_INFO("Created domain " << domainId << " (" << name << ")");
    
    return domainId;
}

bool
DomainManager::DeleteDomain(uint32_t domainId)
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domains.find(domainId);
    if (it == m_domains.end())
    {
        return false;
    }
    
    // Find target domain for users
    uint32_t targetDomain = 0;
    for (const auto& pair : m_domains)
    {
        if (pair.first != domainId)
        {
            targetDomain = pair.first;
            break;
        }
    }
    
    if (targetDomain == 0 && !it->second.userIds.empty())
    {
        NS_LOG_WARN("Cannot delete domain with users when no other domain exists");
        return false;
    }
    
    // Move users to target domain
    for (uint32_t userId : it->second.userIds)
    {
        m_userToDomain[userId] = targetDomain;
        if (targetDomain > 0)
        {
            m_domains[targetDomain].userIds.push_back(userId);
        }
    }
    
    // Move proxies similarly
    for (uint32_t proxyId : it->second.proxyIds)
    {
        m_proxyToDomain.erase(proxyId);
    }
    
    m_domains.erase(domainId);
    
    return true;
}

uint32_t
DomainManager::GetDomain(uint32_t userId) const
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_userToDomain.find(userId);
    if (it != m_userToDomain.end())
    {
        return it->second;
    }
    return 0;
}

bool
DomainManager::MoveUser(uint32_t userId, uint32_t newDomainId)
{
    NS_LOG_FUNCTION(this << userId << newDomainId);
    
    // Verify target domain exists
    if (m_domains.find(newDomainId) == m_domains.end())
    {
        NS_LOG_WARN("Target domain " << newDomainId << " does not exist");
        return false;
    }
    
    uint32_t oldDomainId = GetDomain(userId);
    
    // Remove from old domain if exists
    if (oldDomainId > 0)
    {
        auto& oldDomain = m_domains[oldDomainId];
        auto it = std::find(oldDomain.userIds.begin(), oldDomain.userIds.end(), userId);
        if (it != oldDomain.userIds.end())
        {
            oldDomain.userIds.erase(it);
        }
    }
    
    // Add to new domain
    m_domains[newDomainId].userIds.push_back(userId);
    m_userToDomain[userId] = newDomainId;
    
    NotifyUserMigration(userId, oldDomainId, newDomainId);
    
    return true;
}

uint32_t
DomainManager::SplitDomain(uint32_t domainId)
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domains.find(domainId);
    if (it == m_domains.end())
    {
        return 0;
    }
    
    Domain& sourceDomain = it->second;
    
    // Check if domain has enough users/proxies to split
    if (sourceDomain.userIds.size() < m_thresholds.minUsers * 2)
    {
        NS_LOG_WARN("Domain too small to split");
        return 0;
    }
    
    // Create new domain
    std::string newName = sourceDomain.name + "_split";
    uint32_t newDomainId = CreateDomain(newName);
    
    // Move half the users
    size_t halfUsers = sourceDomain.userIds.size() / 2;
    for (size_t i = 0; i < halfUsers; ++i)
    {
        uint32_t userId = sourceDomain.userIds.back();
        sourceDomain.userIds.pop_back();
        m_domains[newDomainId].userIds.push_back(userId);
        m_userToDomain[userId] = newDomainId;
    }
    
    // Move half the proxies
    if (sourceDomain.proxyIds.size() >= m_thresholds.minProxies * 2)
    {
        size_t halfProxies = sourceDomain.proxyIds.size() / 2;
        for (size_t i = 0; i < halfProxies; ++i)
        {
            uint32_t proxyId = sourceDomain.proxyIds.back();
            sourceDomain.proxyIds.pop_back();
            m_domains[newDomainId].proxyIds.push_back(proxyId);
            m_proxyToDomain[proxyId] = newDomainId;
        }
    }
    
    NotifyDomainEvent(EventType::DOMAIN_SPLIT, domainId);
    
    NS_LOG_INFO("Split domain " << domainId << " into " << domainId << " and " << newDomainId);
    
    return newDomainId;
}

uint32_t
DomainManager::MergeDomain(uint32_t domainIdA, uint32_t domainIdB)
{
    NS_LOG_FUNCTION(this << domainIdA << domainIdB);
    
    auto itA = m_domains.find(domainIdA);
    auto itB = m_domains.find(domainIdB);
    
    if (itA == m_domains.end() || itB == m_domains.end())
    {
        return 0;
    }
    
    // Merge B into A
    for (uint32_t userId : itB->second.userIds)
    {
        itA->second.userIds.push_back(userId);
        m_userToDomain[userId] = domainIdA;
    }
    
    for (uint32_t proxyId : itB->second.proxyIds)
    {
        itA->second.proxyIds.push_back(proxyId);
        m_proxyToDomain[proxyId] = domainIdA;
    }
    
    // Delete domain B
    m_domains.erase(domainIdB);
    
    NotifyDomainEvent(EventType::DOMAIN_MERGE, domainIdA);
    
    NS_LOG_INFO("Merged domains " << domainIdA << " and " << domainIdB);
    
    return domainIdA;
}

bool
DomainManager::AddProxy(uint32_t domainId, uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << domainId << proxyId);
    
    auto it = m_domains.find(domainId);
    if (it == m_domains.end())
    {
        return false;
    }
    
    // Check if already in another domain
    auto proxyIt = m_proxyToDomain.find(proxyId);
    if (proxyIt != m_proxyToDomain.end() && proxyIt->second != domainId)
    {
        // Remove from old domain
        auto& oldDomain = m_domains[proxyIt->second];
        auto pos = std::find(oldDomain.proxyIds.begin(), oldDomain.proxyIds.end(), proxyId);
        if (pos != oldDomain.proxyIds.end())
        {
            oldDomain.proxyIds.erase(pos);
        }
    }
    
    it->second.proxyIds.push_back(proxyId);
    m_proxyToDomain[proxyId] = domainId;
    
    return true;
}

bool
DomainManager::RemoveProxy(uint32_t domainId, uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << domainId << proxyId);
    
    auto it = m_domains.find(domainId);
    if (it == m_domains.end())
    {
        return false;
    }
    
    auto pos = std::find(it->second.proxyIds.begin(), it->second.proxyIds.end(), proxyId);
    if (pos != it->second.proxyIds.end())
    {
        it->second.proxyIds.erase(pos);
        m_proxyToDomain.erase(proxyId);
        return true;
    }
    
    return false;
}

bool
DomainManager::AddUser(uint32_t domainId, uint32_t userId)
{
    NS_LOG_FUNCTION(this << domainId << userId);
    
    auto it = m_domains.find(domainId);
    if (it == m_domains.end())
    {
        return false;
    }
    
    // Check if already assigned
    auto userIt = m_userToDomain.find(userId);
    if (userIt != m_userToDomain.end())
    {
        if (userIt->second == domainId)
        {
            return true; // Already in this domain
        }
        return MoveUser(userId, domainId);
    }
    
    it->second.userIds.push_back(userId);
    m_userToDomain[userId] = domainId;
    
    return true;
}

bool
DomainManager::RemoveUser(uint32_t userId)
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_userToDomain.find(userId);
    if (it == m_userToDomain.end())
    {
        return false;
    }
    
    uint32_t domainId = it->second;
    auto domainIt = m_domains.find(domainId);
    if (domainIt != m_domains.end())
    {
        auto pos = std::find(domainIt->second.userIds.begin(), 
                             domainIt->second.userIds.end(), userId);
        if (pos != domainIt->second.userIds.end())
        {
            domainIt->second.userIds.erase(pos);
        }
    }
    
    m_userToDomain.erase(userId);
    return true;
}

Domain
DomainManager::GetDomainInfo(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        return it->second;
    }
    return Domain();
}

std::vector<uint32_t>
DomainManager::GetAllDomainIds() const
{
    std::vector<uint32_t> ids;
    for (const auto& pair : m_domains)
    {
        ids.push_back(pair.first);
    }
    return ids;
}

std::vector<uint32_t>
DomainManager::GetDomainUsers(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        return it->second.userIds;
    }
    return std::vector<uint32_t>();
}

std::vector<uint32_t>
DomainManager::GetDomainProxies(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        return it->second.proxyIds;
    }
    return std::vector<uint32_t>();
}

void
DomainManager::SetThresholds(const DomainThresholds& thresholds)
{
    NS_LOG_FUNCTION(this);
    m_thresholds = thresholds;
}

DomainThresholds
DomainManager::GetThresholds() const
{
    return m_thresholds;
}

void
DomainManager::UpdateLoadFactor(uint32_t domainId, double loadFactor)
{
    NS_LOG_FUNCTION(this << domainId << loadFactor);
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        it->second.loadFactor = loadFactor;
    }
}

bool
DomainManager::NeedsRebalancing() const
{
    for (const auto& pair : m_domains)
    {
        if (pair.second.loadFactor > m_thresholds.splitThreshold ||
            pair.second.loadFactor < m_thresholds.mergeThreshold)
        {
            return true;
        }
    }
    return false;
}

uint32_t
DomainManager::AutoRebalance()
{
    NS_LOG_FUNCTION(this);
    
    uint32_t operations = 0;
    
    // Find domains to split
    std::vector<uint32_t> toSplit;
    for (const auto& pair : m_domains)
    {
        if (pair.second.loadFactor > m_thresholds.splitThreshold)
        {
            toSplit.push_back(pair.first);
        }
    }
    
    for (uint32_t domainId : toSplit)
    {
        if (SplitDomain(domainId) > 0)
        {
            operations++;
        }
    }
    
    // Find domains to merge
    std::vector<uint32_t> lowLoadDomains;
    for (const auto& pair : m_domains)
    {
        if (pair.second.loadFactor < m_thresholds.mergeThreshold)
        {
            lowLoadDomains.push_back(pair.first);
        }
    }
    
    // Merge pairs of low-load domains
    while (lowLoadDomains.size() >= 2)
    {
        uint32_t a = lowLoadDomains.back();
        lowLoadDomains.pop_back();
        uint32_t b = lowLoadDomains.back();
        lowLoadDomains.pop_back();
        
        if (MergeDomain(a, b) > 0)
        {
            operations++;
        }
    }
    
    return operations;
}

void
DomainManager::SetStrategy(DomainStrategy strategy)
{
    NS_LOG_FUNCTION(this << static_cast<int>(strategy));
    m_strategy = strategy;
}

void
DomainManager::SetCustomStrategy(DomainStrategyCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_customStrategy = callback;
    m_strategy = DomainStrategy::CUSTOM;
}

uint32_t
DomainManager::AssignUserToDomain(uint32_t userId)
{
    NS_LOG_FUNCTION(this << userId);
    
    if (m_domains.empty())
    {
        NS_LOG_WARN("No domains available for assignment");
        return 0;
    }
    
    uint32_t targetDomain = 0;
    
    switch (m_strategy)
    {
        case DomainStrategy::CONSISTENT_HASH:
            targetDomain = ConsistentHashAssign(userId);
            break;
            
        case DomainStrategy::LOAD_AWARE:
        {
            // Assign to domain with lowest load
            double minLoad = std::numeric_limits<double>::max();
            for (const auto& pair : m_domains)
            {
                if (pair.second.loadFactor < minLoad)
                {
                    minLoad = pair.second.loadFactor;
                    targetDomain = pair.first;
                }
            }
            break;
        }
        
        case DomainStrategy::CUSTOM:
            if (!m_customStrategy.IsNull())
            {
                targetDomain = m_customStrategy(userId, m_domains);
            }
            break;
            
        default:
            targetDomain = ConsistentHashAssign(userId);
            break;
    }
    
    if (targetDomain > 0)
    {
        AddUser(targetDomain, userId);
    }
    
    return targetDomain;
}

void
DomainManager::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    m_eventBus = eventBus;
}

DomainMetrics
DomainManager::GetDomainMetrics(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    DomainMetrics metrics;
    metrics.domainId = domainId;
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        metrics.userCount = it->second.userIds.size();
        metrics.proxyCount = it->second.proxyIds.size();
        metrics.loadFactor = it->second.loadFactor;
    }
    
    return metrics;
}

void
DomainManager::SetShuffleFrequency(uint32_t domainId, double frequency)
{
    NS_LOG_FUNCTION(this << domainId << frequency);
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        it->second.shuffleFrequency = frequency;
    }
}

double
DomainManager::GetShuffleFrequency(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domains.find(domainId);
    if (it != m_domains.end())
    {
        return it->second.shuffleFrequency;
    }
    return 0.0;
}

uint32_t
DomainManager::ConsistentHashAssign(uint32_t userId) const
{
    if (m_domains.empty())
    {
        return 0;
    }
    
    // Simple hash-based assignment
    std::hash<uint32_t> hasher;
    size_t hash = hasher(userId);
    
    std::vector<uint32_t> domainIds = GetAllDomainIds();
    size_t index = hash % domainIds.size();
    
    return domainIds[index];
}

void
DomainManager::NotifyDomainEvent(EventType type, uint32_t domainId)
{
    if (m_eventBus != nullptr)
    {
        MtdEvent event(type, Simulator::Now().GetMilliSeconds());
        event.metadata["domainId"] = std::to_string(domainId);
        m_eventBus->Publish(event);
    }
}

void
DomainManager::NotifyUserMigration(uint32_t userId, uint32_t oldDomain, uint32_t newDomain)
{
    if (m_eventBus != nullptr)
    {
        MtdEvent event(EventType::USER_MIGRATED, Simulator::Now().GetMilliSeconds());
        event.metadata["userId"] = std::to_string(userId);
        event.metadata["oldDomain"] = std::to_string(oldDomain);
        event.metadata["newDomain"] = std::to_string(newDomain);
        m_eventBus->Publish(event);
    }
}

// ==================== MetricsApi ====================

NS_OBJECT_ENSURE_REGISTERED(MetricsApi);

TypeId
MetricsApi::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::MetricsApi")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<MetricsApi>();
    return tid;
}

MetricsApi::MetricsApi()
{
    NS_LOG_FUNCTION(this);
}

MetricsApi::~MetricsApi()
{
    NS_LOG_FUNCTION(this);
}

void
MetricsApi::SetDomainManager(Ptr<DomainManager> domainManager)
{
    NS_LOG_FUNCTION(this);
    m_domainManager = domainManager;
}

DomainMetrics
MetricsApi::GetDomainMetrics(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    if (m_domainManager != nullptr)
    {
        return m_domainManager->GetDomainMetrics(domainId);
    }
    
    auto it = m_metricsCache.find(domainId);
    if (it != m_metricsCache.end())
    {
        return it->second;
    }
    
    return DomainMetrics();
}

std::map<uint32_t, DomainMetrics>
MetricsApi::GetAllMetrics() const
{
    NS_LOG_FUNCTION(this);
    
    std::map<uint32_t, DomainMetrics> allMetrics;
    
    if (m_domainManager != nullptr)
    {
        for (uint32_t domainId : m_domainManager->GetAllDomainIds())
        {
            allMetrics[domainId] = m_domainManager->GetDomainMetrics(domainId);
        }
    }
    
    return allMetrics;
}

void
MetricsApi::UpdateThroughput(uint32_t domainId, double throughput)
{
    NS_LOG_FUNCTION(this << domainId << throughput);
    m_metricsCache[domainId].throughput = throughput;
}

void
MetricsApi::UpdateLatency(uint32_t domainId, double latency)
{
    NS_LOG_FUNCTION(this << domainId << latency);
    m_metricsCache[domainId].avgLatency = latency;
}

TrafficStats
MetricsApi::GetTrafficData(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_trafficStats.find(domainId);
    if (it != m_trafficStats.end())
    {
        return it->second;
    }
    return TrafficStats();
}

} // namespace mtd
} // namespace ns3
