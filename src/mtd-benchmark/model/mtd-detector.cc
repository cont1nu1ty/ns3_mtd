/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Attack Detection Layer implementation
 */

#include "mtd-detector.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>
#include <cmath>
#include <fstream>
#include <sstream>
#include <numeric>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdDetector");

// ==================== LocalDetector ====================

NS_OBJECT_ENSURE_REGISTERED(LocalDetector);

TypeId
LocalDetector::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::LocalDetector")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<LocalDetector>();
    return tid;
}

LocalDetector::LocalDetector()
    : m_historySize(60) // Keep 60 samples for trend analysis
{
    NS_LOG_FUNCTION(this);
}

LocalDetector::~LocalDetector()
{
    NS_LOG_FUNCTION(this);
}

void
LocalDetector::SetThresholds(const DetectionThresholds& thresholds)
{
    NS_LOG_FUNCTION(this);
    m_thresholds = thresholds;
}

DetectionThresholds
LocalDetector::GetThresholds() const
{
    return m_thresholds;
}

void
LocalDetector::UpdateThreshold(const std::string& key, double value)
{
    NS_LOG_FUNCTION(this << key << value);
    
    if (key == "packetRate")
    {
        m_thresholds.packetRateThreshold = value;
    }
    else if (key == "byteRate")
    {
        m_thresholds.byteRateThreshold = value;
    }
    else if (key == "connections")
    {
        m_thresholds.connectionThreshold = value;
    }
    else if (key == "anomalyScore")
    {
        m_thresholds.anomalyScoreThreshold = value;
    }
}

TrafficStats
LocalDetector::GetStats(uint32_t agentId) const
{
    NS_LOG_FUNCTION(this << agentId);
    
    auto it = m_agentStats.find(agentId);
    if (it != m_agentStats.end())
    {
        return it->second;
    }
    return TrafficStats();
}

void
LocalDetector::UpdateStats(uint32_t agentId, const TrafficStats& stats)
{
    NS_LOG_FUNCTION(this << agentId);
    
    m_agentStats[agentId] = stats;
    
    // Maintain history
    auto& history = m_statsHistory[agentId];
    history.push_back(stats);
    if (history.size() > m_historySize)
    {
        history.pop_front();
    }
}

DetectionObservation
LocalDetector::Analyze(uint32_t agentId)
{
    NS_LOG_FUNCTION(this << agentId);
    
    DetectionObservation obs;
    obs.timestamp = Simulator::Now().GetMilliSeconds();
    
    auto it = m_agentStats.find(agentId);
    if (it == m_agentStats.end())
    {
        return obs;
    }
    
    const TrafficStats& stats = it->second;
    
    // Calculate rate anomaly
    obs.rateAnomaly = CalculateRateAnomaly(agentId);
    
    // Calculate connection anomaly
    obs.connectionAnomaly = CalculateConnectionAnomaly(agentId);
    
    // Calculate overall anomaly score
    double score = 0.0;
    if (stats.packetRate > m_thresholds.packetRateThreshold)
    {
        score += 0.4 * (stats.packetRate / m_thresholds.packetRateThreshold);
    }
    if (stats.byteRate > m_thresholds.byteRateThreshold)
    {
        score += 0.3 * (stats.byteRate / m_thresholds.byteRateThreshold);
    }
    if (stats.activeConnections > m_thresholds.connectionThreshold)
    {
        score += 0.3 * (stats.activeConnections / m_thresholds.connectionThreshold);
    }
    
    obs.patternAnomaly = std::min(1.0, score);
    obs.confidence = obs.patternAnomaly;
    
    // Determine suspected attack type based on pattern
    if (obs.patternAnomaly > 0.8)
    {
        if (obs.connectionAnomaly > obs.rateAnomaly)
        {
            obs.suspectedType = AttackType::SYN_FLOOD;
        }
        else
        {
            obs.suspectedType = AttackType::UDP_FLOOD;
        }
    }
    else if (obs.patternAnomaly > 0.5)
    {
        obs.suspectedType = AttackType::DOS;
    }
    else
    {
        obs.suspectedType = AttackType::NONE;
    }
    
    // Update attack status
    m_attackStatus[agentId] = (obs.patternAnomaly > m_thresholds.anomalyScoreThreshold);
    
    return obs;
}

bool
LocalDetector::IsUnderAttack(uint32_t agentId) const
{
    NS_LOG_FUNCTION(this << agentId);
    
    auto it = m_attackStatus.find(agentId);
    if (it != m_attackStatus.end())
    {
        return it->second;
    }
    return false;
}

void
LocalDetector::ResetStats(uint32_t agentId)
{
    NS_LOG_FUNCTION(this << agentId);
    
    m_agentStats.erase(agentId);
    m_statsHistory.erase(agentId);
    m_attackStatus.erase(agentId);
}

