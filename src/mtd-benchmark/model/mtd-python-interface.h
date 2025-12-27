/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Python Interface for External Defense Algorithm Integration
 * 
 * This module provides a bridge between NS-3 simulation and Python-based
 * defense algorithms, enabling researchers to rapidly prototype and test
 * defense strategies without recompiling C++ code.
 */

#ifndef MTD_PYTHON_INTERFACE_H
#define MTD_PYTHON_INTERFACE_H

#include "mtd-common.h"
#include "mtd-event-bus.h"
#include "mtd-score-manager.h"
#include "mtd-domain-manager.h"
#include "mtd-shuffle-controller.h"
#include "mtd-detector.h"
#include "mtd-export-api.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/simulator.h"

#include <functional>
#include <map>
#include <memory>
#include <mutex>
#include <string>
#include <vector>

namespace ns3 {
namespace mtd {

// Forward declarations
class PythonAlgorithmBridge;
class SimulationContext;

/**
 * \brief Data structure for passing simulation state to Python
 */
struct SimulationState {
    uint64_t currentTime;                              ///< Current simulation time (ns)
    std::map<uint32_t, Domain> domains;                ///< All domains and their data
    std::map<uint32_t, UserScore> userScores;          ///< All user risk scores
    std::map<uint32_t, TrafficStats> proxyStats;       ///< Traffic stats per proxy
    std::map<uint32_t, DetectionObservation> observations; ///< Recent detections
    std::vector<MtdEvent> recentEvents;                ///< Recent event history
    
    SimulationState() : currentTime(0) {}
};

/**
 * \brief Defense decision structure returned from Python
 */
struct DefenseDecision {
    enum class ActionType {
        NO_ACTION,
        TRIGGER_SHUFFLE,
        MIGRATE_USER,
        SPLIT_DOMAIN,
        MERGE_DOMAINS,
        UPDATE_SCORE,
        CHANGE_FREQUENCY,
        CUSTOM
    };
    
    ActionType action;
    uint32_t targetDomainId;
    uint32_t targetUserId;
    uint32_t targetProxyId;
    uint32_t secondaryDomainId;
    double newScore;
    double newFrequency;
    ShuffleMode shuffleMode;
    std::map<std::string, std::string> customParams;
    std::string reason;
    
    DefenseDecision() 
        : action(ActionType::NO_ACTION),
          targetDomainId(0),
          targetUserId(0),
          targetProxyId(0),
          secondaryDomainId(0),
          newScore(0.0),
          newFrequency(0.0),
          shuffleMode(ShuffleMode::RANDOM) {}
};

/**
 * \brief Configuration for Python algorithm
 */
struct PythonAlgorithmConfig {
    std::string algorithmName;                     ///< Algorithm identifier
    std::string modulePath;                        ///< Path to Python module
    std::string className;                         ///< Python class name
    double evaluationInterval;                     ///< How often to call Python (seconds)
    bool enableParallel;                           ///< Enable parallel execution
    uint32_t maxDecisionsPerEval;                  ///< Max decisions per evaluation
    std::map<std::string, std::string> parameters; ///< Custom algorithm parameters
    
    PythonAlgorithmConfig()
        : algorithmName("DefaultAlgorithm"),
          modulePath(""),
          className("DefenseAlgorithm"),
          evaluationInterval(1.0),
          enableParallel(false),
          maxDecisionsPerEval(10) {}
};

// Callback types for Python bridge
using ScoreCalculator = std::function<double(uint32_t userId, const DetectionObservation& obs, double currentScore)>;
using RiskClassifier = std::function<RiskLevel(uint32_t userId, double score)>;
using ShuffleStrategy = std::function<uint32_t(uint32_t userId, const std::vector<uint32_t>& proxies, const UserScore& score)>;
using DomainAssigner = std::function<uint32_t(uint32_t userId, const std::map<uint32_t, Domain>& domains)>;
using DefenseEvaluator = std::function<std::vector<DefenseDecision>(const SimulationState& state)>;

/**
 * \brief Python Algorithm Bridge
 * 
 * Main interface for connecting Python defense algorithms to NS-3 simulation.
 * Handles callback registration, state transfer, and decision execution.
 */
class PythonAlgorithmBridge : public Object
{
public:
    static TypeId GetTypeId();
    
