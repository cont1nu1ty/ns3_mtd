/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MTD_ATTACK_GENERATOR_H
#define MTD_ATTACK_GENERATOR_H

#include "ns3/mtd-common.h"
#include "ns3/mtd-traffic-helper.h"
#include "ns3/mtd-network-helper.h"
#include "ns3/mtd-event-bus.h"

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"

#include <cstdint>
#include <map>
#include <vector>

namespace ns3 {
namespace mtd {

/**
 * \ingroup mtd
 * \brief 攻击流量生成器 (Attack Logic Module)
 *
 * 职责：
 * 1. 管理僵尸网络节点 (Attacker Nodes)
 * 2. 调度攻击的开启与停止
 * 3. 将高层攻击意图 (AttackType) 转化为底层流量 (TrafficHelper Primitive)
 * 4. 发布安全事件 (ATTACK_STARTED/STOPPED)
 */
class AttackGenerator : public Object
{
public:
    static TypeId GetTypeId();
    
    AttackGenerator();
    ~AttackGenerator() override;

    struct AttackRecord {
    // === 基础信息 ===
    uint64_t attackId;        // [新增] 唯一标识符，用于关联 START/STOP 事件
    uint64_t startTime;       // 改名：timestamp -> startTime 更明确
    uint64_t endTime;         // [新增] 实际结束时间 (Stop 时填充)
    
    // === 攻击配置 ===
    AttackType type;
    uint32_t targetProxyId;
    double ratePps;           // 改名：rate -> ratePps (Packet Per Second)，避免歧义
    uint32_t packetSize;      // [新增] 极其重要！只有 PPS 无法计算带宽 (Mbps)
    uint32_t attackerCount;   // [新增] 多少个僵尸节点参与了攻击？

    // === 统计信息 (可选，区分“计划”与“实际”) ===
    double durationPlanned;   // params.duration
    double durationActual;    // 实际运行时长 (可能被手动 Stop 截断)
    
    // === 交互状态 ===
    bool defenseTriggered;    // 是否触发了防御（需要 ScoreManager 反馈，目前代码里是写死的 false）
};
    using AttackHistory = std::vector<AttackRecord>;

    /**
     * \brief 注入依赖
     */
    void SetTrafficHelper(Ptr<MtdTrafficHelper> trafficHelper);
    void SetNetworkHelper(Ptr<MtdNetworkHelper> networkHelper);
    void SetEventBus(Ptr<EventBus> eventBus);

    /**
     * \brief 配置攻击参数
     * \param params 包含攻击类型、速率、目标等信息
     */
    void Configure(const AttackParams& params);

    /**
     * \brief 立即开始攻击
     * \return true 如果成功启动
     */
    bool Start();

    /**
     * \brief 停止当前攻击
     */
    void Stop();

    /**
     * \brief 获取当前攻击状态
     */
    bool IsActive() const;

    /**
     * \brief 获取攻击统计信息
     */
    std::map<std::string, double> GetStatistics() const;

    /**
     * \brief 获取攻击历史记录
     */
    const AttackHistory& GetAttackHistory() const;

private:
    // 依赖组件
    Ptr<MtdTrafficHelper> m_trafficHelper;
    Ptr<MtdNetworkHelper> m_networkHelper;
    Ptr<EventBus> m_eventBus;

    // 配置与状态
    AttackParams m_params;
    bool m_isActive;
    Time m_attackStartTime;
    uint64_t m_totalPacketsSent;
    uint64_t m_totalBytesSent;
    
    // 追踪当前攻击产生的所有底层流句柄，以便停止时销毁
    std::vector<MtdTrafficHelper::FlowHandle> m_activeFlows;
    
    // 自动停止的定时器 (如果设置了 duration)
    EventId m_stopEvent;

    AttackHistory m_attackHistory;

    // 内部辅助：将 AttackType 映射为 TrafficHelper 的传输层配置
    MtdTrafficHelper::StatelessTransport GetTransportProfile() const;
    
    void NotifyAttackEvent(EventType type, const std::string& reason = "");
};

} // namespace mtd
} // namespace ns3

#endif // MTD_ATTACK_GENERATOR_H