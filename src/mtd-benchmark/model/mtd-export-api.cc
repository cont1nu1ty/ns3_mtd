/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Export API implementation
 */

#include "mtd-export-api.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <sstream>
#include <iomanip>
#include <filesystem>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdExportApi");
NS_OBJECT_ENSURE_REGISTERED(ExportApi);

TypeId
ExportApi::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::ExportApi")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<ExportApi>();
    return tid;
}

ExportApi::ExportApi()
    : m_outputDirectory("."),
      m_recordingInterval(1.0),
      m_autoRecording(false)
{
    NS_LOG_FUNCTION(this);
}

ExportApi::~ExportApi()
{
    NS_LOG_FUNCTION(this);
    StopAutoRecording();
}

void
ExportApi::SetExperimentConfig(const ExperimentConfig& config)
{
    NS_LOG_FUNCTION(this);
    m_config = config;
}

ExperimentConfig
ExportApi::GetExperimentConfig() const
{
    return m_config;
}

void
ExportApi::SetDomainManager(Ptr<DomainManager> domainManager)
{
    NS_LOG_FUNCTION(this);
    m_domainManager = domainManager;
}

void
ExportApi::SetShuffleController(Ptr<ShuffleController> shuffleController)
{
    NS_LOG_FUNCTION(this);
    m_shuffleController = shuffleController;
}

void
ExportApi::SetAttackGenerator(Ptr<AttackGenerator> attackGenerator)
{
    NS_LOG_FUNCTION(this);
    m_attackGenerator = attackGenerator;
}

void
ExportApi::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    m_eventBus = eventBus;
}

bool
ExportApi::ExportExperimentSnapshot(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);
    
    std::string content;
    
    switch (format)
    {
        case ExportFormat::JSON:
            content = GenerateJsonSnapshot();
            break;
        default:
            content = GenerateJsonSnapshot();
            break;
    }
    
    return WriteToFile(path, content);
}

bool
ExportApi::ExportTrafficTrace(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);
    
    std::string content;
    
    switch (format)
    {
        case ExportFormat::CSV:
            content = GenerateTrafficCsv();
            break;
        default:
            content = GenerateTrafficCsv();
            break;
    }
    
    return WriteToFile(path, content);
}

bool
ExportApi::ExportDomainState(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);
    
    std::string content;
    
    switch (format)
    {
        case ExportFormat::JSON:
            content = GenerateDomainJson();
            break;
        default:
            content = GenerateDomainJson();
            break;
    }
    
    return WriteToFile(path, content);
}

bool
ExportApi::ExportShuffleEvents(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);
    
    std::string content;
    
    switch (format)
    {
        case ExportFormat::CSV:
            content = GenerateShuffleCsv();
            break;
        default:
            content = GenerateShuffleCsv();
            break;
    }
    
    return WriteToFile(path, content);
}

bool
ExportApi::ExportAttackEvents(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);
    
    std::string content;
    
    switch (format)
    {
        case ExportFormat::CSV:
            content = GenerateAttackCsv();
            break;
        default:
            content = GenerateAttackCsv();
            break;
    }
    
    return WriteToFile(path, content);
}

bool
ExportApi::ExportEventHistory(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);
    
    std::string content;
    
    switch (format)
    {
        case ExportFormat::JSON:
            content = GenerateEventJson();
            break;
        default:
            content = GenerateEventJson();
            break;
    }
    
    return WriteToFile(path, content);
}

void
ExportApi::RecordTrafficSample(const TrafficStats& stats, 
                               uint32_t domainId, uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << domainId << proxyId);
    
    TrafficRecord record;
    record.timestamp = Simulator::Now().GetMilliSeconds();
    record.domainId = domainId;
    record.proxyId = proxyId;
    record.stats = stats;
    
    m_trafficRecords.push_back(record);
}

void
ExportApi::StartAutoRecording(double intervalSeconds)
{
    NS_LOG_FUNCTION(this << intervalSeconds);
    
    StopAutoRecording();
    
    m_autoRecording = true;
    m_recordingInterval = intervalSeconds;
    
    m_recordingEvent = Simulator::Schedule(Seconds(intervalSeconds),
        &ExportApi::PerformAutoRecord, this);
}

