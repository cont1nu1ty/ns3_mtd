/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Unified Telemetry Helper
 */

#include "mtd-telemetry-helper.h"

#include "ns3/application.h"
#include "ns3/csma-net-device.h"
#include "ns3/flow-monitor.h"
#include "ns3/ipv4-flow-classifier.h"
#include "ns3/ipv4-header.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/ipv4.h"
#include "ns3/log.h"
#include "ns3/node.h"
#include "ns3/point-to-point-net-device.h"
#include "ns3/queue.h"
#include "ns3/simulator.h"

#include <filesystem>
#include <iomanip>
#include <sstream>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdTelemetryHelper");
NS_OBJECT_ENSURE_REGISTERED(MtdTelemetryHelper);

TypeId
MtdTelemetryHelper::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::MtdTelemetryHelper")
                            .SetParent<Object>()
                            .SetGroupName("MtdBenchmark")
                            .AddConstructor<MtdTelemetryHelper>();
    return tid;
}

MtdTelemetryHelper::MtdTelemetryHelper() = default;

MtdTelemetryHelper::~MtdTelemetryHelper()
{
    if (!m_writeEvent.IsExpired())
    {
        Simulator::Cancel(m_writeEvent);
    }

    if (m_trafficSamples.is_open())
    {
        m_trafficSamples.flush();
        m_trafficSamples.close();
    }
    if (m_dropReason.is_open())
    {
        m_dropReason.flush();
        m_dropReason.close();
    }
    if (m_packetTrace.is_open())
    {
        m_packetTrace.flush();
        m_packetTrace.close();
    }
}

void
MtdTelemetryHelper::SetRootDir(const std::string& rootDir)
{
    m_rootDir = rootDir.empty() ? "." : rootDir;
    m_dirsReady = false;
}

std::string
MtdTelemetryHelper::GetRootDir() const
{
    return m_rootDir;
}

void
MtdTelemetryHelper::SetNetworkHelper(MtdNetworkHelper* networkHelper)
{
    m_networkHelper = networkHelper;
}

void
MtdTelemetryHelper::EnablePacketTrace(bool enable)
{
    m_packetTraceEnabled = enable;

    // If monitoring is already installed, ensure the trace file exists/open.
    if (m_packetTraceEnabled)
    {
        EnsureDirsAndFiles();
        WriteHeadersIfNeeded();
    }
}

bool
MtdTelemetryHelper::IsPacketTraceEnabled() const
{
    return m_packetTraceEnabled;
}

void
MtdTelemetryHelper::InstallMonitor(const NodeContainer& proxies)
{
    m_proxies = proxies;
    m_proxyNodeIds.clear();

    for (uint32_t i = 0; i < m_proxies.GetN(); ++i)
    {
        const uint32_t nodeId = m_proxies.Get(i)->GetId();
        m_proxyNodeIds.insert(nodeId);
    }

    EnsureDirsAndFiles();
    WriteHeadersIfNeeded();

    InstallFlowMonitorIfNeeded();
    BuildProxyIpMap();

    SetupHooks();

    // Start periodic write at 1 second.
    if (!m_writeEvent.IsExpired())
    {
        Simulator::Cancel(m_writeEvent);
    }
    m_lastWriteTime = Simulator::Now();
    m_writeEvent = Simulator::Schedule(Seconds(1.0), &MtdTelemetryHelper::WriteWindow, this);
}

void
MtdTelemetryHelper::EnablePcap(const std::string& prefix)
{
    if (!m_networkHelper)
    {
        NS_LOG_WARN("EnablePcap called but NetworkHelper not set");
        return;
    }

    EnsureDirsAndFiles();

    std::filesystem::path p(m_rootDir);
    p /= "traces";
    p /= prefix;

    m_networkHelper->EnablePcap(p.string());
}

