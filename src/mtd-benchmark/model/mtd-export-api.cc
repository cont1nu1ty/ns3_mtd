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
    // Also add to the vector for multi-attacker support
    if (attackGenerator != nullptr) {
        m_attackGenerators.push_back(attackGenerator);
    }
}

void
ExportApi::AddAttackGenerator(Ptr<AttackGenerator> attackGenerator)
{
    NS_LOG_FUNCTION(this);
    if (attackGenerator != nullptr) {
        m_attackGenerators.push_back(attackGenerator);
    }
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
ExportApi::ExportBanEvents(const std::string& path, ExportFormat format)
{
    NS_LOG_FUNCTION(this << path);

    std::string content;

    switch (format)
    {
        case ExportFormat::CSV:
            content = GenerateBansCsv();
            break;
        default:
            content = GenerateBansCsv();
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
ExportApi::SetupEventLogging()
{
    NS_LOG_FUNCTION(this);
    SetupEventLogging(FileLogLevel::INFO, 0, false);
}

void
ExportApi::SetupEventLogging(FileLogLevel logLevel, size_t flushEveryN, bool strongConsistency)
{
    NS_LOG_FUNCTION(this << static_cast<int>(logLevel) << flushEveryN << strongConsistency);

    if (m_eventBus == nullptr)
    {
        NS_LOG_ERROR("EventBus not set - cannot setup event logging");
        return;
    }

    m_eventBus->SetFileLogLevel(logLevel);
    m_eventBus->SetFlushPolicy(flushEveryN, strongConsistency);
    m_eventBus->EnableFileLogging(m_outputDirectory);

    NS_LOG_INFO("Event logging configured: dir=" << m_outputDirectory 
                << " level=" << (logLevel == FileLogLevel::DEBUG ? "DEBUG" : "INFO")
                << " flushEveryN=" << flushEveryN 
                << " strongConsistency=" << strongConsistency);
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
    
    // Header with attacker ID
    ss << "timestamp,attackerId,type,targetProxyId,rate,duration,defenseTriggered\n";
    
    // Export from all attack generators
    if (!m_attackGenerators.empty())
    {
        for (size_t attackerId = 0; attackerId < m_attackGenerators.size(); ++attackerId)
        {
            auto history = m_attackGenerators[attackerId]->GetAttackHistory();
            for (const auto& event : history)
            {
                ss << event.timestamp << ",";
                ss << (attackerId + 1) << ",";  // 1-indexed attacker ID
                ss << static_cast<int>(event.type) << ",";
                ss << event.targetProxyId << ",";
                ss << event.rate << ",";
                ss << event.duration << ",";
                ss << (event.defenseTriggered ? "true" : "false") << "\n";
            }
        }
    }
    else if (m_attackGenerator != nullptr)
    {
        // Fallback to single attacker (backward compatibility)
        auto history = m_attackGenerator->GetAttackHistory();
        for (const auto& event : history)
        {
            ss << event.timestamp << ",";
            ss << "1,";  // Default attacker ID
            ss << static_cast<int>(event.type) << ",";
            ss << event.targetProxyId << ",";
            ss << event.rate << ",";
            ss << event.duration << ",";
            ss << (event.defenseTriggered ? "true" : "false") << "\n";
        }
    }

    // If no AttackGenerator is attached, derive a minimal attack CSV from EventBus events.
    // This supports Python-driven scenarios that publish ATTACK_DETECTED/STARTED/STOPPED
    // events without using AttackGenerator traffic generation.
    else if (m_eventBus != nullptr)
    {
        const auto events = m_eventBus->GetEventHistory();
        for (const auto& ev : events)
        {
            if (ev.type != EventType::ATTACK_DETECTED)
            {
                continue;
            }

            uint32_t attackerUserId = 0;
            auto itAttacker = ev.metadata.find("attackerUserId");
            if (itAttacker != ev.metadata.end())
            {
                try
                {
                    attackerUserId = static_cast<uint32_t>(std::stoul(itAttacker->second));
                }
                catch (...)
                {
                }
            }

            uint32_t proxyId = static_cast<uint32_t>(ev.sourceNodeId);
            auto itProxy = ev.metadata.find("proxyId");
            if (itProxy != ev.metadata.end())
            {
                try
                {
                    proxyId = static_cast<uint32_t>(std::stoul(itProxy->second));
                }
                catch (...)
                {
                }
            }

            // Best-effort parse of attack type; default to DOS.
            AttackType attackType = AttackType::DOS;
            auto itType = ev.metadata.find("attackType");
            if (itType != ev.metadata.end())
            {
                try
                {
                    attackType = static_cast<AttackType>(std::stoul(itType->second));
                }
                catch (...)
                {
                    // allow string names like "DOS", "UDP_FLOOD", ...
                    const std::string& s = itType->second;
                    if (s == "NONE") attackType = AttackType::NONE;
                    else if (s == "DOS") attackType = AttackType::DOS;
                    else if (s == "PROBE") attackType = AttackType::PROBE;
                    else if (s == "PORT_SCAN") attackType = AttackType::PORT_SCAN;
                    else if (s == "ROUTE_MONITOR") attackType = AttackType::ROUTE_MONITOR;
                    else if (s == "SYN_FLOOD") attackType = AttackType::SYN_FLOOD;
                    else if (s == "UDP_FLOOD") attackType = AttackType::UDP_FLOOD;
                    else if (s == "HTTP_FLOOD") attackType = AttackType::HTTP_FLOOD;
                }
            }

            // Python-driven scenarios may not have meaningful rate/duration.
            double rate = 0.0;
            double duration = 0.0;
            bool defenseTriggered = false;
            auto itRate = ev.metadata.find("rate");
            if (itRate != ev.metadata.end())
            {
                try
                {
                    rate = std::stod(itRate->second);
                }
                catch (...)
                {
                }
            }
            auto itDuration = ev.metadata.find("duration");
            if (itDuration != ev.metadata.end())
            {
                try
                {
                    duration = std::stod(itDuration->second);
                }
                catch (...)
                {
                }
            }
            auto itDefense = ev.metadata.find("defenseTriggered");
            if (itDefense != ev.metadata.end())
            {
                defenseTriggered = (itDefense->second == "true" || itDefense->second == "1");
            }

            ss << ev.timestamp << ",";
            ss << attackerUserId << ",";
            ss << static_cast<int>(attackType) << ",";
            ss << proxyId << ",";
            ss << rate << ",";
            ss << duration << ",";
            ss << (defenseTriggered ? "true" : "false") << "\n";
        }
    }
    
    return ss.str();
}

std::string
ExportApi::GenerateBansCsv() const
{
    std::ostringstream ss;
    ss << "timestamp,userId,domainId,reason\n";

    if (m_eventBus == nullptr)
    {
        return ss.str();
    }

    const auto events = m_eventBus->GetEventHistory();
    for (const auto& event : events)
    {
        if (event.type != EventType::USER_BANNED)
        {
            continue;
        }

        uint32_t userId = event.sourceNodeId;
        auto itUser = event.metadata.find("userId");
        if (itUser != event.metadata.end())
        {
            try
            {
                userId = static_cast<uint32_t>(std::stoul(itUser->second));
            }
            catch (...)
            {
            }
        }

        std::string domainId;
        auto itDomain = event.metadata.find("domainId");
        if (itDomain != event.metadata.end())
        {
            domainId = itDomain->second;
        }

        std::string reason;
        auto itReason = event.metadata.find("reason");
        if (itReason != event.metadata.end())
        {
            reason = itReason->second;
        }

        ss << event.timestamp << ",";
        ss << userId << ",";
        ss << domainId << ",";
        ss << "\"" << EscapeCsv(reason) << "\"\n";
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
    // Ensure event log files are up-to-date before exporting derived artifacts.
    // This avoids losing the final (partial) batch when using buffered flush policies.
    if (m_eventBus != nullptr && m_eventBus->IsFileLoggingEnabled())
    {
        m_eventBus->FlushLogs();
    }

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

    // Create parent directory if it doesn't exist
    std::filesystem::path parentDir = std::filesystem::path(fullPath).parent_path();
    if (!parentDir.empty() && !std::filesystem::exists(parentDir))
    {
        std::error_code ec;
        if (!std::filesystem::create_directories(parentDir, ec))
        {
            NS_LOG_ERROR("Failed to create directory: " << parentDir << " - " << ec.message());
            return false;
        }
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
