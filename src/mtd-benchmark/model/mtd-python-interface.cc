/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Python Interface Implementation
 */

#include "mtd-python-interface.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <chrono>
#include <sstream>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdPythonInterface");

// ==================== PythonAlgorithmBridge ====================

NS_OBJECT_ENSURE_REGISTERED(PythonAlgorithmBridge);

TypeId
PythonAlgorithmBridge::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::PythonAlgorithmBridge")
        .SetParent<Object>()
        .SetGroupName("Mtd")
        .AddConstructor<PythonAlgorithmBridge>();
    return tid;
}

PythonAlgorithmBridge::PythonAlgorithmBridge()
    : m_hasScoreCalculator(false),
      m_hasRiskClassifier(false),
      m_hasShuffleStrategy(false),
      m_hasDomainAssigner(false),
      m_hasDefenseEvaluator(false),
      m_evaluationRunning(false),
      m_totalEvaluations(0),
      m_totalDecisions(0),
      m_successfulDecisions(0),
      m_failedDecisions(0),
      m_avgEvaluationTime(0.0)
{
    NS_LOG_FUNCTION(this);
}

PythonAlgorithmBridge::~PythonAlgorithmBridge()
{
    NS_LOG_FUNCTION(this);
    StopPeriodicEvaluation();
}

void
PythonAlgorithmBridge::SetConfig(const PythonAlgorithmConfig& config)
{
    NS_LOG_FUNCTION(this << config.algorithmName);
    m_config = config;
}

PythonAlgorithmConfig
PythonAlgorithmBridge::GetConfig() const
{
    return m_config;
}

void
PythonAlgorithmBridge::SetParameter(const std::string& key, const std::string& value)
{
    NS_LOG_FUNCTION(this << key << value);
    m_config.parameters[key] = value;
}

std::string
PythonAlgorithmBridge::GetParameter(const std::string& key) const
{
    auto it = m_config.parameters.find(key);
    if (it != m_config.parameters.end())
    {
        return it->second;
    }
    return "";
}

void
PythonAlgorithmBridge::SetDomainManager(Ptr<DomainManager> domainManager)
{
    NS_LOG_FUNCTION(this << domainManager);
    m_domainManager = domainManager;
}

void
PythonAlgorithmBridge::SetScoreManager(Ptr<ScoreManager> scoreManager)
{
    NS_LOG_FUNCTION(this << scoreManager);
    m_scoreManager = scoreManager;
}

void
PythonAlgorithmBridge::SetShuffleController(Ptr<ShuffleController> shuffleController)
{
    NS_LOG_FUNCTION(this << shuffleController);
    m_shuffleController = shuffleController;
}

void
PythonAlgorithmBridge::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this << eventBus);
    m_eventBus = eventBus;
}

void
PythonAlgorithmBridge::SetLocalDetector(Ptr<LocalDetector> detector)
{
    NS_LOG_FUNCTION(this << detector);
    m_localDetector = detector;
}

void
PythonAlgorithmBridge::RegisterScoreCalculator(ScoreCalculator callback)
{
    NS_LOG_FUNCTION(this);
    m_scoreCalculator = callback;
    m_hasScoreCalculator = true;
    ApplyCallbacksToComponents();
}

void
PythonAlgorithmBridge::RegisterRiskClassifier(RiskClassifier callback)
{
    NS_LOG_FUNCTION(this);
    m_riskClassifier = callback;
    m_hasRiskClassifier = true;
    ApplyCallbacksToComponents();
}

void
PythonAlgorithmBridge::RegisterShuffleStrategy(ShuffleStrategy callback)
{
    NS_LOG_FUNCTION(this);
    m_shuffleStrategy = callback;
    m_hasShuffleStrategy = true;
    ApplyCallbacksToComponents();
}

void
PythonAlgorithmBridge::RegisterDomainAssigner(DomainAssigner callback)
{
    NS_LOG_FUNCTION(this);
    m_domainAssigner = callback;
    m_hasDomainAssigner = true;
    ApplyCallbacksToComponents();
}

void
PythonAlgorithmBridge::RegisterDefenseEvaluator(DefenseEvaluator callback)
{
    NS_LOG_FUNCTION(this);
    m_defenseEvaluator = callback;
    m_hasDefenseEvaluator = true;
}

