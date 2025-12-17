/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Domain Manager for logical domain operations
 */

#ifndef MTD_DOMAIN_MANAGER_H
#define MTD_DOMAIN_MANAGER_H

#include "mtd-common.h"
#include "mtd-event-bus.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <map>
#include <vector>

namespace ns3 {
namespace mtd {

/**
 * \brief Domain load thresholds for split/merge operations
 */
struct DomainThresholds {
    double splitThreshold;   ///< Load factor above which to split
    double mergeThreshold;   ///< Load factor below which to merge
    uint32_t minProxies;     ///< Minimum proxies per domain
    uint32_t maxProxies;     ///< Maximum proxies per domain
    uint32_t minUsers;       ///< Minimum users per domain
    uint32_t maxUsers;       ///< Maximum users per domain
    
    DomainThresholds()
        : splitThreshold(0.8),
          mergeThreshold(0.2),
          minProxies(2),
          maxProxies(20),
          minUsers(10),
          maxUsers(500) {}
};

/**
 * \brief Domain strategy type
 */
enum class DomainStrategy {
    CONSISTENT_HASH,    ///< Initial static assignment using consistent hashing
    TRAFFIC_CLUSTER,    ///< Traffic-based clustering
    LOAD_AWARE,         ///< Load-aware migration
    GEOGRAPHIC,         ///< Geographic proximity
    CUSTOM              ///< Custom user-defined strategy
};

// Custom strategy callback type
typedef Callback<uint32_t, uint32_t, const std::map<uint32_t, Domain>&> DomainStrategyCallback;

/**
 * \brief Domain Manager for managing logical domains
 * 
 * Handles domain creation, split, merge, and user migration
 * for load balancing and attack surface dispersion.
 */
class DomainManager : public Object
{
public:
    static TypeId GetTypeId();
    
    DomainManager();
    ~DomainManager() override;
    
    /**
     * \brief Create a new domain
     * \param name Domain name
     * \return Domain ID
     */
    uint32_t CreateDomain(const std::string& name);
    
    /**
     * \brief Delete a domain (moves users to other domains first)
     * \param domainId The domain ID
     * \return True if successful
     */
    bool DeleteDomain(uint32_t domainId);
    
    /**
     * \brief Get domain for a user
     * \param userId The user ID
     * \return Domain ID (0 if not found)
     */
    uint32_t GetDomain(uint32_t userId) const;
    
    /**
     * \brief Move user to a new domain
     * \param userId The user ID
     * \param newDomainId Target domain ID
     * \return True if successful
     */
    bool MoveUser(uint32_t userId, uint32_t newDomainId);
    
    /**
     * \brief Split a domain into two
     * \param domainId The domain ID to split
     * \return ID of the new domain, 0 if failed
     */
    uint32_t SplitDomain(uint32_t domainId);
    
    /**
     * \brief Merge two domains
     * \param domainIdA First domain ID
     * \param domainIdB Second domain ID
     * \return ID of the merged domain
     */
    uint32_t MergeDomain(uint32_t domainIdA, uint32_t domainIdB);
    
    /**
     * \brief Add a proxy to a domain
     * \param domainId The domain ID
     * \param proxyId The proxy ID
     * \return True if successful
     */
    bool AddProxy(uint32_t domainId, uint32_t proxyId);
    
    /**
     * \brief Remove a proxy from a domain
     * \param domainId The domain ID
     * \param proxyId The proxy ID
     * \return True if successful
     */
    bool RemoveProxy(uint32_t domainId, uint32_t proxyId);
    
    /**
     * \brief Add a user to a domain
     * \param domainId The domain ID
     * \param userId The user ID
     * \return True if successful
     */
    bool AddUser(uint32_t domainId, uint32_t userId);
    
    /**
     * \brief Remove a user from a domain
     * \param userId The user ID
     * \return True if successful
     */
    bool RemoveUser(uint32_t userId);
    
    /**
     * \brief Get domain information
     * \param domainId The domain ID
     * \return Domain structure
     */
    Domain GetDomainInfo(uint32_t domainId) const;
    
    /**
     * \brief Get all domain IDs
     * \return Vector of domain IDs
     */
    std::vector<uint32_t> GetAllDomainIds() const;
    
    /**
     * \brief Get users in a domain
     * \param domainId The domain ID
     * \return Vector of user IDs
     */
    std::vector<uint32_t> GetDomainUsers(uint32_t domainId) const;
    
