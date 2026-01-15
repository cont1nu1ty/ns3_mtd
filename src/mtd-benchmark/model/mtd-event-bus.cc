/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Event Bus implementation
 * 
 * Refactored with automatic logging, directory management,
 * strategy-based formatting, and buffered I/O.
 */

#include "mtd-event-bus.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <sstream>
#include <iomanip>
#include <filesystem>
#include <ctime>
#include <chrono>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdEventBus");
NS_OBJECT_ENSURE_REGISTERED(EventBus);

TypeId
EventBus::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::EventBus")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<EventBus>();
    return tid;
}

EventBus::EventBus()
    : m_formatter(std::make_unique<TextFormatter>()),
      m_jsonFormatter(std::make_unique<JsonFormatter>()),
      m_compactFormatter(std::make_unique<CompactFormatter>())
{
    NS_LOG_FUNCTION(this);
}

EventBus::~EventBus()
{
    NS_LOG_FUNCTION(this);
    FinalizeLogging();
    ClearSubscriptions();
    ClearHistory();
}

// ============================================================================
// Configuration API
// ============================================================================

void
EventBus::SetLoggingConfig(const LoggingConfig& config)
{
    NS_LOG_FUNCTION(this);
    m_config = config;
    
    // Update formatter if specified
    if (!config.defaultFormat.empty())
    {
        SetFormatterByName(config.defaultFormat);
    }
}

LoggingConfig
EventBus::GetLoggingConfig() const
{
    return m_config;
}

void
EventBus::SetFormatter(LogFormatterPtr formatter)
{
    NS_LOG_FUNCTION(this);
    if (formatter)
    {
        m_formatter = std::move(formatter);
    }
}

void
EventBus::SetFormatterByName(const std::string& formatName)
{
    NS_LOG_FUNCTION(this << formatName);
    m_formatter = CreateFormatter(formatName);
}

std::string
EventBus::GetRunDirectory() const
{
    return m_runDirectory;
}

std::string
EventBus::GetCategoryDirectory(EventCategory category) const
{
    if (m_runDirectory.empty())
    {
        return "";
    }
    return m_runDirectory + "/" + GetCategorySubdir(category);
}

// ============================================================================
// Publish/Subscribe API
// ============================================================================

void
EventBus::Publish(const MtdEvent& event)
{
    NS_LOG_FUNCTION(this << static_cast<int>(event.type));
    
    ++m_totalEventsPublished;
    
    // Auto-initialize logging on first event
    if (m_config.enabled && m_config.autoInitialize && !m_loggingInitialized)
    {
        InitializeLogging();
    }
    
    // Store in history if enabled
    if (m_historyEnabled)
    {
        if (m_eventHistory.size() >= m_config.maxHistorySize)
        {
            m_eventHistory.erase(m_eventHistory.begin());
        }
        m_eventHistory.push_back(event);
    }

    // Write to log files
    if (m_loggingInitialized)
    {
        LogEventToFiles(event);
    }
    
    // Notify subscribers
    NotifySubscribers(event);
}

uint32_t
EventBus::Subscribe(EventType eventType, EventCallback callback)
{
    NS_LOG_FUNCTION(this << static_cast<int>(eventType));
    
    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.eventType = eventType;
    sub.callback = callback;
    sub.allEvents = false;
    
    m_subscriptions[eventType].push_back(sub);
    
    return sub.id;
}

void
EventBus::Unsubscribe(uint32_t subscriptionId)
{
    NS_LOG_FUNCTION(this << subscriptionId);
    
    // Search in event-specific subscriptions
    for (auto& pair : m_subscriptions)
    {
        auto& subs = pair.second;
        for (auto it = subs.begin(); it != subs.end(); ++it)
        {
            if (it->id == subscriptionId)
            {
                subs.erase(it);
                return;
            }
        }
    }
    
    // Search in global subscriptions
    for (auto it = m_globalSubscriptions.begin(); it != m_globalSubscriptions.end(); ++it)
    {
        if (it->id == subscriptionId)
        {
            m_globalSubscriptions.erase(it);
            return;
        }
    }
}

uint32_t
EventBus::SubscribeAll(EventCallback callback)
{
    NS_LOG_FUNCTION(this);
    
    Subscription sub;
    sub.id = m_nextSubscriptionId++;
    sub.callback = callback;
    sub.allEvents = true;
    
    m_globalSubscriptions.push_back(sub);
    
    return sub.id;
}

