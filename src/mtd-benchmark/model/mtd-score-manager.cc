/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Score Manager implementation
 */

#include "mtd-score-manager.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdScoreManager");
NS_OBJECT_ENSURE_REGISTERED(ScoreManager);

TypeId
ScoreManager::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::ScoreManager")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<ScoreManager>();
    return tid;
}

ScoreManager::ScoreManager()
{
    NS_LOG_FUNCTION(this);
}

ScoreManager::~ScoreManager()
{
    NS_LOG_FUNCTION(this);
}

void
ScoreManager::SetWeights(const ScoreWeights& weights)
{
    NS_LOG_FUNCTION(this);
    m_weights = weights;
}

ScoreWeights
ScoreManager::GetWeights() const
{
    return m_weights;
}

void
ScoreManager::SetRiskThresholds(const RiskThresholds& thresholds)
{
    NS_LOG_FUNCTION(this);
    m_thresholds = thresholds;
}

RiskThresholds
ScoreManager::GetRiskThresholds() const
{
    return m_thresholds;
}

void
ScoreManager::UpdateScore(uint32_t userId, const DetectionObservation& observation)
{
    NS_LOG_FUNCTION(this << userId);
    
    // Get or create user score
    if (m_userScores.find(userId) == m_userScores.end())
    {
        m_userScores[userId] = UserScore(userId);
    }
    
    UserScore& userScore = m_userScores[userId];
    
    // Calculate new score - use custom callback if set
    double newScore;
    if (!m_customScoreCallback.IsNull())
    {
        newScore = m_customScoreCallback(userId, observation, userScore.currentScore);
    }
    else
    {
        newScore = CalculateNewScore(userScore, observation);
    }
    
    // Update user record
    userScore.currentScore = std::clamp(newScore, 0.0, 1.0);
    
    // Calculate risk level - use custom callback if set
    if (!m_customRiskLevelCallback.IsNull())
    {
        userScore.riskLevel = m_customRiskLevelCallback(userId, userScore.currentScore);
    }
    else
    {
        userScore.riskLevel = CalculateRiskLevel(userScore.currentScore);
    }
    
    userScore.lastUpdateTime = Simulator::Now().GetMilliSeconds();
    
    // Store observation in history (keep last 10)
    userScore.recentObservations.push_back(observation);
    if (userScore.recentObservations.size() > 10)
    {
        userScore.recentObservations.erase(userScore.recentObservations.begin());
    }
    
    // Notify via event bus
    NotifyScoreUpdate(userId, userScore);
}

RiskLevel
ScoreManager::GetRiskLevel(uint32_t userId) const
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_userScores.find(userId);
    if (it != m_userScores.end())
    {
        return it->second.riskLevel;
    }
    return RiskLevel::LOW;
}

double
ScoreManager::GetScore(uint32_t userId) const
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_userScores.find(userId);
    if (it != m_userScores.end())
    {
        return it->second.currentScore;
    }
    return 0.0;
}

UserScore
ScoreManager::GetUserScore(uint32_t userId) const
{
    NS_LOG_FUNCTION(this << userId);
    
    auto it = m_userScores.find(userId);
    if (it != m_userScores.end())
    {
        return it->second;
    }
    return UserScore(userId);
}

void
ScoreManager::ApplyTimeDecay(uint64_t deltaTime)
{
    NS_LOG_FUNCTION(this << deltaTime);
    
    // Apply exponential decay: score_t+1 = score_t * exp(-λ * Δt)
    double decayFactor = std::exp(-m_weights.lambda * deltaTime / 1000.0);
    
    for (auto& pair : m_userScores)
    {
        pair.second.currentScore *= decayFactor;
        if (!m_customRiskLevelCallback.IsNull())
        {
            pair.second.riskLevel = m_customRiskLevelCallback(pair.first, pair.second.currentScore);
        }
        else
        {
            pair.second.riskLevel = CalculateRiskLevel(pair.second.currentScore);
        }
    }
}

void
ScoreManager::ApplyFeedback(uint32_t userId, double feedbackScore)
{
    NS_LOG_FUNCTION(this << userId << feedbackScore);
    
    auto it = m_userScores.find(userId);
    if (it != m_userScores.end())
    {
        // Apply feedback with delta weight
        double adjustment = m_weights.delta * feedbackScore;
        it->second.currentScore = std::clamp(it->second.currentScore + adjustment, 0.0, 1.0);
        if (!m_customRiskLevelCallback.IsNull())
        {
            it->second.riskLevel = m_customRiskLevelCallback(userId, it->second.currentScore);
        }
        else
        {
            it->second.riskLevel = CalculateRiskLevel(it->second.currentScore);
        }
        
        NotifyScoreUpdate(userId, it->second);
    }
}

