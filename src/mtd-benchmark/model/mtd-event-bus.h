/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Event Bus for inter-module communication
 */

#ifndef MTD_EVENT_BUS_H
#define MTD_EVENT_BUS_H

#include "mtd-common.h"
#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/callback.h"
#include "ns3/simulator.h"

#include <map>
#include <vector>
#include <fstream>
#include <memory>

namespace ns3 {
namespace mtd {

/**
 * \brief Event Bus for decoupled inter-module communication
 * 
 * The EventBus enables publish-subscribe pattern for events between
 * MTD modules without direct coupling. Modules can publish events
 * and subscribe to specific event types.
 */
class EventBus : public Object
{
public:
    static TypeId GetTypeId();
    
    EventBus();
    ~EventBus() override;
    
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
    
    /**
     * \brief Enable/disable event logging
     * \param enable Whether to enable logging
     */
    void SetLogging(bool enable);
    
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
     * \brief Enable file-based event logging
     * \param outputDir Directory to write log files (auto-created if not exists)
     * 
     * Creates per-event-type log files (e.g., ATTACK_DETECTED.log) and two
     * aggregate files: ALL_INFO.log (INFO only) and ALL_INFO_DEBUG.log (with metadata).
     */
    void EnableFileLogging(const std::string& outputDir);

    /**
     * \brief Disable file-based logging and close all files
     */
    void DisableFileLogging();

    /**
     * \brief Check if file logging is enabled
     * \return True if file logging is active
     */
    bool IsFileLoggingEnabled() const;

    /**
     * \brief Set file log level for aggregate logs
     * \param level INFO or DEBUG level
     */
    void SetFileLogLevel(FileLogLevel level);

    /**
     * \brief Configure flush policy
     * \param flushEveryN Flush after every N events (0 = only flush on disable/destroy)
     * \param strongConsistency If true, flush after every write (overrides flushEveryN)
     */
    void SetFlushPolicy(size_t flushEveryN, bool strongConsistency = false);

    /**
     * \brief Force flush all log files now
     */
    void FlushLogs();

private:
    struct Subscription {
        uint32_t id{0};
        EventType eventType{EventType::SHUFFLE_TRIGGERED};
        EventCallback callback;
        bool allEvents{false};
    };
    
    std::map<EventType, std::vector<Subscription>> m_subscriptions;
    std::vector<Subscription> m_globalSubscriptions;
    std::vector<MtdEvent> m_eventHistory;
    uint32_t m_nextSubscriptionId{1};
    // Enabled by default: required for standard dataset export (events.json).
    bool m_loggingEnabled{true};
    std::size_t m_maxHistorySize{10000};

    // File logging members
    bool m_fileLoggingEnabled{false};
    std::string m_logOutputDir;
    FileLogLevel m_fileLogLevel{FileLogLevel::INFO};
    std::map<EventType, std::unique_ptr<std::ofstream>> m_eventFiles;
    std::unique_ptr<std::ofstream> m_allInfoFile;
    std::unique_ptr<std::ofstream> m_allInfoDebugFile;
    size_t m_flushEveryN{0};          ///< Flush after N events (0 = only on close)
    bool m_strongConsistency{false};  ///< Flush after every write
    size_t m_eventsSinceFlush{0};     ///< Counter for batch flush
    
    void NotifySubscribers(const MtdEvent& event);

    // File logging helpers
    void LogEventToFile(const MtdEvent& event);
    std::string FormatLogEntry(const MtdEvent& event, FileLogLevel level) const;
    void EnsureEventFileOpen(EventType type);
    void CloseAllFiles();
    void CheckAndFlush();
};

} // namespace mtd
} // namespace ns3

#endif // MTD_EVENT_BUS_H
