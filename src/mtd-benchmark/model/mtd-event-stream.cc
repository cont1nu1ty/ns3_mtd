/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Incremental event stream adapter
 */

#include "mtd-event-stream.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdEventStream");
NS_OBJECT_ENSURE_REGISTERED(EventStream);

TypeId
EventStream::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::EventStream")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<EventStream>();
    return tid;
}

EventStream::EventStream()
{
    NS_LOG_FUNCTION(this);
}

EventStream::~EventStream()
{
    NS_LOG_FUNCTION(this);

    if (m_eventBus != nullptr && m_subscriptionId != 0)
    {
        m_eventBus->Unsubscribe(m_subscriptionId);
        m_subscriptionId = 0;
    }
}

void
EventStream::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);

    if (m_eventBus != nullptr && m_subscriptionId != 0)
    {
        m_eventBus->Unsubscribe(m_subscriptionId);
        m_subscriptionId = 0;
    }

    m_eventBus = eventBus;

    if (m_eventBus != nullptr)
    {
        m_subscriptionId = m_eventBus->SubscribeAll(MakeCallback(&EventStream::OnEvent, this));
    }
}

Ptr<EventBus>
EventStream::GetEventBus() const
{
    return m_eventBus;
}

uint64_t
EventStream::GetOldestSeq() const
{
    if (m_buffer.empty())
    {
        return m_nextSeq;
    }
    return m_buffer.front().seq;
}

uint64_t
EventStream::GetNextSeq() const
{
    return m_nextSeq;
}

uint32_t
EventStream::GetBufferedCount() const
{
    return static_cast<uint32_t>(m_buffer.size());
}

uint64_t
EventStream::GetDroppedCount() const
{
    return m_dropped;
}

uint64_t
EventStream::GetThrottledCount() const
{
    return m_throttled;
}

std::vector<MtdEvent>
EventStream::GetEventsSince(uint64_t seq, uint32_t maxEvents)
{
    NS_LOG_FUNCTION(this << seq << maxEvents);

    const uint64_t nowMs = Simulator::Now().GetMilliSeconds();
    if (nowMs < m_lastPollMs + kMinPollIntervalMs)
    {
        ++m_throttled;
        return {};
    }
    m_lastPollMs = nowMs;

    std::vector<MtdEvent> out;
    if (maxEvents == 0 || m_buffer.empty())
    {
        return out;
    }

    const uint64_t oldest = GetOldestSeq();
    if (seq < oldest)
    {
        // Caller is behind the buffer window.
        seq = oldest;
    }

    // Linear scan is OK due to bounded buffer size.
    for (const auto& stored : m_buffer)
    {
        if (stored.seq < seq)
        {
            continue;
        }
        out.push_back(stored.event);
        if (out.size() >= maxEvents)
        {
            break;
        }
    }

    return out;
}

void
EventStream::Clear()
{
    NS_LOG_FUNCTION(this);
    m_buffer.clear();
}

bool
EventStream::IsAllowed(EventType type) const
{
    // Hard-coded coarse-grained allowlist: prevents per-packet style event floods
    // from ever crossing the language boundary.
    switch (type)
    {
        case EventType::ATTACK_DETECTED:
        case EventType::ATTACK_STARTED:
        case EventType::ATTACK_STOPPED:
        case EventType::SHUFFLE_TRIGGERED:
        case EventType::SHUFFLE_COMPLETED:
        case EventType::PROXY_SWITCHED:
        case EventType::SCORE_UPDATED:
        case EventType::THRESHOLD_EXCEEDED:
        case EventType::USER_BANNED:
        case EventType::DOMAIN_SPLIT:
        case EventType::DOMAIN_MERGE:
        case EventType::USER_MIGRATED:
            return true;
        default:
            return false;
    }
}

void
EventStream::OnEvent(const MtdEvent& event)
{
    if (!IsAllowed(event.type))
    {
        return;
    }

    StoredEvent stored;
    stored.seq = m_nextSeq++;
    stored.event = event;

    m_buffer.push_back(std::move(stored));
    TrimIfNeeded();
}

void
EventStream::TrimIfNeeded()
{
    while (m_buffer.size() > kMaxBufferSize)
    {
        m_buffer.pop_front();
        ++m_dropped;
    }
}

} // namespace mtd
} // namespace ns3
