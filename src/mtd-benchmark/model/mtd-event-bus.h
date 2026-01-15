/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Event Bus for inter-module communication
 * 
 * Refactored design with:
 * - Automatic initialization and directory management
 * - Strategy pattern for log formatting (Text, JSON)
 * - Buffered I/O for high-performance logging
 * - Organized subdirectory structure
 */

#ifndef MTD_EVENT_BUS_H
#define MTD_EVENT_BUS_H

#include "mtd-common.h"
#include "mtd-log-formatter.h"
#include "mtd-log-buffer.h"

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"

#include <map>
#include <vector>
#include <memory>
#include <string>

namespace ns3 {
namespace mtd {

/**
 * \brief Log directory configuration
 */
struct LogDirectoryConfig
{
    std::string baseDir{"logs"};           ///< Base directory for all logs
    bool useTimestampDir{true};            ///< Create timestamped subdirectory
    bool createSubdirs{true};              ///< Create category subdirectories
    std::string timestampFormat{"%Y%m%d_%H%M%S"};  ///< strftime format
    
    // Subdirectory names
    std::string attackSubdir{"attack"};
    std::string defenseSubdir{"defense"};
    std::string scoreSubdir{"score"};
    std::string systemSubdir{"system"};
    std::string pcapSubdir{"pcap"};
};

/**
 * \brief Logging system configuration
 */
struct LoggingConfig
{
    bool enabled{true};                         ///< Master enable/disable
    bool autoInitialize{true};                  ///< Auto-init on first event
    LogBufferConfig bufferConfig{LogBufferConfig::Default()};
    LogDirectoryConfig directoryConfig;
    std::string defaultFormat{"text"};          ///< "text", "json", "compact"
    bool enableTimeline{true};                  ///< Create timeline_all.log
    bool enableJsonExport{true};                ///< Create events.jsonl
    bool enablePerTypeFiles{true};              ///< Create per-event-type files
    size_t maxHistorySize{10000};               ///< In-memory history limit
};

/**
 * \brief Event category for routing to subdirectories
 */
enum class EventCategory
{
    ATTACK,      ///< Attack-related events
    DEFENSE,     ///< MTD defense events (shuffle, proxy switch)
    SCORE,       ///< Scoring and risk events
    SYSTEM       ///< System/domain events
};

/**
 * \brief Event Bus for decoupled inter-module communication
 * 
 * The EventBus enables publish-subscribe pattern for events between
 * MTD modules without direct coupling. Features:
 * 
 * - Automatic logging initialization on first event
 * - Timestamp-based run directories: logs/YYYYMMDD_HHMMSS/
 * - Category-based subdirectories: attack/, defense/, score/
 * - Pluggable formatters (Text, JSON, Compact)
 * - Buffered I/O for high-frequency scenarios
 * 
 * Usage:
 * \code
 *   Ptr<EventBus> bus = CreateObject<EventBus>();
 *   // Logging is auto-enabled; just publish events
 *   MtdEvent event;
 *   event.type = EventType::ATTACK_DETECTED;
 *   bus->Publish(event);
 * \endcode
 */
class EventBus : public Object
{
public:
    static TypeId GetTypeId();
    
    EventBus();
    ~EventBus() override;

    // ==================== Configuration API ====================

    /**
     * \brief Set logging configuration (call before first Publish)
     * \param config Logging configuration
     */
    void SetLoggingConfig(const LoggingConfig& config);

    /**
     * \brief Get current logging configuration
     * \return Current configuration
     */
    LoggingConfig GetLoggingConfig() const;

    /**
     * \brief Set the log formatter strategy
     * \param formatter Unique pointer to formatter (takes ownership)
     */
    void SetFormatter(LogFormatterPtr formatter);

    /**
     * \brief Set formatter by name
     * \param formatName "text", "json", or "compact"
     */
    void SetFormatterByName(const std::string& formatName);

    /**
     * \brief Get the current run directory path
     * \return Full path to current run directory (e.g., logs/20260114_153000/)
     */
    std::string GetRunDirectory() const;

    /**
     * \brief Get path to a specific subdirectory
     * \param category Event category
     * \return Full path to subdirectory
     */
    std::string GetCategoryDirectory(EventCategory category) const;

    // ==================== Publish/Subscribe API ====================
    
    /**
     * \brief Publish an event to all subscribers
     * \param event The event to publish
     */
    void Publish(const MtdEvent& event);
    
    /**
     * \brief Subscribe to a specific event type
     * \param eventType The type of events to subscribe to
     * \param callback The callback function to invoke when event occurs
     * \return Subscription ID for later unsubscription
     */
    uint32_t Subscribe(EventType eventType, EventCallback callback);
    