void
MtdTelemetryHelper::EnsureDirsAndFiles()
{
    if (m_dirsReady)
    {
        return;
    }

    std::filesystem::path root(m_rootDir);
    std::filesystem::path telemetryDir = root / "telemetry";
    std::filesystem::path tracesDir = root / "traces";

    std::error_code ec;
    std::filesystem::create_directories(telemetryDir, ec);
    std::filesystem::create_directories(tracesDir, ec);

    // Open files (append mode).
    const std::filesystem::path trafficPath = telemetryDir / "traffic_samples.csv";
    const std::filesystem::path dropPath = telemetryDir / "drop_reason.csv";

    if (!m_trafficSamples.is_open())
    {
        m_trafficSamples.open(trafficPath.string(), std::ios::app);
    }
    if (!m_dropReason.is_open())
    {
        m_dropReason.open(dropPath.string(), std::ios::app);
    }

    if (m_packetTraceEnabled && !m_packetTrace.is_open())
    {
        const std::filesystem::path pktPath = tracesDir / "packet_level_debug.csv";
        m_packetTrace.open(pktPath.string(), std::ios::app);
    }

    m_dirsReady = true;
}

void
MtdTelemetryHelper::WriteHeadersIfNeeded()
{
    auto needsHeader = [](std::ofstream& file, const std::string& path) -> bool {
        if (!file.is_open())
        {
            return false;
        }
        std::error_code ec;
        if (!std::filesystem::exists(path, ec))
        {
            return true;
        }
        const auto sz = std::filesystem::file_size(path, ec);
        return (ec ? true : (sz == 0));
    };

    std::filesystem::path root(m_rootDir);
    const std::filesystem::path trafficPath = root / "telemetry" / "traffic_samples.csv";
    const std::filesystem::path dropPath = root / "telemetry" / "drop_reason.csv";

    if (needsHeader(m_trafficSamples, trafficPath.string()))
    {
        // Required columns: Time, NodeId, IngressMbps, DropTotal
        // Also include LatencyAvgMs, JitterAvgMs for RCA.
        m_trafficSamples << "Time,NodeId,IngressMbps,DropTotal,LatencyAvgMs,JitterAvgMs\n";
        m_trafficSamples.flush();
    }

    if (needsHeader(m_dropReason, dropPath.string()))
    {
        // Required columns: Time, NodeId, CongestionCount, NoRouteCount, MtdBanCount, MtdEpochErrCount
        // Also include TTL/Stale/SysOther.
        m_dropReason << "Time,NodeId,CongestionCount,NoRouteCount,MtdBanCount,MtdEpochErrCount,TtlCount,MtdStaleIpCount,SysOtherCount\n";
        m_dropReason.flush();
    }

    if (m_packetTraceEnabled && m_packetTrace.is_open())
    {
        std::filesystem::path pktPath = root / "traces" / "packet_level_debug.csv";
        std::error_code ec;
        const bool headerNeeded = !std::filesystem::exists(pktPath, ec) || std::filesystem::file_size(pktPath, ec) == 0;
        if (headerNeeded)
        {
            m_packetTrace << "Time,NodeId,Where,DropReason,Uid,Size,HasTag,EpochId,UserId\n";
            m_packetTrace.flush();
        }
    }
}

void
MtdTelemetryHelper::InstallFlowMonitorIfNeeded()
{
    if (m_flowMonitor)
    {
        return;
    }

    // Install probes on all nodes so flow stats are collected end-to-end.
    m_flowMonitor = m_flowHelper.InstallAll();
    m_flowClassifier = DynamicCast<Ipv4FlowClassifier>(m_flowHelper.GetClassifier());
}

void
MtdTelemetryHelper::BuildProxyIpMap()
{
    m_proxyIpToNodeId.clear();

    for (uint32_t i = 0; i < m_proxies.GetN(); ++i)
    {
        Ptr<Node> node = m_proxies.Get(i);
        const uint32_t nodeId = node->GetId();
        Ptr<Ipv4> ipv4 = node->GetObject<Ipv4>();
        if (!ipv4)
        {
            continue;
        }

        for (uint32_t ifIndex = 0; ifIndex < ipv4->GetNInterfaces(); ++ifIndex)
        {
            for (uint32_t a = 0; a < ipv4->GetNAddresses(ifIndex); ++a)
            {
                const Ipv4InterfaceAddress ifAddr = ipv4->GetAddress(ifIndex, a);
                const Ipv4Address addr = ifAddr.GetLocal();
                if (addr.IsAny() || addr == Ipv4Address::GetLoopback() || addr.IsBroadcast())
                {
                    continue;
                }
                m_proxyIpToNodeId[addr] = nodeId;
            }
        }
    }
}