void
PythonAlgorithmBridge::ClearCallbacks()
{
    NS_LOG_FUNCTION(this);
    m_scoreCalculator = nullptr;
    m_riskClassifier = nullptr;
    m_shuffleStrategy = nullptr;
    m_domainAssigner = nullptr;
    m_defenseEvaluator = nullptr;
    
    m_hasScoreCalculator = false;
    m_hasRiskClassifier = false;
    m_hasShuffleStrategy = false;
    m_hasDomainAssigner = false;
    m_hasDefenseEvaluator = false;
    
    // Clear callbacks from components
    if (m_scoreManager)
    {
        m_scoreManager->ClearCustomScoreCallback();
        m_scoreManager->ClearCustomRiskLevelCallback();
    }
}

SimulationState
PythonAlgorithmBridge::GetSimulationState() const
{
    NS_LOG_FUNCTION(this);
    
    SimulationState state;
    state.currentTime = Simulator::Now().GetNanoSeconds();
    
    // Collect domain information
    if (m_domainManager)
    {
        auto domainIds = m_domainManager->GetAllDomainIds();
        for (uint32_t domainId : domainIds)
        {
            state.domains[domainId] = m_domainManager->GetDomainInfo(domainId);
        }
    }
    
    // Collect user scores
    if (m_scoreManager)
    {
        auto userIds = m_scoreManager->GetTrackedUsers();
        for (uint32_t userId : userIds)
        {
            state.userScores[userId] = m_scoreManager->GetUserScore(userId);
        }
    }
    
    // Collect proxy traffic stats
    if (m_localDetector)
    {
        auto proxyIds = m_localDetector->GetMonitoredAgents();
        for (uint32_t proxyId : proxyIds)
        {
            state.proxyStats[proxyId] = m_localDetector->GetStats(proxyId);
            state.observations[proxyId] = m_localDetector->Analyze(proxyId);
        }
    }
    
    // Collect recent events
    if (m_eventBus)
    {
        state.recentEvents = m_eventBus->GetEventHistory();
    }
    
    return state;
}

Domain
PythonAlgorithmBridge::GetDomainState(uint32_t domainId) const
{
    if (m_domainManager)
    {
        return m_domainManager->GetDomainInfo(domainId);
    }
    return Domain();
}

std::map<uint32_t, UserScore>
PythonAlgorithmBridge::GetAllUserScores() const
{
    std::map<uint32_t, UserScore> scores;
    if (m_scoreManager)
    {
        auto userIds = m_scoreManager->GetTrackedUsers();
        for (uint32_t userId : userIds)
        {
            scores[userId] = m_scoreManager->GetUserScore(userId);
        }
    }
    return scores;
}

std::map<uint32_t, TrafficStats>
PythonAlgorithmBridge::GetAllProxyStats() const
{
    std::map<uint32_t, TrafficStats> stats;
    if (m_localDetector)
    {
        auto proxyIds = m_localDetector->GetMonitoredAgents();
        for (uint32_t proxyId : proxyIds)
        {
            stats[proxyId] = m_localDetector->GetStats(proxyId);
        }
    }
    return stats;
}

std::vector<DetectionObservation>
PythonAlgorithmBridge::GetRecentObservations(uint32_t count) const
{
    std::vector<DetectionObservation> observations;
    if (m_localDetector)
    {
        auto proxyIds = m_localDetector->GetMonitoredAgents();
        for (uint32_t proxyId : proxyIds)
        {
            observations.push_back(m_localDetector->Analyze(proxyId));
            if (observations.size() >= count)
            {
                break;
            }
        }
    }
    return observations;
}

std::vector<MtdEvent>
PythonAlgorithmBridge::GetRecentEvents(uint32_t count) const
{
    if (m_eventBus)
    {
        auto history = m_eventBus->GetEventHistory();
        if (history.size() > count)
        {
            return std::vector<MtdEvent>(history.end() - count, history.end());
        }
        return history;
    }
    return std::vector<MtdEvent>();
}

