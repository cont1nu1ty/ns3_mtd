/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Unified Telemetry Helper
 */

#ifndef MTD_TELEMETRY_HELPER_H
#define MTD_TELEMETRY_HELPER_H

#include "ns3/mtd-common.h"
#include "ns3/mtd-network-helper.h"
#include "ns3/mtd-security-tag.h"

#include "ns3/event-id.h"
#include "ns3/flow-monitor-helper.h"
#include "ns3/ipv4-address.h"
#include "ns3/ipv4-l3-protocol.h"
#include "ns3/node-container.h"
#include "ns3/object.h"
#include "ns3/packet.h"
#include "ns3/ptr.h"
#include "ns3/nstime.h"

#include <cstdint>
#include <fstream>
#include <map>
#include <set>
#include <string>

namespace ns3 {
class FlowMonitor;
class Ipv4;
class Ipv4FlowClassifier;
class Ipv4Header;
namespace mtd {

/**
 * \brief Centralized RCA telemetry writer (1s aggregation by default).
 *
 * Outputs under rootDir:
 * - telemetry/traffic_samples.csv
 * - telemetry/drop_reason.csv
 * - traces/ (pcap, xml, packet debug)
 */
class MtdTelemetryHelper : public Object
{
  public:
    static TypeId GetTypeId();

    MtdTelemetryHelper();
    ~MtdTelemetryHelper() override;

    void SetRootDir(const std::string& rootDir);
    std::string GetRootDir() const;

    void SetNetworkHelper(MtdNetworkHelper* networkHelper);

    void EnablePacketTrace(bool enable);
    bool IsPacketTraceEnabled() const;

    // Install monitoring on the given proxy nodes.
    void InstallMonitor(const NodeContainer& proxies);

    // Wrapper to enable pcap under ${rootDir}/traces/.
    void EnablePcap(const std::string& prefix);

  private:
    struct DropCounters
    {
        uint64_t congestion{0};
        uint64_t ttl{0};
        uint64_t noRoute{0};
        uint64_t mtdAccessDenied{0};
        uint64_t mtdStaleDest{0};
        uint64_t mtdEpochMismatch{0};
        uint64_t sysOther{0};

        uint64_t Total() const
        {
            return congestion + ttl + noRoute + mtdAccessDenied + mtdStaleDest + mtdEpochMismatch + sysOther;
        }
    };

    struct FlowTotals
    {
        uint64_t rxBytes{0};
        uint64_t rxPackets{0};
        Time delaySum{Seconds(0.0)};
        Time jitterSum{Seconds(0.0)};
    };

    void EnsureDirsAndFiles();
    void WriteHeadersIfNeeded();

    void SetupHooks();

    void ScheduleNextWrite();
    void WriteWindow();

    // Trace callbacks
    void OnQueueDrop(std::string context, Ptr<const Packet> packet);

    void OnIpv4Drop(std::string context,
                    const Ipv4Header& header,
                    Ptr<const Packet> packet,
            Ipv4L3Protocol::DropReason reason,
            Ptr<Ipv4> ipv4,
            uint32_t interface);

    void OnMtdDrop(std::string context, Ptr<const Packet> packet, DropReason reason);

    void CountDrop(uint32_t nodeId, DropReason reason);

    void MaybeWritePacketTrace(uint32_t nodeId, const char* where, DropReason reason, Ptr<const Packet> packet);

    static bool TryParseNodeId(const std::string& context, uint32_t* outNodeId);

    // FlowMonitor aggregation
    void InstallFlowMonitorIfNeeded();
    void BuildProxyIpMap();
    std::map<uint32_t, FlowTotals> ComputeFlowDeltas(double dtSeconds);

  private:
    std::string m_rootDir{"."};
    MtdNetworkHelper* m_networkHelper{nullptr};

    bool m_dirsReady{false};

    bool m_packetTraceEnabled{false};

    std::ofstream m_trafficSamples;
    std::ofstream m_dropReason;
    std::ofstream m_packetTrace;

    NodeContainer m_proxies;
    std::set<uint32_t> m_proxyNodeIds;
    std::map<Ipv4Address, uint32_t> m_proxyIpToNodeId;

    std::map<uint32_t, DropCounters> m_windowDrops;

    FlowMonitorHelper m_flowHelper;
    Ptr<::ns3::FlowMonitor> m_flowMonitor;
    Ptr<Ipv4FlowClassifier> m_flowClassifier;
    std::map<uint32_t, FlowTotals> m_lastFlowTotals; // nodeId -> totals

    EventId m_writeEvent;
    Time m_lastWriteTime{Seconds(0.0)};
};

} // namespace mtd
} // namespace ns3

#endif // MTD_TELEMETRY_HELPER_H