    /**
     * \brief Unsubscribe from events
     * \param subscriptionId The subscription ID returned from Subscribe
     */
    void Unsubscribe(uint32_t subscriptionId);
    
    /**
     * \brief Subscribe to all event types
     * \param callback The callback function to invoke for any event
     * \return Subscription ID
     */
    uint32_t SubscribeAll(EventCallback callback);
    
    /**
     * \brief Get pending events count
     * \return Number of events in the queue
     */
    size_t GetPendingEventCount() const;
    
    /**
     * \brief Clear all subscriptions
     */
    void ClearSubscriptions();
    
    // ==================== History API ====================
    
    /**
     * \brief Enable/disable in-memory event history
     * \param enable Whether to enable history
     */
    void SetHistoryEnabled(bool enable);
    
    /**
     * \brief Get event history
     * \return Vector of past events
     */
    std::vector<MtdEvent> GetEventHistory() const;
    
    /**
     * \brief Clear event history
     */
    void ClearHistory();

    // ==================== File Logging API ====================

    /**
     * \brief Manually initialize logging (called automatically on first event)
     * 
     * Creates the run directory structure and opens log files.
     * Safe to call multiple times (no-op if already initialized).
     */
    void InitializeLogging();

    /**
     * \brief Check if logging is initialized
     * \return True if logging system is active
     */
    bool IsLoggingInitialized() const;

    /**
     * \brief Finalize logging (flush all buffers, write metadata)
     * 
     * Called automatically on destruction, but can be called explicitly
     * to ensure all data is written before simulation ends.
     */
    void FinalizeLogging();

    /**
     * \brief Force flush all log buffers to disk
     */
    void FlushLogs();

    /**
     * \brief Get logging statistics
     * \return Map of metric name to value
     */
    std::map<std::string, size_t> GetLoggingStatistics() const;

    // ==================== Legacy Compatibility API ====================
    // These methods are kept for backward compatibility

    /**
     * \brief Legacy: Enable/disable event logging
     * \param enable Whether to enable logging
     * \deprecated Use SetLoggingConfig instead
     */
    void SetLogging(bool enable);

    /**
     * \brief Legacy: Enable file-based event logging
     * \param outputDir Directory to write log files
     * \deprecated Logging is now automatic; use SetLoggingConfig for customization
     */
    void EnableFileLogging(const std::string& outputDir);

    /**
     * \brief Legacy: Disable file-based logging
     * \deprecated Use FinalizeLogging instead
     */
    void DisableFileLogging();

    /**
     * \brief Legacy: Check if file logging is enabled
     * \return True if file logging is active
     * \deprecated Use IsLoggingInitialized instead
     */
    bool IsFileLoggingEnabled() const;

    /**
     * \brief Legacy: Set file log level
     * \param level INFO or DEBUG level
     * \deprecated Use SetFormatter instead
     */
    void SetFileLogLevel(FileLogLevel level);

    /**
     * \brief Legacy: Configure flush policy
     * \param flushEveryN Flush after every N events
     * \param strongConsistency Flush after every write
     * \deprecated Use SetLoggingConfig with bufferConfig instead
     */
    void SetFlushPolicy(size_t flushEveryN, bool strongConsistency = false);

private:
    struct Subscription {
        uint32_t id{0};
        EventType eventType{EventType::SHUFFLE_TRIGGERED};
        EventCallback callback;
        bool allEvents{false};
    };
    
    // Subscription management
    std::map<EventType, std::vector<Subscription>> m_subscriptions;
    std::vector<Subscription> m_globalSubscriptions;
    uint32_t m_nextSubscriptionId{1};
    
    // Event history
    std::vector<MtdEvent> m_eventHistory;
    bool m_historyEnabled{true};
    
    // Configuration
    LoggingConfig m_config;

    // Logging infrastructure
    bool m_loggingInitialized{false};
    std::string m_runDirectory;
    LogFormatterPtr m_formatter;
    LogFormatterPtr m_jsonFormatter;     // Always JSON for events.jsonl
    LogFormatterPtr m_compactFormatter;  // Always compact for timeline
    std::unique_ptr<LogBufferManager> m_bufferManager;
    
    // Statistics
    size_t m_totalEventsPublished{0};
    
    // Internal methods
    void NotifySubscribers(const MtdEvent& event);
    void LogEventToFiles(const MtdEvent& event);
    void CreateDirectoryStructure();
    void WriteSystemMetadata();
    std::string GenerateTimestampDirectory() const;
    EventCategory GetEventCategory(EventType type) const;
    std::string GetCategorySubdir(EventCategory category) const;
    std::string GetEventTypeFilename(EventType type) const;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_EVENT_BUS_H
