/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Shuffle Controller for MTD proxy switching
 */

#ifndef MTD_SHUFFLE_CONTROLLER_H
#define MTD_SHUFFLE_CONTROLLER_H

#include "mtd-common.h"
#include "mtd-event-bus.h"
#include "mtd-domain-manager.h"
#include "mtd-score-manager.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/timer.h"
#include "ns3/event-id.h"

#include <map>
#include <vector>

#include "ns3/random-variable-stream.h"

namespace ns3 {
namespace mtd {

/**
 * \brief Shuffle mode/strategy
 */
enum class ShuffleMode {
    RANDOM,           ///< Random proxy assignment
    SCORE_DRIVEN,     ///< Risk score based assignment
    ROUND_ROBIN,      ///< Group rotation
    ATTACKER_AVOID,   ///< Avoid suspected attacker patterns
    LOAD_BALANCED,    ///< Balance load across proxies
    CUSTOM            ///< Custom user-defined strategy
};

/**
 * \brief Shuffle configuration
 */
struct ShuffleConfig {
    double baseFrequency;          ///< Base shuffle frequency (seconds)
    double minFrequency;           ///< Minimum frequency
    double maxFrequency;           ///< Maximum frequency
    double riskFactor;             ///< Risk multiplier for adaptive frequency
    bool sessionAffinity;          ///< Enable session preservation
    double sessionTimeout;         ///< Session timeout before forced switch
    uint32_t batchSize;            ///< Max users to shuffle at once
    
    ShuffleConfig()
        : baseFrequency(30.0),
          minFrequency(5.0),
          maxFrequency(120.0),
          riskFactor(1.5),
          sessionAffinity(true),
          sessionTimeout(300.0),
          batchSize(50) {}
};

// Custom shuffle strategy callback
typedef Callback<uint32_t, uint32_t, const std::vector<uint32_t>&, const UserScore&> ShuffleStrategyCallback;

/**
 * \brief Shuffle Controller for MTD proxy switching
 * 
 * Manages the dynamic remapping of users to proxy nodes,
 * supporting multiple strategies and session preservation.
 */
class ShuffleController : public Object
{
public:
    static TypeId GetTypeId();
    
    ShuffleController();
    ~ShuffleController() override;
    
    /**
     * \brief Set shuffle configuration
     * \param config Configuration structure
     */
    void SetConfig(const ShuffleConfig& config);
    
    /**
     * \brief Get current configuration
     * \return Configuration structure
     */
    ShuffleConfig GetConfig() const;
    
    /**
     * \brief Set domain manager reference
     * \param domainManager Pointer to domain manager
     */
    void SetDomainManager(Ptr<DomainManager> domainManager);
    
    /**
     * \brief Set score manager reference
     * \param scoreManager Pointer to score manager
     */
    void SetScoreManager(Ptr<ScoreManager> scoreManager);
    
    /**
     * \brief Set event bus reference
     * \param eventBus Pointer to event bus
     */
    void SetEventBus(Ptr<EventBus> eventBus);
    
    /**
     * \brief Trigger shuffle for a domain
     * \param domainId The domain ID
     * \param mode The shuffle mode/strategy
     * \return Shuffle event record
     */
    ShuffleEvent TriggerShuffle(uint32_t domainId, ShuffleMode mode);
    
    /**
     * \brief Set shuffle frequency for a domain
     * \param domainId The domain ID
     * \param frequency Frequency in seconds
     */
    void SetFrequency(uint32_t domainId, double frequency);
    
    /**
     * \brief Get shuffle frequency for a domain
     * \param domainId The domain ID
     * \return Frequency in seconds
     */
    double GetFrequency(uint32_t domainId) const;
    
    /**
     * \brief Start automatic periodic shuffling
     * \param domainId The domain ID
     */
    void StartPeriodicShuffle(uint32_t domainId);
    
    /**
     * \brief Stop automatic periodic shuffling
     * \param domainId The domain ID
     */
    void StopPeriodicShuffle(uint32_t domainId);
    
    /**
     * \brief Get proxy assignment for a user
     * \param userId The user ID
     * \return Current proxy ID
     */
    uint32_t GetProxyAssignment(uint32_t userId) const;
    
    /**
     * \brief Manually assign user to proxy
     * \param userId The user ID
     * \param proxyId The proxy ID
     * \return True if successful
     */
    bool AssignUserToProxy(uint32_t userId, uint32_t proxyId);
    