void
MtdTelemetryHelper::SetupHooks()
{
    // Connect trace sources directly on the proxy nodes/devices/apps.
    // This avoids Config::Connect path resolution issues.

    for (uint32_t i = 0; i < m_proxies.GetN(); ++i)
    {
        Ptr<Node> node = m_proxies.Get(i);
        if (!node)
        {
            continue;
        }
        const uint32_t nodeId = node->GetId();

        const std::string nodeCtx = "/NodeList/" + std::to_string(nodeId) + "/";

        // Queue drops (congestion) - common in PointToPoint/Csma.
        for (uint32_t d = 0; d < node->GetNDevices(); ++d)
        {
            Ptr<NetDevice> nd = node->GetDevice(d);
            if (!nd)
            {
                continue;
            }

            if (auto p2p = DynamicCast<PointToPointNetDevice>(nd))
            {
                Ptr<Queue<Packet>> q = p2p->GetQueue();
                if (q)
                {
                    q->TraceConnect("Drop", nodeCtx, MakeCallback(&MtdTelemetryHelper::OnQueueDrop, this));
                }
            }
            else if (auto csma = DynamicCast<CsmaNetDevice>(nd))
            {
                Ptr<Queue<Packet>> q = csma->GetQueue();
                if (q)
                {
                    q->TraceConnect("Drop", nodeCtx, MakeCallback(&MtdTelemetryHelper::OnQueueDrop, this));
                }
            }
        }

        // IPv4 drops (TTL / no route / other).
        Ptr<Ipv4L3Protocol> ipv4L3 = node->GetObject<Ipv4L3Protocol>();
        if (ipv4L3)
        {
            ipv4L3->TraceConnect("Drop", nodeCtx, MakeCallback(&MtdTelemetryHelper::OnIpv4Drop, this));
        }

        // Custom app-level drop trace: MtdDrop(Ptr<const Packet>, DropReason).
        for (uint32_t a = 0; a < node->GetNApplications(); ++a)
        {
            Ptr<Application> app = node->GetApplication(a);
            if (!app)
            {
                continue;
            }

            app->TraceConnect("MtdDrop", nodeCtx, MakeCallback(&MtdTelemetryHelper::OnMtdDrop, this));
        }
    }
}

void
MtdTelemetryHelper::ScheduleNextWrite()
{
    m_writeEvent = Simulator::Schedule(Seconds(1.0), &MtdTelemetryHelper::WriteWindow, this);
}

std::map<uint32_t, MtdTelemetryHelper::FlowTotals>
MtdTelemetryHelper::ComputeFlowDeltas(double dtSeconds)
{
    std::map<uint32_t, FlowTotals> totals;

    if (!m_flowMonitor || !m_flowClassifier)
    {
        return totals;
    }

    m_flowMonitor->CheckForLostPackets();

    const auto stats = m_flowMonitor->GetFlowStats();
    for (const auto& [flowId, flowStats] : stats)
    {
        const Ipv4FlowClassifier::FiveTuple t = m_flowClassifier->FindFlow(flowId);
        const auto it = m_proxyIpToNodeId.find(t.destinationAddress);
        if (it == m_proxyIpToNodeId.end())
        {
            continue;
        }

        const uint32_t nodeId = it->second;
        auto& agg = totals[nodeId];
        agg.rxBytes += flowStats.rxBytes;
        agg.rxPackets += flowStats.rxPackets;
        agg.delaySum += flowStats.delaySum;
        agg.jitterSum += flowStats.jitterSum;
    }

    // Convert to deltas vs last snapshot.
    std::map<uint32_t, FlowTotals> deltas;
    for (const auto& [nodeId, absolute] : totals)
    {
        const auto prevIt = m_lastFlowTotals.find(nodeId);
        const FlowTotals prev = (prevIt != m_lastFlowTotals.end()) ? prevIt->second : FlowTotals{};

        FlowTotals d;
        d.rxBytes = (absolute.rxBytes >= prev.rxBytes) ? (absolute.rxBytes - prev.rxBytes) : 0;
        d.rxPackets = (absolute.rxPackets >= prev.rxPackets) ? (absolute.rxPackets - prev.rxPackets) : 0;
        d.delaySum = absolute.delaySum - prev.delaySum;
        d.jitterSum = absolute.jitterSum - prev.jitterSum;

        deltas[nodeId] = d;
        m_lastFlowTotals[nodeId] = absolute;
    }

    (void)dtSeconds;
    return deltas;
}

