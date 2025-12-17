/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Shuffle Controller implementation
 */

#include "mtd-shuffle-controller.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <numeric>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdShuffleController");

// ==================== ShuffleController ====================

NS_OBJECT_ENSURE_REGISTERED(ShuffleController);

TypeId
ShuffleController::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::ShuffleController")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<ShuffleController>();
    return tid;
}

ShuffleController::ShuffleController()
    : m_totalShuffles(0),
      m_successfulShuffles(0)
{
    NS_LOG_FUNCTION(this);
    
    // Initialize random number generator
    m_rng = CreateObject<UniformRandomVariable>();
}

ShuffleController::~ShuffleController()
{
    NS_LOG_FUNCTION(this);
    
    // Cancel all periodic events
    for (auto& pair : m_periodicEvents)
    {
        Simulator::Cancel(pair.second);
    }
}

void
ShuffleController::SetConfig(const ShuffleConfig& config)
{
    NS_LOG_FUNCTION(this);
    m_config = config;
}

ShuffleConfig
ShuffleController::GetConfig() const
{
    return m_config;
}

void
ShuffleController::SetDomainManager(Ptr<DomainManager> domainManager)
{
    NS_LOG_FUNCTION(this);
    m_domainManager = domainManager;
}

void
ShuffleController::SetScoreManager(Ptr<ScoreManager> scoreManager)
{
    NS_LOG_FUNCTION(this);
    m_scoreManager = scoreManager;
}

void
ShuffleController::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    m_eventBus = eventBus;
}

ShuffleEvent
ShuffleController::TriggerShuffle(uint32_t domainId, ShuffleMode mode)
{
    NS_LOG_FUNCTION(this << domainId << static_cast<int>(mode));
    
    ShuffleEvent event;
    event.domainId = domainId;
    event.timestamp = Simulator::Now().GetMilliSeconds();
    event.strategy = SwitchStrategy::ADAPTIVE; // Map from ShuffleMode
    
    if (m_domainManager == nullptr)
    {
        NS_LOG_WARN("Domain manager not set");
        event.success = false;
        event.reason = "Domain manager not set";
        return event;
    }
    
    // Get domain info
    Domain domain = m_domainManager->GetDomainInfo(domainId);
    if (domain.domainId == 0)
    {
        NS_LOG_WARN("Domain " << domainId << " not found");
        event.success = false;
        event.reason = "Domain not found";
        return event;
    }
    
    std::vector<uint32_t> availableProxies = domain.proxyIds;
    if (availableProxies.empty())
    {
        NS_LOG_WARN("No proxies available in domain " << domainId);
        event.success = false;
        event.reason = "No proxies available";
        return event;
    }
    
    uint64_t startTime = Simulator::Now().GetMicroSeconds();
    uint32_t usersShuffled = 0;
    
    // Get users to shuffle
    std::vector<uint32_t> users = domain.userIds;
    
    // Apply batch limit
    if (users.size() > m_config.batchSize)
    {
        // Fisher-Yates shuffle using ns3 RNG
        for (std::size_t i = users.size() - 1; i > 0; --i)
        {
            std::size_t j = m_rng->GetInteger(0, i);
            std::swap(users[i], users[j]);
        }
        users.resize(m_config.batchSize);
    }
    
    // Shuffle each user
    for (uint32_t userId : users)
    {
        // Check session affinity
        if (m_config.sessionAffinity && IsInActiveSession(userId))
        {
            // Check timeout
            auto it = m_activeSessions.find(userId);
            if (it != m_activeSessions.end())
            {
                uint64_t sessionStart = it->second;
                double elapsed = (Simulator::Now().GetMilliSeconds() - sessionStart) / 1000.0;
                if (elapsed < m_config.sessionTimeout)
                {
                    continue; // Skip this user
                }
            }
        }
        
        uint32_t oldProxy = GetProxyAssignment(userId);
        uint32_t newProxy = SelectProxy(userId, mode, availableProxies);
        
        if (newProxy > 0 && newProxy != oldProxy)
        {
            m_userToProxy[userId] = newProxy;
            RecordProxyAssignment(userId, oldProxy, newProxy, false);
            NotifyProxySwitch(userId, oldProxy, newProxy);
            usersShuffled++;
        }
    }
    
    uint64_t endTime = Simulator::Now().GetMicroSeconds();
    
    event.usersAffected = usersShuffled;
    event.executionTime = (endTime - startTime) / 1000.0; // Convert to ms
    event.success = true;
    
    // Map ShuffleMode to SwitchStrategy
    switch (mode)
    {
        case ShuffleMode::RANDOM:
            event.strategy = SwitchStrategy::RANDOM;
            break;
        case ShuffleMode::SCORE_DRIVEN:
        case ShuffleMode::LOAD_BALANCED:
            event.strategy = SwitchStrategy::ADAPTIVE;
            break;
        default:
            event.strategy = SwitchStrategy::PERIODIC;
            break;
    }
    
    // Store event
    m_shuffleHistory[domainId].push_back(event);
    m_totalShuffles++;
    if (event.success)
    {
        m_successfulShuffles++;
    }
    
    NotifyShuffleEvent(event);
    
    NS_LOG_INFO("Shuffle completed for domain " << domainId 
                << ": " << usersShuffled << " users shuffled");
    
    return event;
}

