/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Event Bus implementation
 */

#include "mtd-event-bus.h"
#include "ns3/log.h"

#include <sstream>
#include <iomanip>
#include <filesystem>

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
{
    NS_LOG_FUNCTION(this);
}

EventBus::~EventBus()
{
    NS_LOG_FUNCTION(this);
    CloseAllFiles();
    ClearSubscriptions();
    ClearHistory();
}

void
EventBus::Publish(const MtdEvent& event)
{
    NS_LOG_FUNCTION(this << static_cast<int>(event.type));
    
    // Store in history if logging enabled
    if (m_loggingEnabled)
    {
        if (m_eventHistory.size() >= m_maxHistorySize)
        {
            m_eventHistory.erase(m_eventHistory.begin());
        }
        m_eventHistory.push_back(event);
    }

    // File-based logging (automatic for all events)
    if (m_fileLoggingEnabled)
    {
        LogEventToFile(event);
    }
    
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

void
EventBus::SetLogging(bool enable)
{
    NS_LOG_FUNCTION(this << enable);
    m_loggingEnabled = enable;
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

void
EventBus::NotifySubscribers(const MtdEvent& event)
{
    NS_LOG_FUNCTION(this << static_cast<int>(event.type));
    
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

// ==================== File Logging Implementation ====================

void
EventBus::EnableFileLogging(const std::string& outputDir)
{
    NS_LOG_FUNCTION(this << outputDir);

    // Create directory if not exists
    std::filesystem::path dirPath(outputDir);
    if (!std::filesystem::exists(dirPath))
    {
        std::error_code ec;
        if (!std::filesystem::create_directories(dirPath, ec))
        {
            NS_LOG_ERROR("Failed to create log directory: " << outputDir << " - " << ec.message());
            return;
        }
        NS_LOG_INFO("Created log directory: " << outputDir);
    }

    m_logOutputDir = outputDir;
    m_fileLoggingEnabled = true;

    // Open aggregate log files
    std::string allInfoPath = outputDir + "/ALL_INFO.log";
    std::string allDebugPath = outputDir + "/ALL_INFO_DEBUG.log";

    m_allInfoFile = std::make_unique<std::ofstream>(allInfoPath, std::ios::app);
    m_allInfoDebugFile = std::make_unique<std::ofstream>(allDebugPath, std::ios::app);

    if (!m_allInfoFile->is_open())
    {
        NS_LOG_ERROR("Failed to open: " << allInfoPath);
    }
    if (!m_allInfoDebugFile->is_open())
    {
        NS_LOG_ERROR("Failed to open: " << allDebugPath);
    }

    m_eventsSinceFlush = 0;
    NS_LOG_INFO("File logging enabled: " << outputDir);
}

void
EventBus::DisableFileLogging()
{
    NS_LOG_FUNCTION(this);
    CloseAllFiles();
    m_fileLoggingEnabled = false;
}

bool
EventBus::IsFileLoggingEnabled() const
{
    return m_fileLoggingEnabled;
}

void
EventBus::SetFileLogLevel(FileLogLevel level)
{
    NS_LOG_FUNCTION(this << static_cast<int>(level));
    m_fileLogLevel = level;
}

void
EventBus::SetFlushPolicy(size_t flushEveryN, bool strongConsistency)
{
    NS_LOG_FUNCTION(this << flushEveryN << strongConsistency);
    m_flushEveryN = flushEveryN;
    m_strongConsistency = strongConsistency;
}

void
EventBus::FlushLogs()
{
    NS_LOG_FUNCTION(this);
    
    if (m_allInfoFile && m_allInfoFile->is_open())
    {
        m_allInfoFile->flush();
    }
    if (m_allInfoDebugFile && m_allInfoDebugFile->is_open())
    {
        m_allInfoDebugFile->flush();
    }
    for (auto& pair : m_eventFiles)
    {
        if (pair.second && pair.second->is_open())
        {
            pair.second->flush();
        }
    }
    m_eventsSinceFlush = 0;
}

void
EventBus::LogEventToFile(const MtdEvent& event)
{
    std::string infoEntry = FormatLogEntry(event, FileLogLevel::INFO);
    std::string debugEntry = FormatLogEntry(event, FileLogLevel::DEBUG);

    // 1. Per-event-type file (e.g., ATTACK_DETECTED.log)
    EnsureEventFileOpen(event.type);
    auto it = m_eventFiles.find(event.type);
    if (it != m_eventFiles.end() && it->second && it->second->is_open())
    {
        // Per-type file uses the configured level
        if (m_fileLogLevel == FileLogLevel::DEBUG)
        {
            *it->second << debugEntry;
        }
        else
        {
            *it->second << infoEntry;
        }
    }

    // 2. ALL_INFO.log (INFO level only)
    if (m_allInfoFile && m_allInfoFile->is_open())
    {
        *m_allInfoFile << infoEntry;
    }

    // 3. ALL_INFO_DEBUG.log (always DEBUG level)
    if (m_allInfoDebugFile && m_allInfoDebugFile->is_open())
    {
        *m_allInfoDebugFile << debugEntry;
    }

    ++m_eventsSinceFlush;
    CheckAndFlush();
}

std::string
EventBus::FormatLogEntry(const MtdEvent& event, FileLogLevel level) const
{
    std::ostringstream ss;
    
    // Timestamp is stored in milliseconds across the MTD-Benchmark module.
    double timeMs = static_cast<double>(event.timestamp);
    ss << std::fixed << std::setprecision(3);
    ss << "[" << timeMs << "ms] ";
    ss << EventTypeToString(event.type);
    ss << " node=" << event.sourceNodeId;

    // Include metadata for DEBUG level
    if (level == FileLogLevel::DEBUG && !event.metadata.empty())
    {
        ss << " {";
        bool first = true;
        for (const auto& kv : event.metadata)
        {
            if (!first) ss << ", ";
            ss << kv.first << "=" << kv.second;
            first = false;
        }
        ss << "}";
    }
    ss << "\n";
    return ss.str();
}

void
EventBus::EnsureEventFileOpen(EventType type)
{
    if (m_eventFiles.find(type) == m_eventFiles.end() || !m_eventFiles[type])
    {
        std::string filename = m_logOutputDir + "/" + EventTypeToString(type) + ".log";
        m_eventFiles[type] = std::make_unique<std::ofstream>(filename, std::ios::app);

        if (!m_eventFiles[type]->is_open())
        {
            NS_LOG_ERROR("Failed to open log file: " << filename);
        }
    }
}

void
EventBus::CloseAllFiles()
{
    NS_LOG_FUNCTION(this);

    // Flush before closing
    FlushLogs();

    if (m_allInfoFile)
    {
        m_allInfoFile->close();
        m_allInfoFile.reset();
    }
    if (m_allInfoDebugFile)
    {
        m_allInfoDebugFile->close();
        m_allInfoDebugFile.reset();
    }
    for (auto& pair : m_eventFiles)
    {
        if (pair.second)
        {
            pair.second->close();
            pair.second.reset();
        }
    }
    m_eventFiles.clear();
}

void
EventBus::CheckAndFlush()
{
    if (m_strongConsistency)
    {
        FlushLogs();
    }
    else if (m_flushEveryN > 0 && m_eventsSinceFlush >= m_flushEveryN)
    {
        FlushLogs();
    }
}

} // namespace mtd
} // namespace ns3