std::vector<uint32_t>
LocalDetector::GetMonitoredAgents() const
{
    std::vector<uint32_t> agents;
    for (const auto& pair : m_agentStats)
    {
        agents.push_back(pair.first);
    }
    return agents;
}

double
LocalDetector::CalculateRateAnomaly(uint32_t agentId) const
{
    auto it = m_statsHistory.find(agentId);
    if (it == m_statsHistory.end() || it->second.size() < 2)
    {
        return 0.0;
    }
    
    const auto& history = it->second;
    
    // Calculate mean and std dev of packet rates
    double sum = 0.0;
    for (const auto& stats : history)
    {
        sum += stats.packetRate;
    }
    double mean = sum / history.size();
    
    double sqSum = 0.0;
    for (const auto& stats : history)
    {
        sqSum += (stats.packetRate - mean) * (stats.packetRate - mean);
    }
    double stddev = std::sqrt(sqSum / history.size());
    
    // Current rate z-score
    double current = m_agentStats.at(agentId).packetRate;
    if (stddev > 0)
    {
        double zscore = std::abs(current - mean) / stddev;
        return std::min(1.0, zscore / 3.0); // Normalize to 0-1
    }
    
    return 0.0;
}

double
LocalDetector::CalculateConnectionAnomaly(uint32_t agentId) const
{
    auto it = m_statsHistory.find(agentId);
    if (it == m_statsHistory.end() || it->second.size() < 2)
    {
        return 0.0;
    }
    
    const auto& history = it->second;
    
    // Calculate mean connection count
    double sum = 0.0;
    for (const auto& stats : history)
    {
        sum += stats.activeConnections;
    }
    double mean = sum / history.size();
    
    double sqSum = 0.0;
    for (const auto& stats : history)
    {
        sqSum += (stats.activeConnections - mean) * (stats.activeConnections - mean);
    }
    double stddev = std::sqrt(sqSum / history.size());
    
    double current = m_agentStats.at(agentId).activeConnections;
    if (stddev > 0)
    {
        double zscore = std::abs(current - mean) / stddev;
        return std::min(1.0, zscore / 3.0);
    }
    
    return 0.0;
}

// ==================== CrossAgentDetector ====================

NS_OBJECT_ENSURE_REGISTERED(CrossAgentDetector);

TypeId
CrossAgentDetector::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::CrossAgentDetector")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<CrossAgentDetector>();
    return tid;
}

CrossAgentDetector::CrossAgentDetector()
{
    NS_LOG_FUNCTION(this);
    // Default features for comparison
    m_features = {"packetRate", "byteRate", "connections"};
}

CrossAgentDetector::~CrossAgentDetector()
{
    NS_LOG_FUNCTION(this);
}

void
CrossAgentDetector::SetFeatures(const std::vector<std::string>& features)
{
    NS_LOG_FUNCTION(this);
    m_features = features;
}

std::vector<std::string>
CrossAgentDetector::GetFeatures() const
{
    return m_features;
}

void
CrossAgentDetector::AddAgent(uint32_t agentId, Ptr<LocalDetector> localDetector)
{
    NS_LOG_FUNCTION(this << agentId);
    m_agents[agentId] = localDetector;
}

void
CrossAgentDetector::RemoveAgent(uint32_t agentId)
{
    NS_LOG_FUNCTION(this << agentId);
    m_agents.erase(agentId);
}

std::map<uint32_t, double>
CrossAgentDetector::GetDistribution()
{
    NS_LOG_FUNCTION(this);
    
    std::map<uint32_t, double> distribution;
    double totalRate = 0.0;
    
    // Calculate total traffic
    for (const auto& pair : m_agents)
    {
        TrafficStats stats = pair.second->GetStats(pair.first);
        totalRate += stats.packetRate;
    }
    
    // Calculate normalized distribution
    if (totalRate > 0)
    {
        for (const auto& pair : m_agents)
        {
            TrafficStats stats = pair.second->GetStats(pair.first);
            distribution[pair.first] = stats.packetRate / totalRate;
        }
    }
    
    return distribution;
}

std::map<uint32_t, double>
CrossAgentDetector::AnalyzePatterns()
{
    NS_LOG_FUNCTION(this);
    
    std::map<uint32_t, double> anomalyScores;
    
    if (m_agents.empty())
    {
        return anomalyScores;
    }
    
    // Collect packet rates
    std::vector<double> rates;
    for (const auto& pair : m_agents)
    {
        TrafficStats stats = pair.second->GetStats(pair.first);
        rates.push_back(stats.packetRate);
    }
    
    // Calculate mean and std dev
    auto [mean, stddev] = CalculateMeanStdDev(rates);
    
    // Calculate z-scores
    size_t idx = 0;
    for (const auto& pair : m_agents)
    {
        TrafficStats stats = pair.second->GetStats(pair.first);
        double zscore = CalculateZScore(stats.packetRate, mean, stddev);
        anomalyScores[pair.first] = std::min(1.0, std::abs(zscore) / 3.0);
        idx++;
    }
    
    return anomalyScores;
}