void
ShuffleController::SetFrequency(uint32_t domainId, double frequency)
{
    NS_LOG_FUNCTION(this << domainId << frequency);
    
    // Clamp to valid range
    frequency = std::clamp(frequency, m_config.minFrequency, m_config.maxFrequency);
    m_domainFrequencies[domainId] = frequency;
    
    // Update domain manager if available
    if (m_domainManager != nullptr)
    {
        m_domainManager->SetShuffleFrequency(domainId, frequency);
    }
}

double
ShuffleController::GetFrequency(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domainFrequencies.find(domainId);
    if (it != m_domainFrequencies.end())
    {
        return it->second;
    }
    return m_config.baseFrequency;
}

void
ShuffleController::StartPeriodicShuffle(uint32_t domainId)
{
    NS_LOG_FUNCTION(this << domainId);
    
    // Cancel existing periodic event if any
    StopPeriodicShuffle(domainId);
    
    double frequency = GetFrequency(domainId);
    
    // Schedule first shuffle
    EventId eventId = Simulator::Schedule(Seconds(frequency),
        &ShuffleController::PerformPeriodicShuffle, this, domainId);
    m_periodicEvents[domainId] = eventId;
    
    NS_LOG_INFO("Started periodic shuffle for domain " << domainId 
                << " with frequency " << frequency << "s");
}

void
ShuffleController::StopPeriodicShuffle(uint32_t domainId)
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_periodicEvents.find(domainId);
    if (it != m_periodicEvents.end())
    {
        Simulator::Cancel(it->second);
        m_periodicEvents.erase(it);
    }
}

uint32_t
ShuffleController::GetProxyAssignment(uint32_t userId) const
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_userToProxy.find(userId);
    if (it != m_userToProxy.end())
    {
        return it->second;
    }
    return 0;
}

bool
ShuffleController::AssignUserToProxy(uint32_t userId, uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << userId << proxyId);
    
    uint32_t oldProxy = GetProxyAssignment(userId);
    m_userToProxy[userId] = proxyId;
    
    if (oldProxy != proxyId)
    {
        RecordProxyAssignment(userId, oldProxy, proxyId, IsInActiveSession(userId));
        NotifyProxySwitch(userId, oldProxy, proxyId);
    }
    
    return true;
}

std::vector<ShuffleEvent>
ShuffleController::GetShuffleHistory(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_shuffleHistory.find(domainId);
    if (it != m_shuffleHistory.end())
    {
        return it->second;
    }
    return std::vector<ShuffleEvent>();
}

void
ShuffleController::SetCustomStrategy(ShuffleStrategyCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_customStrategy = callback;
}

double
ShuffleController::CalculateAdaptiveFrequency(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    if (m_domainManager == nullptr || m_scoreManager == nullptr)
    {
        return m_config.baseFrequency;
    }
    
    // Get users in domain
    std::vector<uint32_t> users = m_domainManager->GetDomainUsers(domainId);
    if (users.empty())
    {
        return m_config.baseFrequency;
    }
    
    // Calculate average risk factor
    double totalRisk = 0.0;
    for (uint32_t userId : users)
    {
        double score = m_scoreManager->GetScore(userId);
        totalRisk += score;
    }
    double avgRisk = totalRisk / users.size();
    
    // Formula: f_domain = clamp(f_base * (1 + k·risk_factor), f_min, f_max)
    double adaptedFrequency = m_config.baseFrequency / (1.0 + m_config.riskFactor * avgRisk);
    
    return std::clamp(adaptedFrequency, m_config.minFrequency, m_config.maxFrequency);
}

std::vector<ProxyAssignment>
ShuffleController::GetUserProxyHistory(uint32_t userId) const
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_proxyHistory.find(userId);
    if (it != m_proxyHistory.end())
    {
        return it->second;
    }
    return std::vector<ProxyAssignment>();
}

bool
ShuffleController::IsInActiveSession(uint32_t userId) const
{
    return m_activeSessions.find(userId) != m_activeSessions.end();
}

void
ShuffleController::StartSession(uint32_t userId)
{
    NS_LOG_FUNCTION(this << userId);
    m_activeSessions[userId] = Simulator::Now().GetMilliSeconds();
}

