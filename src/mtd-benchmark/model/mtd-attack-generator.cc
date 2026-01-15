/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Attack Generator Implementation
 * 
 * Publishes Ground Truth events with complete AttackRecord data
 * for accurate logging and post-analysis.
 */

#include "mtd-attack-generator.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdAttackGenerator");
NS_OBJECT_ENSURE_REGISTERED(AttackGenerator);

TypeId
AttackGenerator::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::AttackGenerator")
        .SetParent<Object>()
        .SetGroupName("MTD")
        .AddConstructor<AttackGenerator>();
    return tid;
}

AttackGenerator::AttackGenerator()
{
    NS_LOG_FUNCTION(this);
}

AttackGenerator::~AttackGenerator()
{
    NS_LOG_FUNCTION(this);
    if (m_isActive)
    {
        Stop("destructor");
    }
}

void
AttackGenerator::SetTrafficHelper(Ptr<MtdTrafficHelper> trafficHelper)
{
    m_trafficHelper = trafficHelper;
}

void
AttackGenerator::SetNetworkHelper(Ptr<MtdNetworkHelper> networkHelper)
{
    m_networkHelper = networkHelper;
}

void
AttackGenerator::SetEventBus(Ptr<EventBus> eventBus)
{
    m_eventBus = eventBus;
}

void
AttackGenerator::Configure(const AttackParams& params)
{
    NS_LOG_FUNCTION(this);
    
    // If currently attacking, stop before applying new config
    if (m_isActive)
    {
        Stop("reconfigure");
    }
    m_params = params;
}

bool
AttackGenerator::Start()
{
    NS_LOG_FUNCTION(this);

    // 1. Validation
    if (m_isActive)
    {
        NS_LOG_WARN("Attack already active, ignoring Start()");
        return false;
    }
    
    if (!m_trafficHelper || !m_networkHelper)
    {
        NS_LOG_WARN("Missing dependencies (TrafficHelper or NetworkHelper) - running in simulation-only mode");
        // Allow starting without traffic helper for demonstration/testing
    }

    // 2. Get attacker resources
    uint32_t attackerCount = 0;
    NodeContainer attackers;
    if (m_networkHelper)
    {
        attackers = m_networkHelper->GetAttackerNodes();
        attackerCount = attackers.GetN();
    }

    // 3. Capture Ground Truth - START
    Time startTime = Simulator::Now();
    m_attackStartTime = startTime;

    m_currentRecord = AttackRecord();  // Reset
    m_currentRecord.attackId = m_nextAttackId++;
    m_currentRecord.startTime = startTime.GetMilliSeconds();
    m_currentRecord.endTime = 0;
    m_currentRecord.type = m_params.type;
    m_currentRecord.targetProxyId = m_params.targetProxyId;
    m_currentRecord.targetProxyIds = m_params.targetProxyIds;
    m_currentRecord.ratePps = m_params.rate;
    m_currentRecord.packetSize = m_params.packetSize;
    m_currentRecord.attackerCount = attackerCount > 0 ? attackerCount : 1;
    m_currentRecord.durationPlanned = m_params.duration;
    m_currentRecord.durationActual = 0.0;
    m_currentRecord.packetsSent = 0;
    m_currentRecord.bytesSent = 0;
    m_currentRecord.defenseTriggered = false;
    m_currentRecord.stopReason = "";

    NS_LOG_INFO("Starting attack: ID=" << m_currentRecord.attackId
                << " Type=" << AttackTypeToString(m_params.type)
                << " Target=Proxy" << m_params.targetProxyId
                << " Rate=" << m_params.rate << "pps"
                << " Bandwidth=" << m_currentRecord.GetBandwidthMbps() << "Mbps");

    // 4. Create traffic flows (if traffic helper available)
    if (m_trafficHelper && m_networkHelper && attackerCount > 0)
    {
        DataRate rate(std::to_string(m_params.rate) + "pps");
        MtdTrafficHelper::StatelessTransport transport = GetTransportProfile();
        
        for (uint32_t i = 0; i < attackerCount; ++i)
        {
        Ptr<Node> attackerNode = attackers.Get(i);
        
        auto flowHandle = m_trafficHelper->CreateStatelessHighRateFlow(
            attackerNode,
                m_params.targetProxyId,
            rate,
            transport
        );

            if (flowHandle > 0)
            {
            m_activeFlows.push_back(flowHandle);
            }
        }
    }

    m_isActive = true;

    // 5. Schedule auto-stop (if duration configured)
    if (m_params.duration > 0)
    {
        m_stopEvent = Simulator::Schedule(Seconds(m_params.duration), 
                                          &AttackGenerator::Stop, this, "duration");
    }

    // 6. Publish ATTACK_STARTED with Ground Truth
    PublishGroundTruthEvent(EventType::ATTACK_STARTED);

    return true;
}

