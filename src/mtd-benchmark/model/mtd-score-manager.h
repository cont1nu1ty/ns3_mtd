/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Score Manager for risk scoring and classification
 */

#ifndef MTD_SCORE_MANAGER_H
#define MTD_SCORE_MANAGER_H

#include "mtd-common.h"
#include "mtd-event-bus.h"
#include "ns3/object.h"
#include "ns3/ptr.h"

#include <map>
#include <vector>

namespace ns3 {
namespace mtd {

// Custom scoring callback type: (userId, observation, currentScore) -> newScore
typedef Callback<double, uint32_t, const DetectionObservation&, double> CustomScoreCallback;

// Custom risk level callback type: (userId, score) -> RiskLevel
typedef Callback<RiskLevel, uint32_t, double> CustomRiskLevelCallback;

/**
 * \brief Score weights configuration
 * 
 * Formula: score = α·rate + β·anomaly + γ·persistence + δ·feedback
 */
struct ScoreWeights {
    double alpha;    ///< Rate anomaly weight
    double beta;     ///< Pattern anomaly weight
    double gamma;    ///< Persistence factor weight
    double delta;    ///< Feedback weight
    double lambda;   ///< Time decay factor
    
    ScoreWeights()
        : alpha(0.3), beta(0.3), gamma(0.2), delta(0.2), lambda(0.1) {}
};

/**
 * \brief Risk level thresholds
 */
struct RiskThresholds {
    double lowMax;       ///< Max score for LOW level
    double mediumMax;    ///< Max score for MEDIUM level
    double highMax;      ///< Max score for HIGH level
    
    RiskThresholds()
        : lowMax(0.3), mediumMax(0.6), highMax(0.85) {}
};

/**
 * \brief Score Manager for user risk scoring
 * 
 * Calculates and maintains risk scores for users based on
 * detection observations with time decay and feedback integration.
 */
class ScoreManager : public Object
{
public:
    static TypeId GetTypeId();
    
    ScoreManager();
    ~ScoreManager() override;
    
    /**
     * \brief Set score weights
     * \param weights The weight configuration
     */
    void SetWeights(const ScoreWeights& weights);
    
    /**
     * \brief Get current weights
     * \return Weight configuration
     */
    ScoreWeights GetWeights() const;
    
    /**
     * \brief Set risk level thresholds
     * \param thresholds The threshold configuration
     */
    void SetRiskThresholds(const RiskThresholds& thresholds);
    
    /**
     * \brief Get risk thresholds
     * \return Threshold configuration
     */
    RiskThresholds GetRiskThresholds() const;
    
    /**
     * \brief Update score for a user based on new observation
     * \param userId The user ID
     * \param observation The detection observation
     */
    void UpdateScore(uint32_t userId, const DetectionObservation& observation);
    
    /**
     * \brief Get current risk level for a user
     * \param userId The user ID
     * \return Risk level
     */
    RiskLevel GetRiskLevel(uint32_t userId) const;
    
    /**
     * \brief Get current score for a user
     * \param userId The user ID
     * \return Score value (0.0 to 1.0)
     */
    double GetScore(uint32_t userId) const;
    
    /**
     * \brief Get full user score record
     * \param userId The user ID
     * \return UserScore structure
     */
    UserScore GetUserScore(uint32_t userId) const;
    
    /**
     * \brief Apply time decay to all scores
     * \param deltaTime Time elapsed since last decay (milliseconds)
     */
    void ApplyTimeDecay(uint64_t deltaTime);
    
    /**
     * \brief Apply feedback to a user's score
     * \param userId The user ID
     * \param feedbackScore Feedback value (-1.0 to 1.0)
     */
    void ApplyFeedback(uint32_t userId, double feedbackScore);
    
    /**
     * \brief Get all users with a specific risk level
     * \param level The risk level to filter by
     * \return Vector of user IDs
     */
    std::vector<uint32_t> GetUsersByRiskLevel(RiskLevel level) const;
    
    /**
     * \brief Get score distribution across all users
     * \return Map of risk level to user count
     */
    std::map<RiskLevel, uint32_t> GetScoreDistribution() const;
    
    /**
     * \brief Reset score for a user
     * \param userId The user ID
     */
    void ResetScore(uint32_t userId);
    
    /**
     * \brief Clear all scores
     */
    void ClearAllScores();
    
    /**
     * \brief Get all tracked user IDs
     * \return Vector of user IDs
     */
    std::vector<uint32_t> GetTrackedUsers() const;
    
    /**
     * \brief Set event bus for notifications
     * \param eventBus Pointer to event bus
     */
    void SetEventBus(Ptr<EventBus> eventBus);
    
    /**
     * \brief Set custom scoring algorithm callback
     * 
     * When set, this callback replaces the default scoring formula.
     * The callback receives (userId, observation, currentScore) and returns newScore.
     * 
     * \param callback Custom scoring function
     */
    void SetCustomScoreCallback(CustomScoreCallback callback);
    
    /**
     * \brief Clear custom scoring callback (use default formula)
     */
    void ClearCustomScoreCallback();
    
    /**
     * \brief Set custom risk level classification callback
     * 
     * When set, this callback replaces the default threshold-based classification.
     * The callback receives (userId, score) and returns RiskLevel.
     * 
     * \param callback Custom risk level function
     */
    void SetCustomRiskLevelCallback(CustomRiskLevelCallback callback);
    
    /**
     * \brief Clear custom risk level callback (use default thresholds)
     */
    void ClearCustomRiskLevelCallback();
    
    /**
     * \brief Check if custom scoring is enabled
     * \return True if custom scoring callback is set
     */
    bool IsCustomScoringEnabled() const;
    
    /**
     * \brief Check if custom risk level is enabled
     * \return True if custom risk level callback is set
     */
    bool IsCustomRiskLevelEnabled() const;

private:
    ScoreWeights m_weights;
    RiskThresholds m_thresholds;
    std::map<uint32_t, UserScore> m_userScores;
    Ptr<EventBus> m_eventBus;
    CustomScoreCallback m_customScoreCallback;
    CustomRiskLevelCallback m_customRiskLevelCallback;
    
    RiskLevel CalculateRiskLevel(double score) const;
    double CalculateNewScore(const UserScore& current, const DetectionObservation& obs) const;
    void NotifyScoreUpdate(uint32_t userId, const UserScore& score);
};

} // namespace mtd
} // namespace ns3

#endif // MTD_SCORE_MANAGER_H