    PythonAlgorithmBridge();
    virtual ~PythonAlgorithmBridge();
    
    // ==================== Configuration ====================
    
    /**
     * \brief Set algorithm configuration
     * \param config Algorithm configuration
     */
    void SetConfig(const PythonAlgorithmConfig& config);
    
    /**
     * \brief Get current configuration
     * \return Algorithm configuration
     */
    PythonAlgorithmConfig GetConfig() const;
    
    /**
     * \brief Set custom algorithm parameter
     * \param key Parameter name
     * \param value Parameter value
     */
    void SetParameter(const std::string& key, const std::string& value);
    
    /**
     * \brief Get custom algorithm parameter
     * \param key Parameter name
     * \return Parameter value (empty if not found)
     */
    std::string GetParameter(const std::string& key) const;
    
    // ==================== Component Registration ====================
    
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
     * \brief Set shuffle controller reference
     * \param shuffleController Pointer to shuffle controller
     */
    void SetShuffleController(Ptr<ShuffleController> shuffleController);
    
    /**
     * \brief Set event bus reference
     * \param eventBus Pointer to event bus
     */
    void SetEventBus(Ptr<EventBus> eventBus);
    
    /**
     * \brief Set local detector reference
     * \param detector Pointer to local detector
     */
    void SetLocalDetector(Ptr<LocalDetector> detector);
    
    // ==================== Callback Registration ====================
    
    /**
     * \brief Register custom score calculation callback
     * 
     * The callback receives (userId, observation, currentScore) and returns newScore.
     * This replaces the default scoring algorithm in ScoreManager.
     * 
     * \param callback Score calculation function
     */
    void RegisterScoreCalculator(ScoreCalculator callback);
    
    /**
     * \brief Register custom risk classification callback
     * 
     * The callback receives (userId, score) and returns RiskLevel.
     * This replaces the default threshold-based classification.
     * 
     * \param callback Risk classification function
     */
    void RegisterRiskClassifier(RiskClassifier callback);
    
    /**
     * \brief Register custom shuffle strategy callback
     * 
     * The callback receives (userId, availableProxies, userScore) and returns proxyId.
     * This replaces the default shuffle strategy in ShuffleController.
     * 
     * \param callback Shuffle strategy function
     */
    void RegisterShuffleStrategy(ShuffleStrategy callback);
    
    /**
     * \brief Register custom domain assignment callback
     * 
     * The callback receives (userId, domains) and returns domainId.
     * This replaces the default domain assignment in DomainManager.
     * 
     * \param callback Domain assignment function
     */
    void RegisterDomainAssigner(DomainAssigner callback);
    
    /**
     * \brief Register main defense evaluation callback
     * 
     * The callback receives complete simulation state and returns a list of
     * defense decisions to execute. This is the main entry point for Python
     * algorithms that want full control over defense actions.
     * 
     * \param callback Defense evaluation function
     */
    void RegisterDefenseEvaluator(DefenseEvaluator callback);
    
    /**
     * \brief Clear all registered callbacks
     */
    void ClearCallbacks();
    
    // ==================== Simulation State ====================
    
    /**
     * \brief Get current simulation state
     * 
     * Collects and returns all relevant simulation data for Python processing.
     * 
     * \return SimulationState structure
     */
    SimulationState GetSimulationState() const;
    
    /**
     * \brief Get domain information
     * \param domainId Domain ID
     * \return Domain structure
     */
    Domain GetDomainState(uint32_t domainId) const;
    
    /**
     * \brief Get all user scores
     * \return Map of userId to UserScore
     */
    std::map<uint32_t, UserScore> GetAllUserScores() const;
    
    /**
     * \brief Get traffic statistics for all proxies
     * \return Map of proxyId to TrafficStats
     */
    std::map<uint32_t, TrafficStats> GetAllProxyStats() const;
    