void
MtdTelemetryHelper::WriteWindow()
{
    EnsureDirsAndFiles();

    const Time now = Simulator::Now();
    const double dt = (m_lastWriteTime.IsZero()) ? 1.0 : (now - m_lastWriteTime).GetSeconds();
    m_lastWriteTime = now;

    const double tSec = now.GetSeconds();

    // Flow-based ingress/latency/jitter.
    const auto deltas = ComputeFlowDeltas(dt);

    m_trafficSamples << std::fixed << std::setprecision(3);
    m_dropReason << std::fixed << std::setprecision(3);

    for (uint32_t nodeId : m_proxyNodeIds)
    {
        const auto dropsIt = m_windowDrops.find(nodeId);
        const DropCounters drops = (dropsIt != m_windowDrops.end()) ? dropsIt->second : DropCounters{};

        auto flowIt = deltas.find(nodeId);
        const FlowTotals flow = (flowIt != deltas.end()) ? flowIt->second : FlowTotals{};

        const double ingressMbps = (dt > 0.0) ? (static_cast<double>(flow.rxBytes) * 8.0 / 1e6 / dt) : 0.0;

        double latencyAvgMs = 0.0;
        double jitterAvgMs = 0.0;
        if (flow.rxPackets > 0)
        {
            latencyAvgMs = (flow.delaySum.GetSeconds() / static_cast<double>(flow.rxPackets)) * 1000.0;
            jitterAvgMs = (flow.jitterSum.GetSeconds() / static_cast<double>(flow.rxPackets)) * 1000.0;
        }

        m_trafficSamples << tSec << "," << nodeId << "," << ingressMbps << "," << drops.Total() << ","
                        << latencyAvgMs << "," << jitterAvgMs << "\n";

        m_dropReason << tSec << "," << nodeId << "," << drops.congestion << "," << drops.noRoute << ","
                    << drops.mtdAccessDenied << "," << drops.mtdEpochMismatch << "," << drops.ttl << ","
                    << drops.mtdStaleDest << "," << drops.sysOther << "\n";
    }

    m_trafficSamples.flush();
    m_dropReason.flush();

    // Reset window counters.
    m_windowDrops.clear();

    ScheduleNextWrite();
}

void
MtdTelemetryHelper::OnQueueDrop(std::string context, Ptr<const Packet> packet)
{
    uint32_t nodeId = 0;
    if (!TryParseNodeId(context, &nodeId))
    {
        return;
    }

    if (m_proxyNodeIds.find(nodeId) == m_proxyNodeIds.end())
    {
        return;
    }

    CountDrop(nodeId, DropReason::DROP_PHY_CONGESTION);
    MaybeWritePacketTrace(nodeId, "TxQueue/Drop", DropReason::DROP_PHY_CONGESTION, packet);
}