bool
PythonAlgorithmBridge::ExecuteDecision(const DefenseDecision& decision)
{
    NS_LOG_FUNCTION(this);
    
    bool success = false;
    
    try
    {
        switch (decision.action)
        {
        case DefenseDecision::ActionType::NO_ACTION:
            success = true;
            break;
            
        case DefenseDecision::ActionType::TRIGGER_SHUFFLE:
            success = TriggerShuffle(decision.targetDomainId, decision.shuffleMode);
            break;
            
        case DefenseDecision::ActionType::MIGRATE_USER:
            success = MigrateUser(decision.targetUserId, decision.targetDomainId);
            break;
            
        case DefenseDecision::ActionType::SPLIT_DOMAIN:
            if (m_domainManager)
            {
                success = (m_domainManager->SplitDomain(decision.targetDomainId) != 0);
            }
            break;
            
        case DefenseDecision::ActionType::MERGE_DOMAINS:
            if (m_domainManager)
            {
                success = (m_domainManager->MergeDomain(
                    decision.targetDomainId, decision.secondaryDomainId) != 0);
            }
            break;
            
        case DefenseDecision::ActionType::UPDATE_SCORE:
            success = UpdateUserScore(decision.targetUserId, decision.newScore);
            break;
            
        case DefenseDecision::ActionType::CHANGE_FREQUENCY:
            success = ChangeShuffleFrequency(decision.targetDomainId, decision.newFrequency);
            break;
            
        case DefenseDecision::ActionType::CUSTOM:
            NS_LOG_INFO("Custom action: " << decision.reason);
            success = true;
            break;
            
        default:
            NS_LOG_WARN("Unknown action type");
            success = false;
        }
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("Exception executing decision: " << e.what());
        success = false;
    }
    
    RecordDecision(decision, success);
    
    if (success)
    {
        m_successfulDecisions++;
    }
    else
    {
        m_failedDecisions++;
    }
    m_totalDecisions++;
    
    return success;
}

uint32_t
PythonAlgorithmBridge::ExecuteDecisions(const std::vector<DefenseDecision>& decisions)
{
    NS_LOG_FUNCTION(this << decisions.size());
    
    uint32_t successCount = 0;
    uint32_t maxDecisions = m_config.maxDecisionsPerEval;
    
    for (size_t i = 0; i < decisions.size() && i < maxDecisions; ++i)
    {
        if (ExecuteDecision(decisions[i]))
        {
            successCount++;
        }
    }
    
    return successCount;
}

bool
PythonAlgorithmBridge::TriggerShuffle(uint32_t domainId, ShuffleMode mode)
{
    NS_LOG_FUNCTION(this << domainId);
    
    if (!m_shuffleController)
    {
        NS_LOG_WARN("ShuffleController not set");
        return false;
    }
    
    try
    {
        m_shuffleController->TriggerShuffle(domainId, mode);
        return true;
    }
    catch (const std::exception& e)
    {
        NS_LOG_ERROR("TriggerShuffle failed: " << e.what());
        return false;
    }
}

bool
PythonAlgorithmBridge::MigrateUser(uint32_t userId, uint32_t newDomainId)
{
    NS_LOG_FUNCTION(this << userId << newDomainId);
    
    if (!m_domainManager)
    {
        NS_LOG_WARN("DomainManager not set");
        return false;
    }
    
    return m_domainManager->MoveUser(userId, newDomainId);
}

bool
PythonAlgorithmBridge::UpdateUserScore(uint32_t userId, double newScore)
{
    NS_LOG_FUNCTION(this << userId << newScore);
    
    if (!m_scoreManager)
    {
        NS_LOG_WARN("ScoreManager not set");
        return false;
    }
    
    // Create a synthetic observation with the target score
    DetectionObservation obs;
    obs.rateAnomaly = newScore;
    obs.patternAnomaly = 0.0;
    obs.persistenceFactor = 0.0;
    obs.timestamp = Simulator::Now().GetNanoSeconds();
    
    m_scoreManager->UpdateScore(userId, obs);
    return true;
}

bool
PythonAlgorithmBridge::ChangeShuffleFrequency(uint32_t domainId, double frequency)
{
    NS_LOG_FUNCTION(this << domainId << frequency);
    
    if (!m_shuffleController)
    {
        NS_LOG_WARN("ShuffleController not set");
        return false;
    }
    
    m_shuffleController->SetFrequency(domainId, frequency);
    return true;
}

bool
PythonAlgorithmBridge::AssignUserToProxy(uint32_t userId, uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << userId << proxyId);
    
    if (!m_shuffleController)
    {
        NS_LOG_WARN("ShuffleController not set");
        return false;
    }
    
    return m_shuffleController->AssignUserToProxy(userId, proxyId);
}