void
ExportApi::StopAutoRecording()
{
    NS_LOG_FUNCTION(this);
    
    m_autoRecording = false;
    Simulator::Cancel(m_recordingEvent);
}

std::map<std::string, double>
ExportApi::GetPerformanceSummary() const
{
    NS_LOG_FUNCTION(this);
    
    std::map<std::string, double> summary;
    
    // Traffic summary
    if (!m_trafficRecords.empty())
    {
        double totalPackets = 0.0;
        double totalBytes = 0.0;
        double avgLatency = 0.0;
        
        for (const auto& record : m_trafficRecords)
        {
            totalPackets += record.stats.packetsIn + record.stats.packetsOut;
            totalBytes += record.stats.bytesIn + record.stats.bytesOut;
            avgLatency += record.stats.avgLatency;
        }
        
        summary["totalPackets"] = totalPackets;
        summary["totalBytes"] = totalBytes;
        summary["avgLatency"] = avgLatency / m_trafficRecords.size();
        summary["recordCount"] = static_cast<double>(m_trafficRecords.size());
    }
    
    // Shuffle summary
    if (m_shuffleController != nullptr)
    {
        auto shuffleStats = m_shuffleController->GetShuffleStats();
        summary["totalShuffles"] = shuffleStats["totalShuffles"];
        summary["shuffleSuccessRate"] = shuffleStats["successRate"];
    }
    
    // Attack summary
    if (m_attackGenerator != nullptr)
    {
        auto attackStats = m_attackGenerator->GetStatistics();
        summary["attackPackets"] = attackStats["packetCount"];
        summary["attackBytes"] = attackStats["byteCount"];
    }
    
    return summary;
}

void
ExportApi::ClearRecords()
{
    NS_LOG_FUNCTION(this);
    m_trafficRecords.clear();
}

void
ExportApi::SetOutputDirectory(const std::string& directory)
{
    NS_LOG_FUNCTION(this << directory);
    m_outputDirectory = directory;
}

std::string
ExportApi::GetOutputDirectory() const
{
    return m_outputDirectory;
}

void
ExportApi::PerformAutoRecord()
{
    if (!m_autoRecording)
    {
        return;
    }
    
    // Record domain metrics
    if (m_domainManager != nullptr)
    {
        for (uint32_t domainId : m_domainManager->GetAllDomainIds())
        {
            DomainMetrics metrics = m_domainManager->GetDomainMetrics(domainId);
            
            TrafficStats stats;
            stats.packetRate = metrics.throughput;
            stats.avgLatency = metrics.avgLatency;
            stats.activeConnections = metrics.activeConnections;
            
            RecordTrafficSample(stats, domainId, 0);
        }
    }
    
    // Schedule next recording
    m_recordingEvent = Simulator::Schedule(Seconds(m_recordingInterval),
        &ExportApi::PerformAutoRecord, this);
}

