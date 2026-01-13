/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Analysis Helper
 *
 * Provides two modes:
 * - Lightweight: read PacketSink counters (bytes) + Rx trace (packets)
 * - Heavyweight: enable FlowMonitor and aggregate per-proxy traffic stats
 *
 * This helper is a tool; it does not embed detectors. A caller can optionally
 * pull stats and feed them into LocalDetector from the main script.
 */

#ifndef MTD_ANALYSIS_HELPER_H
#define MTD_ANALYSIS_HELPER_H

#include "ns3/mtd-common.h"
#include "ns3/mtd-network-helper.h"

#include "ns3/node-container.h"
#include "ns3/object.h"
#include "ns3/packet-sink.h"
#include "ns3/ptr.h"
#include "ns3/nstime.h"

#include <map>
#include <memory>
#include <string>

namespace ns3 {
class FlowMonitor;
namespace mtd {

class MtdAnalysisHelper : public Object
{
  public:
    enum class Mode
    {
        LIGHTWEIGHT_SINK,
        HEAVYWEIGHT_FLOWMONITOR
    };

    struct Config
    {
        Mode mode{Mode::LIGHTWEIGHT_SINK};
        Time sampleInterval{Seconds(1.0)};

        // Only used in heavyweight mode.
        bool serializeXml{false};
        std::string xmlFileName{"flowmon.xml"};
        bool xmlIncludeProbes{true};
        bool xmlIncludeFlows{true};
    };

    static TypeId GetTypeId();

    MtdAnalysisHelper();
    ~MtdAnalysisHelper() override;

    void SetConfig(const Config& cfg);
    Config GetConfig() const;

    void SetNetworkHelper(const MtdNetworkHelper* networkHelper);

    // Lightweight mode input: attach PacketSinks installed elsewhere.
    void AttachProxySinks(const std::map<uint32_t, Ptr<PacketSink>>& proxySinks);

    // Heavyweight mode input: install FlowMonitor on the provided nodes.
    void InstallFlowMonitor(const NodeContainer& nodes);

    // Collect and aggregate stats for the last interval.
    void CollectStats();

    // Get last collected stats.
    // If perProxy==true: returns proxyId -> stats.
    // If perProxy==false: returns a single entry keyed by 0 (aggregate across proxies).
    std::map<uint32_t, TrafficStats> GetStats(bool perProxy) const;

    TrafficStats GetTrafficStats(uint32_t proxyId) const;

    Ptr<::ns3::FlowMonitor> GetFlowMonitor() const;

    void SerializeFlowMonitorIfEnabled() const;

    // Internal hook for PacketSink Rx trace.
    void OnSinkRx(uint32_t proxyId, Ptr<const Packet> packet, const Address& from);

  private:
    struct FlowImpl;

    Config m_cfg;
    const MtdNetworkHelper* m_networkHelper{nullptr};

    std::map<uint32_t, Ptr<PacketSink>> m_proxySinks;
    std::map<uint32_t, uint64_t> m_sinkPackets;

    std::map<uint32_t, uint64_t> m_lastBytesIn;
    std::map<uint32_t, uint64_t> m_lastPacketsIn;
    Time m_lastCollectTime{Seconds(0.0)};

    std::map<uint32_t, TrafficStats> m_lastStats; // proxyId -> stats

    std::unique_ptr<FlowImpl> m_flow;
};

} // namespace mtd
} // namespace ns3

#endif // MTD_ANALYSIS_HELPER_H