void
ShuffleController::EndSession(uint32_t userId)
{
    NS_LOG_FUNCTION(this << userId);
    m_activeSessions.erase(userId);
}

uint64_t
ShuffleController::GetTotalShuffleCount() const
{
    return m_totalShuffles;
}

std::map<std::string, double>
ShuffleController::GetShuffleStats() const
{
    std::map<std::string, double> stats;
    stats["totalShuffles"] = static_cast<double>(m_totalShuffles);
    stats["successfulShuffles"] = static_cast<double>(m_successfulShuffles);
    stats["successRate"] = m_totalShuffles > 0 ? 
        static_cast<double>(m_successfulShuffles) / m_totalShuffles : 0.0;
    stats["activeSessions"] = static_cast<double>(m_activeSessions.size());
    stats["trackedUsers"] = static_cast<double>(m_userToProxy.size());
    return stats;
}

uint32_t
ShuffleController::SelectProxy(uint32_t userId, ShuffleMode mode,
                               const std::vector<uint32_t>& availableProxies)
{
    NS_LOG_FUNCTION(this << userId << static_cast<int>(mode));
    
    if (availableProxies.empty())
    {
        return 0;
    }
    
    switch (mode)
    {
        case ShuffleMode::RANDOM:
        {
            std::size_t idx = m_rng->GetInteger(0, availableProxies.size() - 1);
            return availableProxies[idx];
        }
        
        case ShuffleMode::SCORE_DRIVEN:
        {
            if (m_scoreManager != nullptr)
            {
                UserScore score = m_scoreManager->GetUserScore(userId);
                // High risk users get shuffled more aggressively to different proxies
                if (score.riskLevel == RiskLevel::HIGH || score.riskLevel == RiskLevel::CRITICAL)
                {
                    // Avoid current proxy
                    uint32_t currentProxy = GetProxyAssignment(userId);
                    std::vector<uint32_t> alternatives;
                    for (uint32_t proxy : availableProxies)
                    {
                        if (proxy != currentProxy)
                        {
                            alternatives.push_back(proxy);
                        }
                    }
                    if (!alternatives.empty())
                    {
                        std::size_t idx = m_rng->GetInteger(0, alternatives.size() - 1);
                        return alternatives[idx];
                    }
                }
            }
            // Fall back to random
            std::size_t idx = m_rng->GetInteger(0, availableProxies.size() - 1);
            return availableProxies[idx];
        }
        
        case ShuffleMode::ROUND_ROBIN:
        {
            uint32_t currentProxy = GetProxyAssignment(userId);
            auto it = std::find(availableProxies.begin(), availableProxies.end(), currentProxy);
            if (it != availableProxies.end())
            {
                size_t currentIdx = std::distance(availableProxies.begin(), it);
                size_t nextIdx = (currentIdx + 1) % availableProxies.size();
                return availableProxies[nextIdx];
            }
            return availableProxies[0];
        }
        
        case ShuffleMode::ATTACKER_AVOID:
        {
            // Avoid proxies that have high traffic (potential attack targets)
            // This is a simplified implementation
            uint32_t currentProxy = GetProxyAssignment(userId);
            std::vector<uint32_t> alternatives;
            for (uint32_t proxy : availableProxies)
            {
                if (proxy != currentProxy)
                {
                    alternatives.push_back(proxy);
                }
            }
            if (!alternatives.empty())
            {
                std::size_t idx = m_rng->GetInteger(0, alternatives.size() - 1);
                return alternatives[idx];
            }
            return availableProxies[0];
        }
        
        case ShuffleMode::CUSTOM:
        {
            if (!m_customStrategy.IsNull() && m_scoreManager != nullptr)
            {
                UserScore score = m_scoreManager->GetUserScore(userId);
                return m_customStrategy(userId, availableProxies, score);
            }
            // Fall back to random
            std::size_t idx = m_rng->GetInteger(0, availableProxies.size() - 1);
            return availableProxies[idx];
        }
        
        default:
        {
            std::size_t idx = m_rng->GetInteger(0, availableProxies.size() - 1);
            return availableProxies[idx];
        }
    }
}

void
ShuffleController::PerformPeriodicShuffle(uint32_t domainId)
{
    NS_LOG_FUNCTION(this << domainId);
    
    // Perform shuffle
    TriggerShuffle(domainId, ShuffleMode::SCORE_DRIVEN);
    
    // Calculate next frequency (potentially adaptive)
    double nextFrequency = CalculateAdaptiveFrequency(domainId);
    m_domainFrequencies[domainId] = nextFrequency;
    
    // Schedule next shuffle
    EventId eventId = Simulator::Schedule(Seconds(nextFrequency),
        &ShuffleController::PerformPeriodicShuffle, this, domainId);
    m_periodicEvents[domainId] = eventId;
}