size_t
EventBus::GetPendingEventCount() const
{
    return 0; // Events are processed synchronously
}

void
EventBus::ClearSubscriptions()
{
    NS_LOG_FUNCTION(this);
    m_subscriptions.clear();
    m_globalSubscriptions.clear();
}

// ============================================================================
// History API
// ============================================================================

void
EventBus::SetHistoryEnabled(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_historyEnabled = enable;
}

void
EventBus::SetLogging(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_historyEnabled = enable;
    m_config.enabled = enable;
}

std::vector<MtdEvent>
EventBus::GetEventHistory() const
{
    return m_eventHistory;
}

void
EventBus::ClearHistory()
{
    NS_LOG_FUNCTION(this);
    m_eventHistory.clear();
}

// ============================================================================
// File Logging API
// ============================================================================

void
EventBus::InitializeLogging()
{
    NS_LOG_FUNCTION(this);
    
    if (m_loggingInitialized)
    {
        return;
    }
    
    if (!m_config.enabled)
    {
        return;
    }
    
    // Generate timestamped directory
    m_runDirectory = GenerateTimestampDirectory();
    
    // Create directory structure
    CreateDirectoryStructure();
    
    // Initialize buffer manager
    m_bufferManager = std::make_unique<LogBufferManager>(m_config.bufferConfig);
    
    // Write system metadata
    WriteSystemMetadata();
    
    m_loggingInitialized = true;
    
    // Schedule finalization at end of simulation
    // Use weak reference pattern to avoid preventing destruction
    Ptr<EventBus> self = this;
    Simulator::ScheduleDestroy([self]() {
        if (self != nullptr)
        {
            self->FinalizeLogging();
        }
    });
    
    NS_LOG_INFO("Logging initialized: " << m_runDirectory);
}

bool
EventBus::IsLoggingInitialized() const
{
    return m_loggingInitialized;
}

void
EventBus::FinalizeLogging()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_loggingInitialized)
    {
        return;
    }
    
    // Flush and close all buffers
    if (m_bufferManager)
    {
        m_bufferManager->FlushAll();
        m_bufferManager->CloseAll();
    }
    
    // Write final statistics
    std::ostringstream ss;
    ss << "{\n";
    ss << "  \"finalized\": true,\n";
    ss << "  \"totalEvents\": " << m_totalEventsPublished << ",\n";
    ss << "  \"simulationTimeNs\": " << Simulator::Now().GetNanoSeconds() << "\n";
    ss << "}\n";
    
    std::string statsPath = m_runDirectory + "/run_stats.json";
    std::ofstream statsFile(statsPath);
    if (statsFile.is_open())
    {
        statsFile << ss.str();
        statsFile.close();
    }
    
    NS_LOG_INFO("Logging finalized: " << m_totalEventsPublished << " events written");
}

void
EventBus::FlushLogs()
{
    NS_LOG_FUNCTION(this);
    if (m_bufferManager)
    {
        m_bufferManager->FlushAll();
    }
}

std::map<std::string, size_t>
EventBus::GetLoggingStatistics() const
{
    std::map<std::string, size_t> stats;
    stats["totalEventsPublished"] = m_totalEventsPublished;
    stats["historySize"] = m_eventHistory.size();
    stats["loggingInitialized"] = m_loggingInitialized ? 1 : 0;
    
    if (m_bufferManager)
    {
        auto bufferStats = m_bufferManager->GetStatistics();
        for (const auto& pair : bufferStats)
        {
            // Use filename only as key
            std::filesystem::path p(pair.first);
            stats["file_" + p.filename().string()] = pair.second;
        }
    }
    
    return stats;
}

// ============================================================================
// Legacy Compatibility API
// ============================================================================

void
EventBus::EnableFileLogging(const std::string& outputDir)
{
    NS_LOG_FUNCTION(this << outputDir);
    
    // Legacy: Use provided directory as base
    m_config.directoryConfig.baseDir = outputDir;
    m_config.directoryConfig.useTimestampDir = false;  // Use exact path
    m_config.enabled = true;
    
    InitializeLogging();
}

