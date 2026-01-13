/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Analysis Helper implementation
 */

#include "mtd-analysis-helper.h"

#include "ns3/log.h"
#include "ns3/simulator.h"

#include "ns3/flow-monitor-helper.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdAnalysisHelper");

namespace {

static void
SinkRxThunk(MtdAnalysisHelper* self,
            uint32_t proxyId,
            Ptr<const Packet> packet,
            const Address& from)
{
    self->OnSinkRx(proxyId, packet, from);
}

} // namespace

struct MtdAnalysisHelper::FlowImpl
{
    FlowMonitorHelper helper;
    Ptr<::ns3::FlowMonitor> monitor;

    struct Totals
    {
        uint64_t rxBytes{0};
        uint64_t rxPackets{0};
        uint64_t lostPackets{0};
        Time delaySum{Seconds(0.0)};
    };

    std::map<uint32_t, Totals> lastTotals;
};

NS_OBJECT_ENSURE_REGISTERED(MtdAnalysisHelper);

TypeId
MtdAnalysisHelper::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::MtdAnalysisHelper")
                            .SetParent<Object>()
                            .SetGroupName("MtdBenchmark")
                            .AddConstructor<MtdAnalysisHelper>();
    return tid;
}

MtdAnalysisHelper::MtdAnalysisHelper() = default;

MtdAnalysisHelper::~MtdAnalysisHelper() = default;

void
MtdAnalysisHelper::SetConfig(const Config& cfg)
{
    m_cfg = cfg;
    if (m_cfg.mode == Mode::HEAVYWEIGHT_FLOWMONITOR && !m_flow)
    {
        m_flow = std::make_unique<FlowImpl>();
    }
    if (m_cfg.mode == Mode::LIGHTWEIGHT_SINK)
    {
        m_flow.reset();
    }
}

MtdAnalysisHelper::Config
MtdAnalysisHelper::GetConfig() const
{
    return m_cfg;
}

void
MtdAnalysisHelper::SetNetworkHelper(const MtdNetworkHelper* networkHelper)
{
    m_networkHelper = networkHelper;
}

void
MtdAnalysisHelper::AttachProxySinks(const std::map<uint32_t, Ptr<PacketSink>>& proxySinks)
{
    m_proxySinks = proxySinks;

    for (const auto& [proxyId, sink] : m_proxySinks)
    {
        if (!sink)
        {
            continue;
        }

        // Initialize counters.
        m_sinkPackets[proxyId] = 0;
        m_lastBytesIn[proxyId] = sink->GetTotalRx();
        m_lastPacketsIn[proxyId] = 0;

        sink->TraceConnectWithoutContext(
            "Rx",
            MakeBoundCallback(&SinkRxThunk, this, proxyId));
    }
}

void
MtdAnalysisHelper::InstallFlowMonitor(const NodeContainer& nodes)
{
    if (!m_flow)
    {
        m_flow = std::make_unique<FlowImpl>();
    }

    m_flow->monitor = m_flow->helper.Install(nodes);
}

Ptr<::ns3::FlowMonitor>
MtdAnalysisHelper::GetFlowMonitor() const
{
    if (m_flow)
    {
        return m_flow->monitor;
    }
    return nullptr;
}

void
MtdAnalysisHelper::OnSinkRx(uint32_t proxyId, Ptr<const Packet> packet, const Address&)
{
    (void)packet;
    m_sinkPackets[proxyId]++;
}