void
ShuffleController::NotifyShuffleEvent(const ShuffleEvent& event)
{
    if (m_eventBus != nullptr)
    {
        MtdEvent mtdEvent(EventType::SHUFFLE_COMPLETED, event.timestamp);
        mtdEvent.metadata["domainId"] = std::to_string(event.domainId);
        mtdEvent.metadata["usersAffected"] = std::to_string(event.usersAffected);
        mtdEvent.metadata["executionTime"] = std::to_string(event.executionTime);
        mtdEvent.metadata["success"] = event.success ? "true" : "false";
        m_eventBus->Publish(mtdEvent);
    }
}

void
ShuffleController::NotifyProxySwitch(uint32_t userId, uint32_t oldProxy, uint32_t newProxy)
{
    if (m_eventBus != nullptr)
    {
        MtdEvent event(EventType::PROXY_SWITCHED, Simulator::Now().GetMilliSeconds());
        event.metadata["userId"] = std::to_string(userId);
        event.metadata["oldProxy"] = std::to_string(oldProxy);
        event.metadata["newProxy"] = std::to_string(newProxy);
        m_eventBus->Publish(event);
    }
}

void
ShuffleController::RecordProxyAssignment(uint32_t userId, uint32_t oldProxy,
                                          uint32_t newProxy, bool sessionPreserved)
{
    ProxyAssignment assignment;
    assignment.userId = userId;
    assignment.oldProxyId = oldProxy;
    assignment.newProxyId = newProxy;
    assignment.timestamp = Simulator::Now().GetMilliSeconds();
    assignment.sessionPreserved = sessionPreserved;
    
    m_proxyHistory[userId].push_back(assignment);
    
    // Limit history size
    if (m_proxyHistory[userId].size() > 100)
    {
        m_proxyHistory[userId].erase(m_proxyHistory[userId].begin());
    }
}

// ==================== TrafficDataApi ====================

NS_OBJECT_ENSURE_REGISTERED(TrafficDataApi);

TypeId
TrafficDataApi::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::TrafficDataApi")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<TrafficDataApi>();
    return tid;
}

TrafficDataApi::TrafficDataApi()
{
    NS_LOG_FUNCTION(this);
}

TrafficDataApi::~TrafficDataApi()
{
    NS_LOG_FUNCTION(this);
}

void
TrafficDataApi::SetDomainManager(Ptr<DomainManager> domainManager)
{
    NS_LOG_FUNCTION(this);
    m_domainManager = domainManager;
}

TrafficStats
TrafficDataApi::GetTrafficData(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    auto it = m_domainStats.find(domainId);
    if (it != m_domainStats.end())
    {
        return it->second;
    }
    return GetAggregateTraffic(domainId);
}

TrafficStats
TrafficDataApi::GetProxyTrafficData(uint32_t proxyId) const
{
    NS_LOG_FUNCTION(this << proxyId);
    
    auto it = m_proxyStats.find(proxyId);
    if (it != m_proxyStats.end())
    {
        return it->second;
    }
    return TrafficStats();
}

void
TrafficDataApi::UpdateProxyStats(uint32_t proxyId, const TrafficStats& stats)
{
    NS_LOG_FUNCTION(this << proxyId);
    m_proxyStats[proxyId] = stats;
}

TrafficStats
TrafficDataApi::GetAggregateTraffic(uint32_t domainId) const
{
    NS_LOG_FUNCTION(this << domainId);
    
    TrafficStats aggregate;
    
    if (m_domainManager != nullptr)
    {
        std::vector<uint32_t> proxies = m_domainManager->GetDomainProxies(domainId);
        for (uint32_t proxyId : proxies)
        {
            auto it = m_proxyStats.find(proxyId);
            if (it != m_proxyStats.end())
            {
                aggregate.packetsIn += it->second.packetsIn;
                aggregate.packetsOut += it->second.packetsOut;
                aggregate.bytesIn += it->second.bytesIn;
                aggregate.bytesOut += it->second.bytesOut;
                aggregate.packetRate += it->second.packetRate;
                aggregate.byteRate += it->second.byteRate;
                aggregate.activeConnections += it->second.activeConnections;
            }
        }
        
        // Average latency
        if (!proxies.empty())
        {
            double totalLatency = 0.0;
            uint32_t count = 0;
            for (uint32_t proxyId : proxies)
            {
                auto it = m_proxyStats.find(proxyId);
                if (it != m_proxyStats.end())
                {
                    totalLatency += it->second.avgLatency;
                    count++;
                }
            }
            if (count > 0)
            {
                aggregate.avgLatency = totalLatency / count;
            }
        }
    }
    
    return aggregate;
}

} // namespace mtd
} // namespace ns3