std::string
ExportApi::GenerateJsonSnapshot() const
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    ss << "{\n";
    ss << "  \"experimentId\": \"" << EscapeJson(m_config.experimentId) << "\",\n";
    ss << "  \"timestamp\": " << Simulator::Now().GetMilliSeconds() << ",\n";
    ss << "  \"randomSeed\": " << m_config.randomSeed << ",\n";
    ss << "  \"simulationDuration\": " << m_config.simulationDuration << ",\n";
    ss << "  \"configuration\": {\n";
    ss << "    \"numClients\": " << m_config.numClients << ",\n";
    ss << "    \"numProxies\": " << m_config.numProxies << ",\n";
    ss << "    \"numDomains\": " << m_config.numDomains << ",\n";
    ss << "    \"numAttackers\": " << m_config.numAttackers << ",\n";
    ss << "    \"defaultStrategy\": " << static_cast<int>(m_config.defaultStrategy) << ",\n";
    ss << "    \"defaultShuffleFrequency\": " << m_config.defaultShuffleFrequency << "\n";
    ss << "  },\n";
    
    // Domain state
    ss << "  \"domains\": [\n";
    if (m_domainManager != nullptr)
    {
        auto domainIds = m_domainManager->GetAllDomainIds();
        for (size_t i = 0; i < domainIds.size(); ++i)
        {
            Domain domain = m_domainManager->GetDomainInfo(domainIds[i]);
            ss << "    {\n";
            ss << "      \"domainId\": " << domain.domainId << ",\n";
            ss << "      \"name\": \"" << EscapeJson(domain.name) << "\",\n";
            ss << "      \"userCount\": " << domain.userIds.size() << ",\n";
            ss << "      \"proxyCount\": " << domain.proxyIds.size() << ",\n";
            ss << "      \"loadFactor\": " << domain.loadFactor << ",\n";
            ss << "      \"shuffleFrequency\": " << domain.shuffleFrequency << "\n";
            ss << "    }";
            if (i < domainIds.size() - 1) ss << ",";
            ss << "\n";
        }
    }
    ss << "  ],\n";
    
    // Performance summary
    auto summary = GetPerformanceSummary();
    ss << "  \"performance\": {\n";
    bool first = true;
    for (const auto& pair : summary)
    {
        if (!first) ss << ",\n";
        ss << "    \"" << pair.first << "\": " << pair.second;
        first = false;
    }
    ss << "\n  }\n";
    
    ss << "}\n";
    
    return ss.str();
}

std::string
ExportApi::GenerateTrafficCsv() const
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    // Header
    ss << "timestamp,domainId,proxyId,packetsIn,packetsOut,bytesIn,bytesOut,";
    ss << "packetRate,byteRate,activeConnections,avgLatency\n";
    
    for (const auto& record : m_trafficRecords)
    {
        ss << record.timestamp << ",";
        ss << record.domainId << ",";
        ss << record.proxyId << ",";
        ss << record.stats.packetsIn << ",";
        ss << record.stats.packetsOut << ",";
        ss << record.stats.bytesIn << ",";
        ss << record.stats.bytesOut << ",";
        ss << record.stats.packetRate << ",";
        ss << record.stats.byteRate << ",";
        ss << record.stats.activeConnections << ",";
        ss << record.stats.avgLatency << "\n";
    }
    
    return ss.str();
}

std::string
ExportApi::GenerateDomainJson() const
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    ss << "{\n";
    ss << "  \"timestamp\": " << Simulator::Now().GetMilliSeconds() << ",\n";
    ss << "  \"domains\": [\n";
    
    if (m_domainManager != nullptr)
    {
        auto domainIds = m_domainManager->GetAllDomainIds();
        for (size_t i = 0; i < domainIds.size(); ++i)
        {
            Domain domain = m_domainManager->GetDomainInfo(domainIds[i]);
            DomainMetrics metrics = m_domainManager->GetDomainMetrics(domainIds[i]);
            
            ss << "    {\n";
            ss << "      \"domainId\": " << domain.domainId << ",\n";
            ss << "      \"name\": \"" << EscapeJson(domain.name) << "\",\n";
            ss << "      \"users\": [";
            for (size_t j = 0; j < domain.userIds.size(); ++j)
            {
                ss << domain.userIds[j];
                if (j < domain.userIds.size() - 1) ss << ", ";
            }
            ss << "],\n";
            ss << "      \"proxies\": [";
            for (size_t j = 0; j < domain.proxyIds.size(); ++j)
            {
                ss << domain.proxyIds[j];
                if (j < domain.proxyIds.size() - 1) ss << ", ";
            }
            ss << "],\n";
            ss << "      \"metrics\": {\n";
            ss << "        \"throughput\": " << metrics.throughput << ",\n";
            ss << "        \"avgLatency\": " << metrics.avgLatency << ",\n";
            ss << "        \"activeConnections\": " << metrics.activeConnections << ",\n";
            ss << "        \"loadFactor\": " << metrics.loadFactor << "\n";
            ss << "      }\n";
            ss << "    }";
            if (i < domainIds.size() - 1) ss << ",";
            ss << "\n";
        }
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

std::string
ExportApi::GenerateShuffleCsv() const
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    // Header
    ss << "timestamp,domainId,strategy,usersAffected,executionTime,success,reason\n";
    
    if (m_shuffleController != nullptr && m_domainManager != nullptr)
    {
        for (uint32_t domainId : m_domainManager->GetAllDomainIds())
        {
            auto history = m_shuffleController->GetShuffleHistory(domainId);
            for (const auto& event : history)
            {
                ss << event.timestamp << ",";
                ss << event.domainId << ",";
                ss << static_cast<int>(event.strategy) << ",";
                ss << event.usersAffected << ",";
                ss << event.executionTime << ",";
                ss << (event.success ? "true" : "false") << ",";
                ss << "\"" << EscapeCsv(event.reason) << "\"\n";
            }
        }
    }
    
    return ss.str();
}