    /**
     * \brief Get proxies in a domain
     * \param domainId The domain ID
     * \return Vector of proxy IDs
     */
    std::vector<uint32_t> GetDomainProxies(uint32_t domainId) const;
    
    /**
     * \brief Set domain thresholds
     * \param thresholds Threshold configuration
     */
    void SetThresholds(const DomainThresholds& thresholds);
    
    /**
     * \brief Get current thresholds
     * \return Threshold configuration
     */
    DomainThresholds GetThresholds() const;
    
    /**
     * \brief Update domain load factor
     * \param domainId The domain ID
     * \param loadFactor New load factor (0.0-1.0)
     */
    void UpdateLoadFactor(uint32_t domainId, double loadFactor);
    
    /**
     * \brief Check if any domains need split/merge
     * \return True if rebalancing needed
     */
    bool NeedsRebalancing() const;
    
    /**
     * \brief Auto-rebalance domains based on thresholds
     * \return Number of operations performed
     */
    uint32_t AutoRebalance();
    
    /**
     * \brief Set domain assignment strategy
     * \param strategy The strategy type
     */
    void SetStrategy(DomainStrategy strategy);
    
    /**
     * \brief Set custom domain strategy callback
     * \param callback Custom strategy function
     */
    void SetCustomStrategy(DomainStrategyCallback callback);
    
    /**
     * \brief Assign user to domain based on current strategy
     * \param userId The user ID
     * \return Assigned domain ID
     */
    uint32_t AssignUserToDomain(uint32_t userId);
    
    /**
     * \brief Set event bus for notifications
     * \param eventBus Pointer to event bus
     */
    void SetEventBus(Ptr<EventBus> eventBus);
    
    /**
     * \brief Get domain metrics
     * \param domainId The domain ID
     * \return Domain metrics structure
     */
    DomainMetrics GetDomainMetrics(uint32_t domainId) const;
    
    /**
     * \brief Set shuffle frequency for a domain
     * \param domainId The domain ID
     * \param frequency Shuffle frequency in seconds
     */
    void SetShuffleFrequency(uint32_t domainId, double frequency);
    
    /**
     * \brief Get shuffle frequency for a domain
     * \param domainId The domain ID
     * \return Shuffle frequency in seconds
     */
    double GetShuffleFrequency(uint32_t domainId) const;

private:
    std::map<uint32_t, Domain> m_domains;
    std::map<uint32_t, uint32_t> m_userToDomain;
    std::map<uint32_t, uint32_t> m_proxyToDomain;
    DomainThresholds m_thresholds;
    DomainStrategy m_strategy;
    DomainStrategyCallback m_customStrategy;
    Ptr<EventBus> m_eventBus;
    uint32_t m_nextDomainId;
    
    uint32_t ConsistentHashAssign(uint32_t userId) const;
    void NotifyDomainEvent(EventType type, uint32_t domainId);
    void NotifyUserMigration(uint32_t userId, uint32_t oldDomain, uint32_t newDomain);
};

/**
 * \brief Metrics API for domain performance monitoring
 */
class MetricsApi : public Object
{
public:
    static TypeId GetTypeId();
    
    MetricsApi();
    ~MetricsApi() override;
    
    /**
     * \brief Set domain manager reference
     * \param domainManager Pointer to domain manager
     */
    void SetDomainManager(Ptr<DomainManager> domainManager);
    
    /**
     * \brief Get metrics for a specific domain
     * \param domainId The domain ID
     * \return Domain metrics
     */
    DomainMetrics GetDomainMetrics(uint32_t domainId) const;
    
    /**
     * \brief Get aggregate metrics for all domains
     * \return Map of domain ID to metrics
     */
    std::map<uint32_t, DomainMetrics> GetAllMetrics() const;
    
    /**
     * \brief Update throughput metric for a domain
     * \param domainId The domain ID
     * \param throughput Throughput value
     */
    void UpdateThroughput(uint32_t domainId, double throughput);
    
    /**
     * \brief Update latency metric for a domain
     * \param domainId The domain ID
     * \param latency Latency value
     */
    void UpdateLatency(uint32_t domainId, double latency);
    
    /**
     * \brief Get traffic data for a domain
     * \param domainId The domain ID
     * \return Traffic statistics
     */
    TrafficStats GetTrafficData(uint32_t domainId) const;

private:
    Ptr<DomainManager> m_domainManager;
    std::map<uint32_t, DomainMetrics> m_metricsCache;
    std::map<uint32_t, TrafficStats> m_trafficStats;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_DOMAIN_MANAGER_H