void
MtdAnalysisHelper::CollectStats()
{
    const Time now = Simulator::Now();
    const double dt = (m_lastCollectTime.IsZero()) ? m_cfg.sampleInterval.GetSeconds()
                                                   : (now - m_lastCollectTime).GetSeconds();
    m_lastCollectTime = now;

    if (dt <= 0.0)
    {
        return;
    }

    m_lastStats.clear();

    if (m_cfg.mode == Mode::LIGHTWEIGHT_SINK)
    {
        for (const auto& [proxyId, sink] : m_proxySinks)
        {
            if (!sink)
            {
                continue;
            }

            const uint64_t bytes = sink->GetTotalRx();
            const uint64_t packets = m_sinkPackets[proxyId];

            const uint64_t lastBytes = m_lastBytesIn[proxyId];
            const uint64_t lastPackets = m_lastPacketsIn[proxyId];

            const uint64_t dBytes = (bytes >= lastBytes) ? (bytes - lastBytes) : 0;
            const uint64_t dPackets = (packets >= lastPackets) ? (packets - lastPackets) : 0;

            TrafficStats ts;
            ts.bytesIn = bytes;
            ts.packetsIn = packets;
            ts.byteRate = static_cast<double>(dBytes) / dt;
            ts.packetRate = static_cast<double>(dPackets) / dt;
            ts.activeConnections = 0;
            ts.avgLatency = 0.0;

            m_lastStats[proxyId] = ts;
            m_lastBytesIn[proxyId] = bytes;
            m_lastPacketsIn[proxyId] = packets;
        }

        return;
    }

    // Heavyweight FlowMonitor mode
    if (!m_flow || m_flow->monitor == nullptr)
    {
        return;
    }

    if (m_networkHelper == nullptr)
    {
        return;
    }

    m_flow->monitor->CheckForLostPackets();

    std::map<uint32_t, FlowImpl::Totals> totals;

    const auto classifier = DynamicCast<Ipv4FlowClassifier>(m_flow->helper.GetClassifier());
    const auto stats = m_flow->monitor->GetFlowStats();

    for (const auto& [flowId, flowStats] : stats)
    {
        const Ipv4FlowClassifier::FiveTuple t = classifier->FindFlow(flowId);
        const uint32_t proxyId = m_networkHelper->GetProxyIdByIp(t.destinationAddress);
        if (proxyId == 0)
        {
            continue;
        }

        auto& snap = totals[proxyId];
        snap.rxBytes += flowStats.rxBytes;
        snap.rxPackets += flowStats.rxPackets;
        snap.lostPackets += flowStats.lostPackets;
        snap.delaySum += flowStats.delaySum;
    }

    for (const auto& [proxyId, current] : totals)
    {
        const auto previousIt = m_flow->lastTotals.find(proxyId);
        const FlowImpl::Totals previous =
            (previousIt != m_flow->lastTotals.end()) ? previousIt->second : FlowImpl::Totals{};

        const uint64_t dBytes = (current.rxBytes >= previous.rxBytes) ? (current.rxBytes - previous.rxBytes) : 0;
        const uint64_t dPackets =
            (current.rxPackets >= previous.rxPackets) ? (current.rxPackets - previous.rxPackets) : 0;

        TrafficStats ts;
        ts.bytesIn = current.rxBytes;
        ts.packetsIn = current.rxPackets;
        ts.byteRate = static_cast<double>(dBytes) / dt;
        ts.packetRate = static_cast<double>(dPackets) / dt;
        ts.activeConnections = 0;

        if (current.rxPackets > 0)
        {
            ts.avgLatency = current.delaySum.GetSeconds() / static_cast<double>(current.rxPackets);
        }

        m_lastStats[proxyId] = ts;
        m_flow->lastTotals[proxyId] = current;
    }
}

std::map<uint32_t, TrafficStats>
MtdAnalysisHelper::GetStats(bool perProxy) const
{
    if (perProxy)
    {
        return m_lastStats;
    }

    TrafficStats total;
    for (const auto& [proxyId, ts] : m_lastStats)
    {
        (void)proxyId;
        total.packetsIn += ts.packetsIn;
        total.bytesIn += ts.bytesIn;
        total.packetRate += ts.packetRate;
        total.byteRate += ts.byteRate;
    }

    return {{0u, total}};
}

TrafficStats
MtdAnalysisHelper::GetTrafficStats(uint32_t proxyId) const
{
    auto it = m_lastStats.find(proxyId);
    if (it != m_lastStats.end())
    {
        return it->second;
    }
    return TrafficStats{};
}

void
MtdAnalysisHelper::SerializeFlowMonitorIfEnabled() const
{
    if (!m_cfg.serializeXml)
    {
        return;
    }

    if (!m_flow || m_flow->monitor == nullptr)
    {
        return;
    }

    m_flow->monitor->SerializeToXmlFile(m_cfg.xmlFileName, m_cfg.xmlIncludeProbes, m_cfg.xmlIncludeFlows);
}

} // namespace mtd
} // namespace ns3