void
EventBus::DisableFileLogging()
{
    NS_LOG_FUNCTION(this);
    FinalizeLogging();
    m_loggingInitialized = false;
}

bool
EventBus::IsFileLoggingEnabled() const
{
    return m_loggingInitialized;
}

void
EventBus::SetFileLogLevel(FileLogLevel level)
{
    NS_LOG_FUNCTION(this << static_cast<int>(level));
    
    // Map legacy level to formatter
    if (level == FileLogLevel::DEBUG)
    {
        // Use text formatter with full metadata
        m_formatter = std::make_unique<TextFormatter>();
    }
    else
    {
        m_formatter = std::make_unique<CompactFormatter>();
    }
}

void
EventBus::SetFlushPolicy(size_t flushEveryN, bool strongConsistency)
{
    NS_LOG_FUNCTION(this << flushEveryN << strongConsistency);
    
    if (strongConsistency)
    {
        m_config.bufferConfig = LogBufferConfig::HighDurability();
    }
    else if (flushEveryN > 0)
    {
        m_config.bufferConfig.bufferCapacity = flushEveryN * 100;  // Approximate
    }
}

// ============================================================================
// Internal Methods
// ============================================================================

void
EventBus::NotifySubscribers(const MtdEvent& event)
{
    // Notify event-specific subscribers
    auto it = m_subscriptions.find(event.type);
    if (it != m_subscriptions.end())
    {
        for (const auto& sub : it->second)
        {
            sub.callback(event);
        }
    }
    
    // Notify global subscribers
    for (const auto& sub : m_globalSubscriptions)
    {
        sub.callback(event);
    }
}

void
EventBus::LogEventToFiles(const MtdEvent& event)
{
    if (!m_bufferManager)
    {
            return;
        }
    
    // 1. Write to timeline_all (compact format)
    if (m_config.enableTimeline)
    {
        std::string timelinePath = m_runDirectory + "/timeline_all.log";
        std::string formatted = m_compactFormatter->Format(event);
        m_bufferManager->Write(timelinePath, formatted);
    }
    
    // 2. Write to events.jsonl (always JSON)
    if (m_config.enableJsonExport)
    {
        std::string jsonPath = m_runDirectory + "/events.jsonl";
        std::string formatted = m_jsonFormatter->Format(event);
        m_bufferManager->Write(jsonPath, formatted);
    }
    
    // 3. Write to per-type file in category subdirectory
    if (m_config.enablePerTypeFiles)
    {
        EventCategory category = GetEventCategory(event.type);
        std::string categoryDir = GetCategoryDirectory(category);
        std::string filename = GetEventTypeFilename(event.type) + m_formatter->GetFileExtension();
        std::string filePath = categoryDir + "/" + filename;
        
        std::string formatted = m_formatter->Format(event);
        m_bufferManager->Write(filePath, formatted);
}

    // Flush periodically for durability (every 100 events)
    if (m_totalEventsPublished % 100 == 0)
    {
        m_bufferManager->FlushAll();
    }
}

void
EventBus::CreateDirectoryStructure()
{
    NS_LOG_FUNCTION(this);
    
    std::error_code ec;
    
    // Create run directory
    if (!std::filesystem::exists(m_runDirectory))
    {
        if (!std::filesystem::create_directories(m_runDirectory, ec))
    {
            NS_LOG_ERROR("Failed to create run directory: " << m_runDirectory 
                         << " - " << ec.message());
            return;
        }
    }
    
    // Create subdirectories
    if (m_config.directoryConfig.createSubdirs)
{
        std::vector<std::string> subdirs = {
            m_config.directoryConfig.attackSubdir,
            m_config.directoryConfig.defenseSubdir,
            m_config.directoryConfig.scoreSubdir,
            m_config.directoryConfig.systemSubdir,
            m_config.directoryConfig.pcapSubdir
        };
        
        for (const auto& subdir : subdirs)
        {
            std::string path = m_runDirectory + "/" + subdir;
            if (!std::filesystem::exists(path))
    {
                std::filesystem::create_directories(path, ec);
    }
        }
    }
    
    NS_LOG_DEBUG("Created directory structure at: " << m_runDirectory);
    }