void
AttackGenerator::Stop(const std::string& reason)
{
    NS_LOG_FUNCTION(this << reason);

    if (!m_isActive)
    {
        return;
    }

    // 1. Calculate actual duration and statistics
    Time stopTime = Simulator::Now();
    double durationSeconds = (stopTime - m_attackStartTime).GetSeconds();
    if (durationSeconds < 0.0)
    {
        durationSeconds = 0.0;
    }
    
    uint64_t packets = static_cast<uint64_t>(m_params.rate * durationSeconds);
    uint64_t bytes = packets * static_cast<uint64_t>(m_params.packetSize);
    
    // 2. Update Ground Truth - END
    m_currentRecord.endTime = stopTime.GetMilliSeconds();
    m_currentRecord.durationActual = durationSeconds;
    m_currentRecord.packetsSent = packets;
    m_currentRecord.bytesSent = bytes;
    m_currentRecord.stopReason = reason;
    
    // Update totals
    m_totalPacketsSent += packets;
    m_totalBytesSent += bytes;
    
    // 3. Store in history
    m_attackHistory.push_back(m_currentRecord);

    NS_LOG_INFO("Attack stopped: ID=" << m_currentRecord.attackId
                << " Duration=" << durationSeconds << "s"
                << " Packets=" << packets
                << " Bytes=" << bytes
                << " Reason=" << reason);

    // 4. Terminate traffic flows
    if (m_trafficHelper)
    {
        for (auto handle : m_activeFlows)
        {
        m_trafficHelper->TerminateFlow(handle);
        }
    }
    m_activeFlows.clear();

    // 5. Cancel auto-stop timer if pending
    if (m_stopEvent.IsPending())
    {
        Simulator::Cancel(m_stopEvent);
    }

    m_isActive = false;

    // 6. Publish ATTACK_STOPPED with Ground Truth
    PublishGroundTruthEvent(EventType::ATTACK_STOPPED);
}

bool
AttackGenerator::IsActive() const
{
    return m_isActive;
}

void
AttackGenerator::MarkDefenseTriggered()
{
    NS_LOG_FUNCTION(this);
    if (m_isActive)
    {
        m_currentRecord.defenseTriggered = true;
    }
}

const AttackGenerator::AttackRecord&
AttackGenerator::GetCurrentRecord() const
{
    return m_currentRecord;
}

MtdTrafficHelper::StatelessTransport
AttackGenerator::GetTransportProfile() const
{
    switch (m_params.type)
    {
        case AttackType::SYN_FLOOD:
            return MtdTrafficHelper::STATELESS_TCP_SYN;
        case AttackType::UDP_FLOOD:
        case AttackType::DOS:
        default:
            return MtdTrafficHelper::STATELESS_UDP;
    }
}

std::map<std::string, double>
AttackGenerator::GetStatistics() const
{
    double packets = static_cast<double>(m_totalPacketsSent);
    double bytes = static_cast<double>(m_totalBytesSent);

    if (m_isActive)
    {
        Time now = Simulator::Now();
        double liveDuration = (now - m_attackStartTime).GetSeconds();
        if (liveDuration < 0.0)
        {
            liveDuration = 0.0;
        }
        double livePackets = m_params.rate * liveDuration;
        packets += livePackets;
        bytes += livePackets * static_cast<double>(m_params.packetSize);
    }

    return std::map<std::string, double>{
        {"packetCount", packets},
        {"byteCount", bytes},
        {"attackCount", static_cast<double>(m_attackHistory.size())}
    };
}

const AttackGenerator::AttackHistory&
AttackGenerator::GetAttackHistory() const
{
    return m_attackHistory;
}

void
AttackGenerator::PublishGroundTruthEvent(EventType type)
{
    if (!m_eventBus)
    {
        return;
    }

    MtdEvent event(type, Simulator::Now().GetMilliSeconds());
    event.sourceNodeId = m_currentRecord.targetProxyId;
    
    // Only include the full Ground Truth JSON - no redundant individual fields
    // The JSON contains all structured data for Python analysis
    event.metadata["groundTruth"] = m_currentRecord.ToJson();

    m_eventBus->Publish(event);
}

} // namespace mtd
} // namespace ns3