std::vector<DetectionObservation>
CrossAgentDetector::GetAnomalyReport()
{
    NS_LOG_FUNCTION(this);
    
    std::vector<DetectionObservation> report;
    auto anomalyScores = AnalyzePatterns();
    
    for (const auto& pair : anomalyScores)
    {
        if (pair.second > 0.5) // Threshold for including in report
        {
            DetectionObservation obs;
            obs.timestamp = Simulator::Now().GetMilliSeconds();
            obs.patternAnomaly = pair.second;
            obs.confidence = pair.second;
            obs.suspectedType = pair.second > 0.8 ? AttackType::DOS : AttackType::PROBE;
            report.push_back(obs);
        }
    }
    
    return report;
}

std::vector<uint32_t>
CrossAgentDetector::IdentifyOutliers(double threshold)
{
    NS_LOG_FUNCTION(this << threshold);
    
    std::vector<uint32_t> outliers;
    
    if (m_agents.empty())
    {
        return outliers;
    }
    
    // Collect packet rates
    std::vector<double> rates;
    std::vector<uint32_t> agentIds;
    for (const auto& pair : m_agents)
    {
        TrafficStats stats = pair.second->GetStats(pair.first);
        rates.push_back(stats.packetRate);
        agentIds.push_back(pair.first);
    }
    
    auto [mean, stddev] = CalculateMeanStdDev(rates);
    
    for (size_t i = 0; i < rates.size(); ++i)
    {
        double zscore = CalculateZScore(rates[i], mean, stddev);
        if (std::abs(zscore) > threshold)
        {
            outliers.push_back(agentIds[i]);
        }
    }
    
    return outliers;
}

double
CrossAgentDetector::CalculateZScore(double value, double mean, double stddev) const
{
    if (stddev > 0)
    {
        return (value - mean) / stddev;
    }
    return 0.0;
}

std::pair<double, double>
CrossAgentDetector::CalculateMeanStdDev(const std::vector<double>& values) const
{
    if (values.empty())
    {
        return {0.0, 0.0};
    }
    
    double sum = std::accumulate(values.begin(), values.end(), 0.0);
    double mean = sum / values.size();
    
    double sqSum = 0.0;
    for (double v : values)
    {
        sqSum += (v - mean) * (v - mean);
    }
    double stddev = std::sqrt(sqSum / values.size());
    
    return {mean, stddev};
}

// ==================== GlobalDetector ====================

NS_OBJECT_ENSURE_REGISTERED(GlobalDetector);

TypeId
GlobalDetector::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::GlobalDetector")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<GlobalDetector>();
    return tid;
}

GlobalDetector::GlobalDetector()
    : m_trained(false)
{
    NS_LOG_FUNCTION(this);
    InitializeDefaultModel();
}

GlobalDetector::~GlobalDetector()
{
    NS_LOG_FUNCTION(this);
}

bool
GlobalDetector::LoadDataset(const std::string& path)
{
    NS_LOG_FUNCTION(this << path);
    
    m_datasetPath = path;
    
    std::ifstream file(path);
    if (!file.is_open())
    {
        NS_LOG_WARN("Could not open dataset file: " << path);
        return false;
    }
    
    m_trainingData.clear();
    m_trainingLabels.clear();
    
    std::string line;
    // Skip header
    std::getline(file, line);
    
    while (std::getline(file, line))
    {
        std::istringstream iss(line);
        std::vector<double> features;
        std::string token;
        
        // Parse comma-separated values
        while (std::getline(iss, token, ','))
        {
            try
            {
                features.push_back(std::stod(token));
            }
            catch (...)
            {
                continue;
            }
        }
        
        if (features.size() > 1)
        {
            // Last column is label
            int label = static_cast<int>(features.back());
            features.pop_back();
            m_trainingData.push_back(features);
            m_trainingLabels.push_back(static_cast<AttackType>(label));
        }
    }
    
    NS_LOG_INFO("Loaded " << m_trainingData.size() << " samples");
    return !m_trainingData.empty();
}