std::vector<uint32_t>
ScoreManager::GetUsersByRiskLevel(RiskLevel level) const
{
    NS_LOG_FUNCTION(this << static_cast<int>(level));
    
    std::vector<uint32_t> users;
    for (const auto& pair : m_userScores)
    {
        if (pair.second.riskLevel == level)
        {
            users.push_back(pair.first);
        }
    }
    return users;
}

std::map<RiskLevel, uint32_t>
ScoreManager::GetScoreDistribution() const
{
    NS_LOG_FUNCTION(this);
    
    std::map<RiskLevel, uint32_t> distribution;
    distribution[RiskLevel::LOW] = 0;
    distribution[RiskLevel::MEDIUM] = 0;
    distribution[RiskLevel::HIGH] = 0;
    distribution[RiskLevel::CRITICAL] = 0;
    
    for (const auto& pair : m_userScores)
    {
        distribution[pair.second.riskLevel]++;
    }
    
    return distribution;
}

void
ScoreManager::ResetScore(uint32_t userId)
{
    NS_LOG_FUNCTION(this << userId);
    m_userScores.erase(userId);
}

void
ScoreManager::ClearAllScores()
{
    NS_LOG_FUNCTION(this);
    m_userScores.clear();
}

std::vector<uint32_t>
ScoreManager::GetTrackedUsers() const
{
    std::vector<uint32_t> users;
    for (const auto& pair : m_userScores)
    {
        users.push_back(pair.first);
    }
    return users;
}

void
ScoreManager::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    m_eventBus = eventBus;
}

void
ScoreManager::SetCustomScoreCallback(CustomScoreCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_customScoreCallback = callback;
}

void
ScoreManager::ClearCustomScoreCallback()
{
    NS_LOG_FUNCTION(this);
    m_customScoreCallback.Nullify();
}

void
ScoreManager::SetCustomRiskLevelCallback(CustomRiskLevelCallback callback)
{
    NS_LOG_FUNCTION(this);
    m_customRiskLevelCallback = callback;
}

void
ScoreManager::ClearCustomRiskLevelCallback()
{
    NS_LOG_FUNCTION(this);
    m_customRiskLevelCallback.Nullify();
}

bool
ScoreManager::IsCustomScoringEnabled() const
{
    return !m_customScoreCallback.IsNull();
}

bool
ScoreManager::IsCustomRiskLevelEnabled() const
{
    return !m_customRiskLevelCallback.IsNull();
}

RiskLevel
ScoreManager::CalculateRiskLevel(double score) const
{
    if (score <= m_thresholds.lowMax)
    {
        return RiskLevel::LOW;
    }
    else if (score <= m_thresholds.mediumMax)
    {
        return RiskLevel::MEDIUM;
    }
    else if (score <= m_thresholds.highMax)
    {
        return RiskLevel::HIGH;
    }
    else
    {
        return RiskLevel::CRITICAL;
    }
}

double
ScoreManager::CalculateNewScore(const UserScore& current, const DetectionObservation& obs) const
{
    // Calculate component scores
    double rateComponent = m_weights.alpha * obs.rateAnomaly;
    double anomalyComponent = m_weights.beta * obs.patternAnomaly;
    double persistenceComponent = m_weights.gamma * obs.persistenceFactor;
    
    // Calculate new observation weight
    double newObsWeight = rateComponent + anomalyComponent + persistenceComponent;
    
    // Apply time decay to current score
    uint64_t currentTime = Simulator::Now().GetMilliSeconds();
    double deltaTime = currentTime - current.lastUpdateTime;
    double decayedCurrent = current.currentScore * std::exp(-m_weights.lambda * deltaTime / 1000.0);
    
    // Combine with new observation
    double newScore = decayedCurrent + newObsWeight;
    
    return newScore;
}

void
ScoreManager::NotifyScoreUpdate(uint32_t userId, const UserScore& score)
{
    if (m_eventBus != nullptr)
    {
        MtdEvent event(EventType::SCORE_UPDATED, Simulator::Now().GetMilliSeconds());
        event.metadata["userId"] = std::to_string(userId);
        event.metadata["score"] = std::to_string(score.currentScore);
        event.metadata["riskLevel"] = std::to_string(static_cast<int>(score.riskLevel));
        m_eventBus->Publish(event);
    }
}

} // namespace mtd
} // namespace ns3