void
MtdTelemetryHelper::OnIpv4Drop(std::string context,
                              const Ipv4Header&,
                              Ptr<const Packet> packet,
                              Ipv4L3Protocol::DropReason reason,
                              Ptr<Ipv4>,
                              uint32_t)
{
    uint32_t nodeId = 0;
    if (!TryParseNodeId(context, &nodeId))
    {
        return;
    }

    if (m_proxyNodeIds.find(nodeId) == m_proxyNodeIds.end())
    {
        return;
    }

    // Best-effort mapping from ns-3 IPv4 drop reason to our RCA contract.
    DropReason mapped = DropReason::DROP_SYS_OTHER;
    switch (reason)
    {
    case Ipv4L3Protocol::DROP_NO_ROUTE:
        mapped = DropReason::DROP_PHY_NO_ROUTE;
        break;
    case Ipv4L3Protocol::DROP_TTL_EXPIRED:
        mapped = DropReason::DROP_PHY_TTL;
        break;
    default:
        mapped = DropReason::DROP_SYS_OTHER;
        break;
    }

    CountDrop(nodeId, mapped);
    MaybeWritePacketTrace(nodeId, "Ipv4L3Protocol/Drop", mapped, packet);
}

void
MtdTelemetryHelper::OnMtdDrop(std::string context, Ptr<const Packet> packet, DropReason reason)
{
    uint32_t nodeId = 0;
    if (!TryParseNodeId(context, &nodeId))
    {
        return;
    }

    if (m_proxyNodeIds.find(nodeId) == m_proxyNodeIds.end())
    {
        return;
    }

    CountDrop(nodeId, reason);
    MaybeWritePacketTrace(nodeId, "MtdDrop", reason, packet);
}

bool
MtdTelemetryHelper::TryParseNodeId(const std::string& context, uint32_t* outNodeId)
{
    // Example context: "/NodeList/12/DeviceList/0/..."
    const std::string needle = "/NodeList/";
    const auto pos = context.find(needle);
    if (pos == std::string::npos)
    {
        return false;
    }

    const auto start = pos + needle.size();
    const auto end = context.find('/', start);
    if (end == std::string::npos)
    {
        return false;
    }

    const std::string idStr = context.substr(start, end - start);
    try
    {
        *outNodeId = static_cast<uint32_t>(std::stoul(idStr));
        return true;
    }
    catch (...)
    {
        return false;
    }
}

void
MtdTelemetryHelper::CountDrop(uint32_t nodeId, DropReason reason)
{
    auto& c = m_windowDrops[nodeId];
    switch (reason)
    {
    case DropReason::DROP_PHY_CONGESTION:
        c.congestion++;
        break;
    case DropReason::DROP_PHY_TTL:
        c.ttl++;
        break;
    case DropReason::DROP_PHY_NO_ROUTE:
        c.noRoute++;
        break;
    case DropReason::DROP_MTD_ACCESS_DENIED:
        c.mtdAccessDenied++;
        break;
    case DropReason::DROP_MTD_STALE_DEST:
        c.mtdStaleDest++;
        break;
    case DropReason::DROP_MTD_EPOCH_MISMATCH:
        c.mtdEpochMismatch++;
        break;
    default:
        c.sysOther++;
        break;
    }
}

void
MtdTelemetryHelper::MaybeWritePacketTrace(uint32_t nodeId,
                                         const char* where,
                                         DropReason reason,
                                         Ptr<const Packet> packet)
{
    if (!m_packetTraceEnabled || !m_packetTrace.is_open() || !packet)
    {
        return;
    }

    MtdSecurityTag tag;
    bool hasTag = false;

    // PeekPacketTag is not const on all ns-3 versions; use a copy.
    Ptr<Packet> p = packet->Copy();
    hasTag = p->PeekPacketTag(tag);

    const double tSec = Simulator::Now().GetSeconds();
    m_packetTrace << std::fixed << std::setprecision(3);
    m_packetTrace << tSec << "," << nodeId << "," << where << "," << static_cast<int>(reason) << ","
                  << p->GetUid() << "," << p->GetSize() << "," << (hasTag ? "1" : "0") << ","
                  << (hasTag ? tag.GetEpochId() : 0) << "," << (hasTag ? tag.GetUserId() : 0) << "\n";
}

} // namespace mtd
} // namespace ns3