void
PythonAlgorithmBridge::StartPeriodicEvaluation()
{
    NS_LOG_FUNCTION(this);
    
    if (m_evaluationRunning)
    {
        NS_LOG_WARN("Evaluation already running");
        return;
    }
    
    if (!m_hasDefenseEvaluator)
    {
        NS_LOG_WARN("No defense evaluator registered");
        return;
    }
    
    m_evaluationRunning = true;
    PerformEvaluation();
}

void
PythonAlgorithmBridge::StopPeriodicEvaluation()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_evaluationRunning)
    {
        return;
    }
    
    m_evaluationRunning = false;
    Simulator::Cancel(m_evaluationEvent);
}

uint32_t
PythonAlgorithmBridge::TriggerEvaluation()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_hasDefenseEvaluator)
    {
        NS_LOG_WARN("No defense evaluator registered");
        return 0;
    }
    
    auto startTime = std::chrono::high_resolution_clock::now();
    
    SimulationState state = GetSimulationState();
    std::vector<DefenseDecision> decisions = m_defenseEvaluator(state);
    
    auto endTime = std::chrono::high_resolution_clock::now();
    auto duration = std::chrono::duration_cast<std::chrono::microseconds>(endTime - startTime);
    
    // Update average evaluation time
    m_avgEvaluationTime = (m_avgEvaluationTime * m_totalEvaluations + duration.count()) 
                          / (m_totalEvaluations + 1);
    m_totalEvaluations++;
    
    return ExecuteDecisions(decisions);
}

bool
PythonAlgorithmBridge::IsEvaluationRunning() const
{
    return m_evaluationRunning;
}

std::map<std::string, double>
PythonAlgorithmBridge::GetStatistics() const
{
    std::map<std::string, double> stats;
    stats["totalEvaluations"] = static_cast<double>(m_totalEvaluations);
    stats["totalDecisions"] = static_cast<double>(m_totalDecisions);
    stats["successfulDecisions"] = static_cast<double>(m_successfulDecisions);
    stats["failedDecisions"] = static_cast<double>(m_failedDecisions);
    stats["successRate"] = m_totalDecisions > 0 
        ? static_cast<double>(m_successfulDecisions) / m_totalDecisions 
        : 0.0;
    stats["avgEvaluationTimeUs"] = m_avgEvaluationTime;
    return stats;
}

std::vector<std::tuple<uint64_t, DefenseDecision, bool>>
PythonAlgorithmBridge::GetDecisionHistory(uint32_t count) const
{
    if (m_decisionHistory.size() <= count)
    {
        return m_decisionHistory;
    }
    return std::vector<std::tuple<uint64_t, DefenseDecision, bool>>(
        m_decisionHistory.end() - count, m_decisionHistory.end());
}

void
PythonAlgorithmBridge::ResetStatistics()
{
    NS_LOG_FUNCTION(this);
    m_totalEvaluations = 0;
    m_totalDecisions = 0;
    m_successfulDecisions = 0;
    m_failedDecisions = 0;
    m_avgEvaluationTime = 0.0;
    m_decisionHistory.clear();
}

void
PythonAlgorithmBridge::PerformEvaluation()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_evaluationRunning)
    {
        return;
    }
    
    TriggerEvaluation();
    
    // Schedule next evaluation
    m_evaluationEvent = Simulator::Schedule(
        Seconds(m_config.evaluationInterval),
        &PythonAlgorithmBridge::PerformEvaluation, this);
}

void
PythonAlgorithmBridge::ApplyCallbacksToComponents()
{
    NS_LOG_FUNCTION(this);
    
    // Apply score calculator to ScoreManager
    if (m_hasScoreCalculator && m_scoreManager)
    {
        m_scoreManager->SetCustomScoreCallback(
            MakeCallback(&PythonAlgorithmBridge::WrapScoreCalculator, this));
    }
    
    // Apply risk classifier to ScoreManager
    if (m_hasRiskClassifier && m_scoreManager)
    {
        m_scoreManager->SetCustomRiskLevelCallback(
            MakeCallback(&PythonAlgorithmBridge::WrapRiskClassifier, this));
    }
    
    // Apply shuffle strategy to ShuffleController
    if (m_hasShuffleStrategy && m_shuffleController)
    {
        m_shuffleController->SetCustomStrategy(
            MakeCallback(&PythonAlgorithmBridge::WrapShuffleStrategy, this));
    }
    
    // Apply domain assigner to DomainManager
    if (m_hasDomainAssigner && m_domainManager)
    {
        m_domainManager->SetCustomStrategy(
            MakeCallback(&PythonAlgorithmBridge::WrapDomainAssigner, this));
    }
}

