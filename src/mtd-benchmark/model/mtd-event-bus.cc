/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Event Bus implementation
 */

#include "mtd-event-bus.h"
#include "ns3/log.h"

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

} // namespace mtd
} // namespace ns3