std::string
ExportApi::GenerateAttackCsv() const
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    // Header
    ss << "timestamp,type,targetProxyId,rate,duration,defenseTriggered\n";
    
    if (m_attackGenerator != nullptr)
    {
        auto history = m_attackGenerator->GetAttackHistory();
        for (const auto& event : history)
        {
            ss << event.timestamp << ",";
            ss << static_cast<int>(event.type) << ",";
            ss << event.targetProxyId << ",";
            ss << event.rate << ",";
            ss << event.duration << ",";
            ss << (event.defenseTriggered ? "true" : "false") << "\n";
        }
    }
    
    return ss.str();
}

std::string
ExportApi::GenerateEventJson() const
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    ss << "{\n";
    ss << "  \"events\": [\n";
    
    if (m_eventBus != nullptr)
    {
        auto events = m_eventBus->GetEventHistory();
        for (size_t i = 0; i < events.size(); ++i)
        {
            const auto& event = events[i];
            ss << "    {\n";
            ss << "      \"timestamp\": " << event.timestamp << ",\n";
            ss << "      \"type\": " << static_cast<int>(event.type) << ",\n";
            ss << "      \"sourceNodeId\": " << event.sourceNodeId << ",\n";
            ss << "      \"metadata\": {\n";
            
            bool first = true;
            for (const auto& pair : event.metadata)
            {
                if (!first) ss << ",\n";
                ss << "        \"" << EscapeJson(pair.first) << "\": \"" 
                   << EscapeJson(pair.second) << "\"";
                first = false;
            }
            ss << "\n      }\n";
            ss << "    }";
            if (i < events.size() - 1) ss << ",";
            ss << "\n";
        }
    }
    
    ss << "  ]\n";
    ss << "}\n";
    
    return ss.str();
}

bool
ExportApi::WriteToFile(const std::string& path, const std::string& content)
{
    if (path.empty())
    {
        NS_LOG_ERROR("Empty path provided");
        return false;
    }
    
    std::filesystem::path fsPath(path);
    std::string fullPath;
    
    // Use std::filesystem for robust path handling
    if (fsPath.is_absolute() || path[0] == '.')
    {
        fullPath = path;
    }
    else
    {
        std::filesystem::path outputDir(m_outputDirectory);
        fullPath = (outputDir / fsPath).string();
    }
    
    std::ofstream file(fullPath);
    if (!file.is_open())
    {
        NS_LOG_ERROR("Failed to open file for writing: " << fullPath);
        return false;
    }
    
    file << content;
    file.close();
    
    NS_LOG_INFO("Exported to " << fullPath);
    return true;
}

std::string
ExportApi::EscapeJson(const std::string& str) const
{
    std::string result;
    for (char c : str)
    {
        switch (c)
        {
            case '"': result += "\\\""; break;
            case '\\': result += "\\\\"; break;
            case '\n': result += "\\n"; break;
            case '\r': result += "\\r"; break;
            case '\t': result += "\\t"; break;
            default: result += c; break;
        }
    }
    return result;
}

std::string
ExportApi::EscapeCsv(const std::string& str) const
{
    std::string result;
    for (char c : str)
    {
        if (c == '"')
        {
            result += "\"\"";
        }
        else
        {
            result += c;
        }
    }
    return result;
}

} // namespace mtd
} // namespace ns3