void
EventBus::WriteSystemMetadata()
{
    std::ostringstream ss;
    ss << std::fixed << std::setprecision(3);
    
    // Get current time
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    
    ss << "{\n";
    ss << "  \"version\": \"1.0\",\n";
    ss << "  \"module\": \"mtd-benchmark\",\n";
    ss << "  \"runDirectory\": \"" << m_runDirectory << "\",\n";
    ss << "  \"startTime\": \"" << std::put_time(std::localtime(&nowTime), "%Y-%m-%dT%H:%M:%S") << "\",\n";
    ss << "  \"loggingConfig\": {\n";
    ss << "    \"enabled\": " << (m_config.enabled ? "true" : "false") << ",\n";
    ss << "    \"bufferCapacity\": " << m_config.bufferConfig.bufferCapacity << ",\n";
    ss << "    \"defaultFormat\": \"" << m_config.defaultFormat << "\",\n";
    ss << "    \"enableTimeline\": " << (m_config.enableTimeline ? "true" : "false") << ",\n";
    ss << "    \"enableJsonExport\": " << (m_config.enableJsonExport ? "true" : "false") << ",\n";
    ss << "    \"enablePerTypeFiles\": " << (m_config.enablePerTypeFiles ? "true" : "false") << "\n";
    ss << "  },\n";
    ss << "  \"directories\": {\n";
    ss << "    \"attack\": \"" << m_config.directoryConfig.attackSubdir << "\",\n";
    ss << "    \"defense\": \"" << m_config.directoryConfig.defenseSubdir << "\",\n";
    ss << "    \"score\": \"" << m_config.directoryConfig.scoreSubdir << "\",\n";
    ss << "    \"system\": \"" << m_config.directoryConfig.systemSubdir << "\",\n";
    ss << "    \"pcap\": \"" << m_config.directoryConfig.pcapSubdir << "\"\n";
    ss << "  }\n";
    ss << "}\n";
    
    std::string metaPath = m_runDirectory + "/system_meta.json";
    std::ofstream metaFile(metaPath);
    if (metaFile.is_open())
        {
        metaFile << ss.str();
        metaFile.close();
        NS_LOG_DEBUG("Wrote system metadata to: " << metaPath);
        }
    }

std::string
EventBus::GenerateTimestampDirectory() const
{
    std::string baseDir = m_config.directoryConfig.baseDir;

    if (!m_config.directoryConfig.useTimestampDir)
    {
        return baseDir;
    }
    
    // Generate timestamp
    auto now = std::chrono::system_clock::now();
    auto nowTime = std::chrono::system_clock::to_time_t(now);
    
    std::ostringstream ss;
    ss << std::put_time(std::localtime(&nowTime), 
                        m_config.directoryConfig.timestampFormat.c_str());
    
    return baseDir + "/" + ss.str();
    }

EventCategory
EventBus::GetEventCategory(EventType type) const
{
    switch (type)
    {
        case EventType::ATTACK_DETECTED:
        case EventType::ATTACK_STARTED:
        case EventType::ATTACK_STOPPED:
            return EventCategory::ATTACK;
            
        case EventType::SHUFFLE_TRIGGERED:
        case EventType::SHUFFLE_COMPLETED:
        case EventType::PROXY_SWITCHED:
            return EventCategory::DEFENSE;
            
        case EventType::SCORE_UPDATED:
        case EventType::THRESHOLD_EXCEEDED:
        case EventType::USER_BANNED:
            return EventCategory::SCORE;
            
        case EventType::DOMAIN_SPLIT:
        case EventType::DOMAIN_MERGE:
        case EventType::USER_MIGRATED:
        default:
            return EventCategory::SYSTEM;
}
}

std::string
EventBus::GetCategorySubdir(EventCategory category) const
{
    switch (category)
{
        case EventCategory::ATTACK:
            return m_config.directoryConfig.attackSubdir;
        case EventCategory::DEFENSE:
            return m_config.directoryConfig.defenseSubdir;
        case EventCategory::SCORE:
            return m_config.directoryConfig.scoreSubdir;
        case EventCategory::SYSTEM:
        default:
            return m_config.directoryConfig.systemSubdir;
    }
}

std::string
EventBus::GetEventTypeFilename(EventType type) const
{
    // Convert event type to lowercase filename
    std::string name = EventTypeToString(type);
    std::transform(name.begin(), name.end(), name.begin(), ::tolower);
    return name;
}

} // namespace mtd
} // namespace ns3