bool
GlobalDetector::Train()
{
    NS_LOG_FUNCTION(this);
    
    if (m_trainingData.empty())
    {
        NS_LOG_WARN("No training data available");
        return false;
    }
    
    // Simple feature weight calculation based on class means
    // This is a simplified model - real implementation would use proper ML
    m_featureWeights.clear();
    
    // Group samples by label
    std::map<AttackType, std::vector<std::vector<double>>> classData;
    for (size_t i = 0; i < m_trainingData.size(); ++i)
    {
        classData[m_trainingLabels[i]].push_back(m_trainingData[i]);
    }
    
    // Calculate mean features per class
    for (const auto& pair : classData)
    {
        std::vector<double> meanFeatures;
        size_t numFeatures = pair.second[0].size();
        
        for (size_t f = 0; f < numFeatures; ++f)
        {
            double sum = 0.0;
            for (const auto& sample : pair.second)
            {
                sum += sample[f];
            }
            meanFeatures.push_back(sum / pair.second.size());
        }
        
        m_featureWeights[pair.first] = meanFeatures;
    }
    
    m_trained = true;
    NS_LOG_INFO("Model trained with " << m_featureWeights.size() << " classes");
    return true;
}

std::pair<AttackType, double>
GlobalDetector::GetPrediction(const DetectionObservation& observation)
{
    NS_LOG_FUNCTION(this);
    
    std::vector<double> features = {
        observation.rateAnomaly,
        observation.connectionAnomaly,
        observation.patternAnomaly,
        observation.persistenceFactor
    };
    
    AttackType predicted = ClassifyObservation(features);
    double confidence = observation.confidence;
    
    // Log prediction
    m_predictionLog.push_back({
        Simulator::Now().GetMilliSeconds(),
        {predicted, confidence}
    });
    
    return {predicted, confidence};
}

std::map<AttackType, double>
GlobalDetector::GetClassificationReport()
{
    NS_LOG_FUNCTION(this);
    
    std::map<AttackType, double> report;
    
    // Count predictions per class
    std::map<AttackType, int> counts;
    for (const auto& pred : m_predictionLog)
    {
        counts[pred.second.first]++;
    }
    
    // Calculate proportions
    double total = m_predictionLog.size();
    if (total > 0)
    {
        for (const auto& pair : counts)
        {
            report[pair.first] = pair.second / total;
        }
    }
    
    return report;
}

std::vector<std::pair<AttackType, double>>
GlobalDetector::BatchPredict(const std::vector<DetectionObservation>& observations)
{
    NS_LOG_FUNCTION(this);
    
    std::vector<std::pair<AttackType, double>> results;
    for (const auto& obs : observations)
    {
        results.push_back(GetPrediction(obs));
    }
    return results;
}

void
GlobalDetector::SetCrossAgentDetector(Ptr<CrossAgentDetector> detector)
{
    NS_LOG_FUNCTION(this);
    m_crossAgentDetector = detector;
}

std::vector<std::pair<uint64_t, std::pair<AttackType, double>>>
GlobalDetector::GetPredictionLog() const
{
    return m_predictionLog;
}

void
GlobalDetector::SetModelParams(const std::map<std::string, double>& params)
{
    NS_LOG_FUNCTION(this);
    m_modelParams = params;
}

void
GlobalDetector::InitializeDefaultModel()
{
    // Default feature weights for different attack types
    m_featureWeights[AttackType::NONE] = {0.0, 0.0, 0.0, 0.0};
    m_featureWeights[AttackType::DOS] = {0.8, 0.3, 0.7, 0.5};
    m_featureWeights[AttackType::SYN_FLOOD] = {0.6, 0.9, 0.8, 0.6};
    m_featureWeights[AttackType::UDP_FLOOD] = {0.9, 0.2, 0.8, 0.4};
    m_featureWeights[AttackType::HTTP_FLOOD] = {0.7, 0.5, 0.6, 0.7};
    m_featureWeights[AttackType::PROBE] = {0.3, 0.4, 0.5, 0.8};
    m_featureWeights[AttackType::PORT_SCAN] = {0.4, 0.6, 0.4, 0.3};
}

AttackType
GlobalDetector::ClassifyObservation(const std::vector<double>& features) const
{
    if (!m_trained && m_featureWeights.empty())
    {
        return AttackType::NONE;
    }
    
    // Find closest class using Euclidean distance
    AttackType bestClass = AttackType::NONE;
    double bestDistance = std::numeric_limits<double>::max();
    
    for (const auto& pair : m_featureWeights)
    {
        double dist = 0.0;
        size_t numFeatures = std::min(features.size(), pair.second.size());
        
        for (size_t i = 0; i < numFeatures; ++i)
        {
            double diff = features[i] - pair.second[i];
            dist += diff * diff;
        }
        dist = std::sqrt(dist);
        
        if (dist < bestDistance)
        {
            bestDistance = dist;
            bestClass = pair.first;
        }
    }
    
    return bestClass;
}

} // namespace mtd
} // namespace ns3
