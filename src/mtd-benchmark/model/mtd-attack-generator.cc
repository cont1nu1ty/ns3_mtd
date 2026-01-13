/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
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
    : m_isActive(false),
      m_totalPacketsSent(0),
      m_totalBytesSent(0)
{
    NS_LOG_FUNCTION(this);
}

AttackGenerator::~AttackGenerator()
{
    NS_LOG_FUNCTION(this);
    if (m_isActive) {
        Stop();
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
    // 如果正在攻击，先停止再应用新配置
    if (m_isActive) {
        Stop();
    }
    m_params = params;
}

bool
AttackGenerator::Start()
{
    NS_LOG_FUNCTION(this);

    // 1. 完整性检查
    if (m_isActive) {
        NS_LOG_WARN("Attack already active, ignoring Start()");
        return false;
    }
    if (!m_trafficHelper || !m_networkHelper) {
        NS_LOG_ERROR("Missing dependencies (TrafficHelper or NetworkHelper)");
        return false;
    }

    // 2. 获取攻击资源 (Attacker Nodes)
    NodeContainer attackers = m_networkHelper->GetAttackerNodes(); //
    if (attackers.GetN() == 0) {
        NS_LOG_WARN("No attacker nodes available in topology");
        return false;
    }

    // 3. 准备攻击参数
    DataRate rate(std::to_string(m_params.rate) + "pps"); // 假设 params.rate 是包率
    MtdTrafficHelper::StatelessTransport transport = GetTransportProfile();
    uint32_t targetProxy = m_params.targetProxyId;

    Time startTime = Simulator::Now();
    m_attackStartTime = startTime;

    AttackRecord record;
    record.attackId = m_attackHistory.size() + 1;
    record.startTime = startTime.GetMilliSeconds();
    record.endTime = 0;
    record.type = m_params.type;
    record.targetProxyId = targetProxy;
    record.ratePps = m_params.rate;
    record.packetSize = m_params.packetSize;
    record.attackerCount = attackers.GetN();
    record.durationPlanned = m_params.duration;
    record.durationActual = 0.0;
    record.defenseTriggered = false;
    m_attackHistory.push_back(record);

    NS_LOG_INFO("Starting attack: Type=" << (int)m_params.type 
                << " Target=Proxy" << targetProxy 
                << " Rate=" << m_params.rate);

    // 4. [关键] 调用 TrafficHelper 执行攻击
    // Primitive 1: 无状态高压流量
    for (uint32_t i = 0; i < attackers.GetN(); ++i) {
        Ptr<Node> attackerNode = attackers.Get(i);
        
        auto flowHandle = m_trafficHelper->CreateStatelessHighRateFlow(
            attackerNode,
            targetProxy,
            rate,
            transport
        );

        if (flowHandle > 0) {
            m_activeFlows.push_back(flowHandle);
        }
    }

    m_isActive = true;

    // 5. 调度自动停止 (如果配置了 duration)
    if (m_params.duration > 0) {
        m_stopEvent = Simulator::Schedule(Seconds(m_params.duration), 
                                          &AttackGenerator::Stop, this);
    }

    // 6. 发布事件
    NotifyAttackEvent(EventType::ATTACK_STARTED);

    return true;
}

void
AttackGenerator::Stop()
{
    NS_LOG_FUNCTION(this);

    if (!m_isActive) return;

    if (!m_trafficHelper) return;

    Time stopTime = Simulator::Now();
    double durationSeconds = (stopTime - m_attackStartTime).GetSeconds();
    if (durationSeconds < 0.0) {
        durationSeconds = 0.0;
    }
    uint64_t packets = static_cast<uint64_t>(m_params.rate * durationSeconds);
    uint64_t bytes = packets * static_cast<uint64_t>(m_params.packetSize);
    m_totalPacketsSent += packets;
    m_totalBytesSent += bytes;
    if (!m_attackHistory.empty()) {
        m_attackHistory.back().endTime = stopTime.GetMilliSeconds();
        m_attackHistory.back().durationActual = durationSeconds;
    }

    // 1. 销毁所有攻击流
    // 调用 TerminateFlow
    for (auto handle : m_activeFlows) {
        m_trafficHelper->TerminateFlow(handle);
    }
    m_activeFlows.clear();

    // 2. 取消自动停止定时器
    if (m_stopEvent.IsPending()) {
        Simulator::Cancel(m_stopEvent);
    }

    m_isActive = false;
    NS_LOG_INFO("Attack stopped");

    // 3. 发布事件
    NotifyAttackEvent(EventType::ATTACK_STOPPED);
}

bool
AttackGenerator::IsActive() const
{
    return m_isActive;
}

MtdTrafficHelper::StatelessTransport
AttackGenerator::GetTransportProfile() const
{
    // 将 mtd-common.h 中的 AttackType 映射到 TrafficHelper 的传输层配置
    switch (m_params.type) {
        case AttackType::SYN_FLOOD:
            return MtdTrafficHelper::STATELESS_TCP_SYN;
        case AttackType::UDP_FLOOD:
        case AttackType::DOS: // 默认为 UDP
        default:
            return MtdTrafficHelper::STATELESS_UDP;
    }
}

std::map<std::string, double>
AttackGenerator::GetStatistics() const
{
    double packets = static_cast<double>(m_totalPacketsSent);
    double bytes = static_cast<double>(m_totalBytesSent);

    if (m_isActive) {
        Time now = Simulator::Now();
        double liveDuration = (now - m_attackStartTime).GetSeconds();
        if (liveDuration < 0.0) {
            liveDuration = 0.0;
        }
        double livePackets = m_params.rate * liveDuration;
        packets += livePackets;
        bytes += livePackets * static_cast<double>(m_params.packetSize);
    }

    return std::map<std::string, double>{
        {"packetCount", packets},
        {"byteCount", bytes}
    };
}

const AttackGenerator::AttackHistory&
AttackGenerator::GetAttackHistory() const
{
    return m_attackHistory;
}

void
AttackGenerator::NotifyAttackEvent(EventType type, const std::string& reason)
{
    if (!m_eventBus) return;

    MtdEvent event(type, Simulator::Now().GetMilliSeconds());
    
    // 填充元数据供数据导出使用
    event.metadata["attackType"] = std::to_string(static_cast<int>(m_params.type));
    event.metadata["targetProxy"] = std::to_string(m_params.targetProxyId);
    event.metadata["rate"] = std::to_string(m_params.rate);
    event.metadata["attackerCount"] = std::to_string(m_activeFlows.size());
    
    if (!reason.empty()) {
        event.metadata["reason"] = reason;
    }

    m_eventBus->Publish(event); //
}

} // namespace mtd
} // namespace ns3