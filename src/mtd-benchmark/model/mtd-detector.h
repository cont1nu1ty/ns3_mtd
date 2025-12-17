/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Attack Detection Layer
 * 
 * This module implements multi-level attack detection:
 * - LocalDetector: Per-proxy threshold-based detection
 * - CrossAgentDetector: Cross-proxy comparative analysis
 * - GlobalDetector: ML-based global pattern detection
 */

#ifndef MTD_DETECTOR_H
#define MTD_DETECTOR_H

#include "mtd-common.h"
#include "mtd-event-bus.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/nstime.h"

#include <map>
#include <vector>
#include <deque>

namespace ns3 {
namespace mtd {

/**
 * \brief Detection thresholds configuration
 */
struct DetectionThresholds {
    double packetRateThreshold;      ///< Packets per second threshold
    double byteRateThreshold;        ///< Bytes per second threshold
    double connectionThreshold;      ///< New connections per second
    double anomalyScoreThreshold;    ///< Anomaly score (0-1) threshold
    
    DetectionThresholds()
        : packetRateThreshold(10000.0),
          byteRateThreshold(10000000.0),
          connectionThreshold(1000.0),
          anomalyScoreThreshold(0.7) {}
};

/**
 * \brief Local (per-proxy) attack detector
 * 
 * Implements threshold-based detection for individual proxy nodes.
 * Fast but prone to false positives - suitable for initial filtering.
 */
class LocalDetector : public Object
{
public:
    static TypeId GetTypeId();
    
    LocalDetector();
    virtual ~LocalDetector();
    
    /**
     * \brief Set detection thresholds
     * \param thresholds The threshold configuration
     */
    void SetThresholds(const DetectionThresholds& thresholds);
    
    /**
     * \brief Get current thresholds
     * \return Current threshold configuration
     */
    DetectionThresholds GetThresholds() const;
    
    /**
     * \brief Update thresholds dynamically
     * \param key Threshold parameter name
     * \param value New value
     */
    void UpdateThreshold(const std::string& key, double value);
    
    /**
     * \brief Get traffic statistics for a specific agent/proxy
     * \param agentId The agent/proxy ID
     * \return Traffic statistics structure
     */
    TrafficStats GetStats(uint32_t agentId) const;
    
    /**
     * \brief Update statistics for an agent
     * \param agentId The agent/proxy ID
     * \param stats Updated traffic statistics
     */
    void UpdateStats(uint32_t agentId, const TrafficStats& stats);
    
    /**
     * \brief Analyze traffic and detect anomalies
     * \param agentId The agent/proxy ID
     * \return Detection observation with anomaly scores
     */
    DetectionObservation Analyze(uint32_t agentId);
    
    /**
     * \brief Check if an agent is under suspected attack
     * \param agentId The agent/proxy ID
     * \return True if attack suspected
     */
    bool IsUnderAttack(uint32_t agentId) const;
    
    /**
     * \brief Reset statistics for an agent
     * \param agentId The agent/proxy ID
     */
    void ResetStats(uint32_t agentId);
    
    /**
     * \brief Get all monitored agent IDs
     * \return Vector of agent IDs
     */
    std::vector<uint32_t> GetMonitoredAgents() const;

private:
    DetectionThresholds m_thresholds;
    std::map<uint32_t, TrafficStats> m_agentStats;
    std::map<uint32_t, std::deque<TrafficStats>> m_statsHistory;
    std::map<uint32_t, bool> m_attackStatus;
    size_t m_historySize;
    
    double CalculateRateAnomaly(uint32_t agentId) const;
    double CalculateConnectionAnomaly(uint32_t agentId) const;
};

/**
 * \brief Cross-agent (inter-proxy) detector
 * 
 * Compares traffic patterns across proxies to detect distributed attacks.
 * Uses statistical methods for anomaly detection.
 */
class CrossAgentDetector : public Object
{
public:
    static TypeId GetTypeId();
    
    CrossAgentDetector();
    virtual ~CrossAgentDetector();
    
