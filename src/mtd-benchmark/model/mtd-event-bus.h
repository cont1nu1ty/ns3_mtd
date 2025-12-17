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
    bool m_loggingEnabled{false};
    std::size_t m_maxHistorySize{10000};
    
    void NotifySubscribers(const MtdEvent& event);
};

} // namespace mtd
} // namespace ns3

#endif // MTD_EVENT_BUS_H