    /**
     * \brief Get shuffle history for a domain
     * \param domainId The domain ID
     * \return Vector of shuffle events
     */
    std::vector<ShuffleEvent> GetShuffleHistory(uint32_t domainId) const;
    
    /**
     * \brief Set custom shuffle strategy
     * \param callback Custom strategy function
     */
    void SetCustomStrategy(ShuffleStrategyCallback callback);
    
    /**
     * \brief Calculate adaptive frequency based on risk
     * \param domainId The domain ID
     * \return Calculated frequency
     */
    double CalculateAdaptiveFrequency(uint32_t domainId) const;
    
    /**
     * \brief Get proxy assignment history for a user
     * \param userId The user ID
     * \return Vector of proxy assignments
     */
    std::vector<ProxyAssignment> GetUserProxyHistory(uint32_t userId) const;
    
    /**
     * \brief Check if a user is in an active session
     * \param userId The user ID
     * \return True if in active session
     */
    bool IsInActiveSession(uint32_t userId) const;
    
    /**
     * \brief Register user session start
     * \param userId The user ID
     */
    void StartSession(uint32_t userId);
    
    /**
     * \brief End user session
     * \param userId The user ID
     */
    void EndSession(uint32_t userId);
    
    /**
     * \brief Get total shuffle count
     * \return Total number of shuffles performed
     */
    uint64_t GetTotalShuffleCount() const;
    
    /**
     * \brief Get shuffle statistics
     * \return Map of metric name to value
     */
    std::map<std::string, double> GetShuffleStats() const;

private:
    ShuffleConfig m_config;
    Ptr<DomainManager> m_domainManager;
    Ptr<ScoreManager> m_scoreManager;
    Ptr<EventBus> m_eventBus;
    
    std::map<uint32_t, uint32_t> m_userToProxy;
    std::map<uint32_t, std::vector<ProxyAssignment>> m_proxyHistory;
    std::map<uint32_t, std::vector<ShuffleEvent>> m_shuffleHistory;
    std::map<uint32_t, double> m_domainFrequencies;
    std::map<uint32_t, EventId> m_periodicEvents;
    std::map<uint32_t, uint64_t> m_activeSessions;
    
    ShuffleStrategyCallback m_customStrategy;
    Ptr<UniformRandomVariable> m_rng;
    uint64_t m_totalShuffles;
    uint64_t m_successfulShuffles;
    
    uint32_t SelectProxy(uint32_t userId, ShuffleMode mode, 
                         const std::vector<uint32_t>& availableProxies);
    void PerformPeriodicShuffle(uint32_t domainId);
    void NotifyShuffleEvent(const ShuffleEvent& event);
    void NotifyProxySwitch(uint32_t userId, uint32_t oldProxy, uint32_t newProxy);
    void RecordProxyAssignment(uint32_t userId, uint32_t oldProxy, 
                               uint32_t newProxy, bool sessionPreserved);
};

/**
 * \brief Traffic Data API for shuffle decisions
 */
class TrafficDataApi : public Object
{
public:
    static TypeId GetTypeId();
    
    TrafficDataApi();
    ~TrafficDataApi() override;
    
    /**
     * \brief Set domain manager reference
     * \param domainManager Pointer to domain manager
     */
    void SetDomainManager(Ptr<DomainManager> domainManager);
    
    /**
     * \brief Get traffic data for a domain
     * \param domainId The domain ID
     * \return Traffic statistics
     */
    TrafficStats GetTrafficData(uint32_t domainId) const;
    
    /**
     * \brief Get traffic data for a proxy
     * \param proxyId The proxy ID
     * \return Traffic statistics
     */
    TrafficStats GetProxyTrafficData(uint32_t proxyId) const;
    
    /**
     * \brief Update proxy traffic statistics
     * \param proxyId The proxy ID
     * \param stats Updated statistics
     */
    void UpdateProxyStats(uint32_t proxyId, const TrafficStats& stats);
    
    /**
     * \brief Get aggregate domain traffic
     * \param domainId The domain ID
     * \return Aggregate statistics
     */
    TrafficStats GetAggregateTraffic(uint32_t domainId) const;

private:
    Ptr<DomainManager> m_domainManager;
    std::map<uint32_t, TrafficStats> m_proxyStats;
    std::map<uint32_t, TrafficStats> m_domainStats;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_SHUFFLE_CONTROLLER_H