void
PythonAlgorithmBridge::RecordDecision(const DefenseDecision& decision, bool success)
{
    uint64_t timestamp = Simulator::Now().GetNanoSeconds();
    m_decisionHistory.emplace_back(timestamp, decision, success);
    
    // Keep history bounded
    const size_t maxHistory = 10000;
    if (m_decisionHistory.size() > maxHistory)
    {
        m_decisionHistory.erase(m_decisionHistory.begin(), 
                                 m_decisionHistory.begin() + maxHistory / 2);
    }
}

// Callback wrapper implementations for NS-3 MakeCallback compatibility
double
PythonAlgorithmBridge::WrapScoreCalculator(uint32_t userId, const DetectionObservation& obs, double currentScore)
{
    if (m_hasScoreCalculator && m_scoreCalculator)
    {
        return m_scoreCalculator(userId, obs, currentScore);
    }
    return currentScore;
}

RiskLevel
PythonAlgorithmBridge::WrapRiskClassifier(uint32_t userId, double score)
{
    if (m_hasRiskClassifier && m_riskClassifier)
    {
        return m_riskClassifier(userId, score);
    }
    // Default classification
    if (score > 0.85) return RiskLevel::CRITICAL;
    if (score > 0.60) return RiskLevel::HIGH;
    if (score > 0.30) return RiskLevel::MEDIUM;
    return RiskLevel::LOW;
}

uint32_t
PythonAlgorithmBridge::WrapShuffleStrategy(uint32_t userId, const std::vector<uint32_t>& proxies, const UserScore& score)
{
    if (m_hasShuffleStrategy && m_shuffleStrategy)
    {
        return m_shuffleStrategy(userId, proxies, score);
    }
    // Default: random selection
    if (proxies.empty()) return 0;
    return proxies[userId % proxies.size()];
}

uint32_t
PythonAlgorithmBridge::WrapDomainAssigner(uint32_t userId, const std::map<uint32_t, Domain>& domains)
{
    if (m_hasDomainAssigner && m_domainAssigner)
    {
        return m_domainAssigner(userId, domains);
    }
    // Default: first domain
    if (domains.empty()) return 0;
    return domains.begin()->first;
}

// ==================== SimulationContext ====================

NS_OBJECT_ENSURE_REGISTERED(SimulationContext);

TypeId
SimulationContext::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::SimulationContext")
        .SetParent<Object>()
        .SetGroupName("Mtd")
        .AddConstructor<SimulationContext>();
    return tid;
}

SimulationContext::SimulationContext()
{
    NS_LOG_FUNCTION(this);
}

SimulationContext::~SimulationContext()
{
    NS_LOG_FUNCTION(this);
}

void
SimulationContext::Initialize(Ptr<PythonAlgorithmBridge> bridge,
                               Ptr<DomainManager> domainManager,
                               Ptr<ScoreManager> scoreManager,
                               Ptr<ShuffleController> shuffleController,
                               Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    m_bridge = bridge;
    m_domainManager = domainManager;
    m_scoreManager = scoreManager;
    m_shuffleController = shuffleController;
    m_eventBus = eventBus;
}

Ptr<PythonAlgorithmBridge>
SimulationContext::GetBridge() const
{
    return m_bridge;
}

double
SimulationContext::GetCurrentTime() const
{
    return Simulator::Now().GetSeconds();
}

std::vector<uint32_t>
SimulationContext::GetDomainIds() const
{
    if (m_domainManager)
    {
        return m_domainManager->GetAllDomainIds();
    }
    return std::vector<uint32_t>();
}

std::vector<uint32_t>
SimulationContext::GetUserIds() const
{
    if (m_scoreManager)
    {
        return m_scoreManager->GetTrackedUsers();
    }
    return std::vector<uint32_t>();
}

std::vector<uint32_t>
SimulationContext::GetProxyIds() const
{
    std::vector<uint32_t> proxies;
    if (m_domainManager)
    {
        auto domainIds = m_domainManager->GetAllDomainIds();
        for (uint32_t domainId : domainIds)
        {
            auto domainProxies = m_domainManager->GetDomainProxies(domainId);
            proxies.insert(proxies.end(), domainProxies.begin(), domainProxies.end());
        }
    }
    return proxies;
}