    /**
     * \brief Get recent detection observations
     * \param count Maximum number of observations
     * \return Vector of observations
     */
    std::vector<DetectionObservation> GetRecentObservations(uint32_t count = 100) const;
    
    /**
     * \brief Get recent events from event bus
     * \param count Maximum number of events
     * \return Vector of events
     */
    std::vector<MtdEvent> GetRecentEvents(uint32_t count = 100) const;
    
    // ==================== Decision Execution ====================
    
    /**
     * \brief Execute a defense decision
     * 
     * Applies the decision to the appropriate NS-3 component.
     * 
     * \param decision Defense decision to execute
     * \return True if execution successful
     */
    bool ExecuteDecision(const DefenseDecision& decision);
    
    /**
     * \brief Execute multiple decisions
     * \param decisions Vector of decisions
     * \return Number of successful executions
     */
    uint32_t ExecuteDecisions(const std::vector<DefenseDecision>& decisions);
    
    // ==================== Direct Action APIs ====================
    
    /**
     * \brief Trigger shuffle for a domain
     * \param domainId Domain ID
     * \param mode Shuffle mode
     * \return True if successful
     */
    bool TriggerShuffle(uint32_t domainId, ShuffleMode mode);
    
    /**
     * \brief Migrate user to different domain
     * \param userId User ID
     * \param newDomainId Target domain ID
     * \return True if successful
     */
    bool MigrateUser(uint32_t userId, uint32_t newDomainId);
    
    /**
     * \brief Update user risk score directly
     * \param userId User ID
     * \param newScore New score value
     * \return True if successful
     */
    bool UpdateUserScore(uint32_t userId, double newScore);
    
    /**
     * \brief Change shuffle frequency for domain
     * \param domainId Domain ID
     * \param frequency New frequency (seconds)
     * \return True if successful
     */
    bool ChangeShuffleFrequency(uint32_t domainId, double frequency);
    
    /**
     * \brief Assign user to specific proxy
     * \param userId User ID
     * \param proxyId Proxy ID
     * \return True if successful
     */
    bool AssignUserToProxy(uint32_t userId, uint32_t proxyId);
    
    // ==================== Periodic Evaluation ====================
    
    /**
     * \brief Start periodic evaluation using registered defense evaluator
     */
    void StartPeriodicEvaluation();
    
    /**
     * \brief Stop periodic evaluation
     */
    void StopPeriodicEvaluation();
    
    /**
     * \brief Manually trigger one evaluation cycle
     * \return Number of decisions executed
     */
    uint32_t TriggerEvaluation();
    
    /**
     * \brief Check if periodic evaluation is running
     * \return True if running
     */
    bool IsEvaluationRunning() const;
    
    // ==================== Statistics ====================
    
    /**
     * \brief Get bridge statistics
     * \return Map of metric name to value
     */
    std::map<std::string, double> GetStatistics() const;
    
    /**
     * \brief Get decision execution history
     * \param count Maximum entries to return
     * \return Vector of (timestamp, decision, success) tuples
     */
    std::vector<std::tuple<uint64_t, DefenseDecision, bool>> GetDecisionHistory(uint32_t count = 100) const;
    
    /**
     * \brief Reset statistics
     */
    void ResetStatistics();

private:
    PythonAlgorithmConfig m_config;
    
    // Component references
    Ptr<DomainManager> m_domainManager;
    Ptr<ScoreManager> m_scoreManager;
    Ptr<ShuffleController> m_shuffleController;
    Ptr<EventBus> m_eventBus;
    Ptr<LocalDetector> m_localDetector;
    
    // Registered callbacks
    ScoreCalculator m_scoreCalculator;
    RiskClassifier m_riskClassifier;
    ShuffleStrategy m_shuffleStrategy;
    DomainAssigner m_domainAssigner;
    DefenseEvaluator m_defenseEvaluator;
    