    /**
     * \brief Set features to use for comparison
     * \param features Vector of feature names
     */
    void SetFeatures(const std::vector<std::string>& features);
    
    /**
     * \brief Get current feature set
     * \return Vector of feature names
     */
    std::vector<std::string> GetFeatures() const;
    
    /**
     * \brief Add an agent to monitor
     * \param agentId Agent ID
     * \param localDetector Pointer to the agent's local detector
     */
    void AddAgent(uint32_t agentId, Ptr<LocalDetector> localDetector);
    
    /**
     * \brief Remove an agent from monitoring
     * \param agentId Agent ID
     */
    void RemoveAgent(uint32_t agentId);
    
    /**
     * \brief Get traffic distribution across agents
     * \return Map of agent ID to normalized traffic distribution
     */
    std::map<uint32_t, double> GetDistribution();
    
    /**
     * \brief Analyze cross-agent patterns
     * \return Map of agent ID to anomaly score
     */
    std::map<uint32_t, double> AnalyzePatterns();
    
    /**
     * \brief Get anomaly report
     * \return Vector of detection observations for anomalous agents
     */
    std::vector<DetectionObservation> GetAnomalyReport();
    
    /**
     * \brief Identify outlier agents
     * \param threshold Z-score threshold for outlier detection
     * \return Vector of outlier agent IDs
     */
    std::vector<uint32_t> IdentifyOutliers(double threshold = 2.0);

private:
    std::vector<std::string> m_features;
    std::map<uint32_t, Ptr<LocalDetector>> m_agents;
    
    double CalculateZScore(double value, double mean, double stddev) const;
    std::pair<double, double> CalculateMeanStdDev(const std::vector<double>& values) const;
};

/**
 * \brief Global ML-based detector
 * 
 * Uses machine learning models for comprehensive attack detection.
 * Higher latency but better accuracy for complex attack patterns.
 */
class GlobalDetector : public Object
{
public:
    static TypeId GetTypeId();
    
    GlobalDetector();
    virtual ~GlobalDetector();
    
    /**
     * \brief Load attack dataset for training
     * \param path Path to dataset file (CSV format)
     * \return True if loaded successfully
     */
    bool LoadDataset(const std::string& path);
    
    /**
     * \brief Train the detection model
     * \return True if training successful
     */
    bool Train();
    
    /**
     * \brief Get prediction for current traffic patterns
     * \param observation Current observation
     * \return Predicted attack type and confidence
     */
    std::pair<AttackType, double> GetPrediction(const DetectionObservation& observation);
    
    /**
     * \brief Get classification report
     * \return Map of attack type to detection metrics
     */
    std::map<AttackType, double> GetClassificationReport();
    
    /**
     * \brief Batch predict for multiple observations
     * \param observations Vector of observations
     * \return Vector of predictions (attack type, confidence)
     */
    std::vector<std::pair<AttackType, double>> BatchPredict(
        const std::vector<DetectionObservation>& observations);
    
    /**
     * \brief Set local detector reference for data collection
     * \param detector Pointer to cross-agent detector
     */
    void SetCrossAgentDetector(Ptr<CrossAgentDetector> detector);
    
    /**
     * \brief Get attack prediction log
     * \return Vector of prediction history
     */
    std::vector<std::pair<uint64_t, std::pair<AttackType, double>>> GetPredictionLog() const;
    
    /**
     * \brief Set model parameters
     * \param params Map of parameter names to values
     */
    void SetModelParams(const std::map<std::string, double>& params);

private:
    bool m_trained;
    std::string m_datasetPath;
    std::vector<std::vector<double>> m_trainingData;
    std::vector<AttackType> m_trainingLabels;
    std::map<std::string, double> m_modelParams;
    std::vector<std::pair<uint64_t, std::pair<AttackType, double>>> m_predictionLog;
    Ptr<CrossAgentDetector> m_crossAgentDetector;
    
    // Simple decision tree weights (for demonstration)
    std::map<AttackType, std::vector<double>> m_featureWeights;
    
    void InitializeDefaultModel();
    AttackType ClassifyObservation(const std::vector<double>& features) const;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_DETECTOR_H