void
SimulationContext::Log(int level, const std::string& message)
{
    switch (level)
    {
    case 0:
        NS_LOG_DEBUG("[Python] " << message);
        break;
    case 1:
        NS_LOG_INFO("[Python] " << message);
        break;
    case 2:
        NS_LOG_WARN("[Python] " << message);
        break;
    case 3:
        NS_LOG_ERROR("[Python] " << message);
        break;
    default:
        NS_LOG_INFO("[Python] " << message);
    }
}

void
SimulationContext::ScheduleEvent(double delaySeconds, std::function<void()> callback)
{
    Simulator::Schedule(Seconds(delaySeconds), callback);
}

// ==================== Helper Functions ====================

std::string
ShuffleModeToString(ShuffleMode mode)
{
    switch (mode)
    {
    case ShuffleMode::RANDOM:
        return "RANDOM";
    case ShuffleMode::SCORE_DRIVEN:
        return "SCORE_DRIVEN";
    case ShuffleMode::ROUND_ROBIN:
        return "ROUND_ROBIN";
    case ShuffleMode::ATTACKER_AVOID:
        return "ATTACKER_AVOID";
    case ShuffleMode::LOAD_BALANCED:
        return "LOAD_BALANCED";
    case ShuffleMode::CUSTOM:
        return "CUSTOM";
    default:
        return "UNKNOWN";
    }
}

ShuffleMode
StringToShuffleMode(const std::string& str)
{
    if (str == "RANDOM") return ShuffleMode::RANDOM;
    if (str == "SCORE_DRIVEN") return ShuffleMode::SCORE_DRIVEN;
    if (str == "ROUND_ROBIN") return ShuffleMode::ROUND_ROBIN;
    if (str == "ATTACKER_AVOID") return ShuffleMode::ATTACKER_AVOID;
    if (str == "LOAD_BALANCED") return ShuffleMode::LOAD_BALANCED;
    if (str == "CUSTOM") return ShuffleMode::CUSTOM;
    return ShuffleMode::RANDOM;
}

std::string
RiskLevelToString(RiskLevel level)
{
    switch (level)
    {
    case RiskLevel::LOW:
        return "LOW";
    case RiskLevel::MEDIUM:
        return "MEDIUM";
    case RiskLevel::HIGH:
        return "HIGH";
    case RiskLevel::CRITICAL:
        return "CRITICAL";
    default:
        return "UNKNOWN";
    }
}

RiskLevel
StringToRiskLevel(const std::string& str)
{
    if (str == "LOW") return RiskLevel::LOW;
    if (str == "MEDIUM") return RiskLevel::MEDIUM;
    if (str == "HIGH") return RiskLevel::HIGH;
    if (str == "CRITICAL") return RiskLevel::CRITICAL;
    return RiskLevel::LOW;
}

std::string
AttackTypeToString(AttackType type)
{
    switch (type)
    {
    case AttackType::NONE:
        return "NONE";
    case AttackType::DOS:
        return "DOS";
    case AttackType::PROBE:
        return "PROBE";
    case AttackType::PORT_SCAN:
        return "PORT_SCAN";
    case AttackType::ROUTE_MONITOR:
        return "ROUTE_MONITOR";
    case AttackType::SYN_FLOOD:
        return "SYN_FLOOD";
    case AttackType::UDP_FLOOD:
        return "UDP_FLOOD";
    case AttackType::HTTP_FLOOD:
        return "HTTP_FLOOD";
    default:
        return "UNKNOWN";
    }
}

AttackType
StringToAttackType(const std::string& str)
{
    if (str == "NONE") return AttackType::NONE;
    if (str == "DOS") return AttackType::DOS;
    if (str == "PROBE") return AttackType::PROBE;
    if (str == "PORT_SCAN") return AttackType::PORT_SCAN;
    if (str == "ROUTE_MONITOR") return AttackType::ROUTE_MONITOR;
    if (str == "SYN_FLOOD") return AttackType::SYN_FLOOD;
    if (str == "UDP_FLOOD") return AttackType::UDP_FLOOD;
    if (str == "HTTP_FLOOD") return AttackType::HTTP_FLOOD;
    return AttackType::NONE;
}

} // namespace mtd
} // namespace ns3
