/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Incremental event stream adapter
 */

#ifndef MTD_EVENT_STREAM_H
#define MTD_EVENT_STREAM_H

#include "mtd-common.h"
#include "mtd-event-bus.h"

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/callback.h"

#include <cstdint>
#include <deque>
#include <vector>

namespace ns3 {
namespace mtd {

/**
 * \brief Lightweight incremental event reader for Python bridge.
 *
 * This adapter subscribes to an EventBus and buffers a bounded subset of events
 * with a monotonically increasing sequence number. Consumers can efficiently
 * read event deltas via GetEventsSince(seq) without copying the entire event
 * history across the language boundary.
 *
 * Hard constraints (by design):
 * - Only coarse-grained MTD events are buffered (no per-packet events).
 * - Calls are throttled by a minimum poll interval.
 */
class EventStream : public Object
{
public:
    static TypeId GetTypeId();

    EventStream();
    ~EventStream() override;

    /**
     * \brief Attach to an EventBus (replaces previous bus subscription).
     */
    void SetEventBus(Ptr<EventBus> eventBus);

    Ptr<EventBus> GetEventBus() const;

    /**
     * \return Oldest available sequence number.
     *
     * If the consumer requests a seq < GetOldestSeq(), earlier events were
     * dropped due to bounded buffering.
     */
    uint64_t GetOldestSeq() const;

    /**
     * \return Next sequence number (one past the latest buffered event).
     */
    uint64_t GetNextSeq() const;

    /**
     * \return Number of buffered events currently stored.
     */
    uint32_t GetBufferedCount() const;

    /**
     * \return Number of events dropped due to buffer overflow.
     */
    uint64_t GetDroppedCount() const;

    /**
     * \return Number of polls throttled by the minimum poll interval.
     */
    uint64_t GetThrottledCount() const;

    /**
     * \brief Read at most maxEvents events starting from sequence number seq.
     *
     * Consumers should track the next sequence as:
     *   seq = max(seq, GetOldestSeq())
     *   events = GetEventsSince(seq, maxEvents)
     *   seq += events.size()
     *
     * If seq < GetOldestSeq(), the caller should reset seq to GetOldestSeq().
     */
    std::vector<MtdEvent> GetEventsSince(uint64_t seq, uint32_t maxEvents);

    /**
     * \brief Clear the internal buffer.
     */
    void Clear();

private:
    struct StoredEvent
    {
        uint64_t seq{0};
        MtdEvent event;
    };

    static constexpr uint32_t kMaxBufferSize = 10000;
    static constexpr uint64_t kMinPollIntervalMs = 100; // hard-coded safety guard

    Ptr<EventBus> m_eventBus;
    uint32_t m_subscriptionId{0};

    std::deque<StoredEvent> m_buffer;
    uint64_t m_nextSeq{1};
    uint64_t m_dropped{0};
    uint64_t m_throttled{0};
    uint64_t m_lastPollMs{0};

    bool IsAllowed(EventType type) const;
    void OnEvent(const MtdEvent& event);
    void TrimIfNeeded();
};

} // namespace mtd
} // namespace ns3

#endif // MTD_EVENT_STREAM_H