    // Callback registration flags
    bool m_hasScoreCalculator;
    bool m_hasRiskClassifier;
    bool m_hasShuffleStrategy;
    bool m_hasDomainAssigner;
    bool m_hasDefenseEvaluator;
    
    // Periodic evaluation
    EventId m_evaluationEvent;
    bool m_evaluationRunning;
    
    // Statistics
    uint64_t m_totalEvaluations;
    uint64_t m_totalDecisions;
    uint64_t m_successfulDecisions;
    uint64_t m_failedDecisions;
    double m_avgEvaluationTime;
    
    // Decision history
    std::vector<std::tuple<uint64_t, DefenseDecision, bool>> m_decisionHistory;
    
    // Recent observations cache
    mutable std::vector<DetectionObservation> m_observationCache;
    
    // Internal methods
    void PerformEvaluation();
    void ApplyCallbacksToComponents();
    void RecordDecision(const DefenseDecision& decision, bool success);
};

/**
 * \brief Simulation Context for Python interface
 * 
 * Provides a simplified API for Python scripts to interact with
 * the simulation without needing to understand NS-3 internals.
 */
class SimulationContext : public Object
{
public:
    static TypeId GetTypeId();
    
    SimulationContext();
    virtual ~SimulationContext();
    
    /**
     * \brief Initialize context with all MTD components
     * \param bridge Python algorithm bridge
     * \param domainManager Domain manager
     * \param scoreManager Score manager
     * \param shuffleController Shuffle controller
     * \param eventBus Event bus
     */
    void Initialize(Ptr<PythonAlgorithmBridge> bridge,
                    Ptr<DomainManager> domainManager,
                    Ptr<ScoreManager> scoreManager,
                    Ptr<ShuffleController> shuffleController,
                    Ptr<EventBus> eventBus);
    
    /**
     * \brief Get the Python algorithm bridge
     * \return Pointer to bridge
     */
    Ptr<PythonAlgorithmBridge> GetBridge() const;
    
    /**
     * \brief Get current simulation time (seconds)
     * \return Current time
     */
    double GetCurrentTime() const;
    
    /**
     * \brief Get all domain IDs
     * \return Vector of domain IDs
     */
    std::vector<uint32_t> GetDomainIds() const;
    
    /**
     * \brief Get all user IDs
     * \return Vector of user IDs
     */
    std::vector<uint32_t> GetUserIds() const;
    
    /**
     * \brief Get all proxy IDs
     * \return Vector of proxy IDs
     */
    std::vector<uint32_t> GetProxyIds() const;
    
    /**
     * \brief Log message to NS-3 logging system
     * \param level Log level (0=debug, 1=info, 2=warn, 3=error)
     * \param message Log message
     */
    void Log(int level, const std::string& message);
    
    /**
     * \brief Schedule a custom event
     * \param delaySeconds Delay from current time
     * \param callback Callback function
     */
    void ScheduleEvent(double delaySeconds, std::function<void()> callback);

private:
    Ptr<PythonAlgorithmBridge> m_bridge;
    Ptr<DomainManager> m_domainManager;
    Ptr<ScoreManager> m_scoreManager;
    Ptr<ShuffleController> m_shuffleController;
    Ptr<EventBus> m_eventBus;
};

/**
 * \brief Helper function to create string from ShuffleMode enum
 */
std::string ShuffleModeToString(ShuffleMode mode);

/**
 * \brief Helper function to parse ShuffleMode from string
 */
ShuffleMode StringToShuffleMode(const std::string& str);

/**
 * \brief Helper function to create string from RiskLevel enum
 */
std::string RiskLevelToString(RiskLevel level);

/**
 * \brief Helper function to parse RiskLevel from string
 */
RiskLevel StringToRiskLevel(const std::string& str);

/**
 * \brief Helper function to create string from AttackType enum
 */
std::string AttackTypeToString(AttackType type);

/**
 * \brief Helper function to parse AttackType from string
 */
AttackType StringToAttackType(const std::string& str);

} // namespace mtd
} // namespace ns3

#endif // MTD_PYTHON_INTERFACE_H
