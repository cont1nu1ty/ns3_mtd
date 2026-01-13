/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MTD_TRAFFIC_HELPER_H
#define MTD_TRAFFIC_HELPER_H

#include "ns3/object.h"
#include "ns3/ptr.h"
#include "ns3/ipv4-address.h"
#include "ns3/node-container.h"
#include "ns3/application-container.h"
#include "ns3/nstime.h"
#include "ns3/data-rate.h"

#include <map>
#include <vector>
#include <cstdint>

namespace ns3 {
namespace mtd {

// 前置声明：依赖 NetworkHelper 获取 IP 映射
class MtdNetworkHelper;

/**
 * \ingroup mtd
 * \brief 流量与连接实体管理器 (Traffic & Connection Entity Manager)
 *
 * 职责边界：
 * 1. 生成、维持、销毁基于 Socket 的逻辑流 (Flow)。
 * 2. 维护 UserId <-> Flow 的映射关系。
 * 3. 执行“指定源 -> 指定 Proxy”的流量注入。
 *
 * 设计原则：
 * 本 Helper 仅负责流量的物理形态与生命周期管理，不包含任何安全策略、
 * 攻击判定或业务语义。
 */
class MtdTrafficHelper : public Object
{
public:
    static TypeId GetTypeId (void);
    MtdTrafficHelper ();
    virtual ~MtdTrafficHelper ();

    /**
     * \brief 全局初始化，注入网络拓扑信息
     * 用于将 ProxyId 解析为具体的 Ipv4Address
     */
    void SetNetworkContext (Ptr<MtdNetworkHelper> netHelper);

    // =================================================================
    // 基础类型定义
    // =================================================================
    
    using FlowHandle = uint32_t;  ///< 流量实体的唯一标识符
    using UserId = uint32_t;      ///< 用户的逻辑身份
    using ProxyId = uint32_t;     ///< 目标代理节点的逻辑 ID
    
    /// \brief 标识无归属流量（未绑定特定 User 身份）
    static constexpr UserId USER_ID_NONE = 0;

    // =================================================================
    // 传输层协议特征 (中性描述)
    // =================================================================
    enum StatelessTransport {
    STATELESS_TCP_SYN,
    STATELESS_UDP
    };

    enum TransportProfile {
        TRANSPORT_TCP_FULL        ///< 完整 TCP 连接（完成三次握手）
    };

    // =================================================================
    // Primitive 1：无状态高压流量 (Stateless High-Rate Flow)  理论可以用在udp的其余通信
    // 对应原：Proxy 定向的高压无差别攻击 (Volumetric Attack)
    // =================================================================

    /**
     * \brief 创建无状态高速率流量
     *
     * 特征：
     * - 不绑定 UserId (使用 USER_ID_NONE)
     * - 不维护完整连接状态
     * - 明确指向某个 Proxy
     * * \param sourceNode 流量发起节点
     * \param targetProxy 目标 Proxy ID
     * \param rate 发包速率 (DataRate)
     * \param profile 传输层行为 (SYN_ONLY 或 UDP)
     * \return FlowHandle 流量句柄
     */
    FlowHandle CreateStatelessHighRateFlow (Ptr<Node> sourceNode, 
                                            ProxyId targetProxy, 
                                            DataRate rate, 
                                            StatelessTransport profile);

    // =================================================================
    // Primitive 2：长生命周期 TCP 连接集合 (Long-Lived Connections)
    // 对应原：连接占位型压力流量 (Connection Stress)
    // =================================================================

    /**
     * \brief 创建一组长期存活的 TCP 连接
     *
     * 特征：
     * - 建立完整的 TCP 三次握手
     * - 占用目标节点的 Socket 描述符资源
     * - 发送极少数据或不发送数据，仅维持连接
     *
     * \param sourceNode 流量发起节点
     * \param targetProxy 目标 Proxy ID
     * \param concurrency 并发连接数
     * \param connectionLifetime 单个连接的维持时间
     * \return std::vector<FlowHandle> 创建的所有连接句柄集合
     */
    std::vector<FlowHandle> CreateLongLivedTcpConnections (Ptr<Node> sourceNode,
                                                           ProxyId targetProxy,
                                                           uint32_t concurrency,
                                                           Time connectionLifetime);

    // =================================================================
    // Primitive 3：周期性 TCP 业务流 (Periodic TCP Flow)
    // 对应原：正常用户的低速业务流 (Benign User)
    // =================================================================

    /**
     * \brief 创建周期性 TCP 业务流
     *
     * 特征：
     * - 强绑定 UserId
     * - 模拟 Request-Response 交互模式
     * - 当发生网络迁移时，调用方需显式 Terminate 旧 Flow 并重新 Create
     *
     * \param userId 关联的用户逻辑 ID
     * \param userNode 用户物理节点
     * \param targetProxy 目标 Proxy ID
     * \param dataRate 数据传输速率
     * \param packetSize 数据包大小
     * \param interval 业务交互间隔
     * \return FlowHandle 新建立的流句柄
     */
    FlowHandle CreatePeriodicTcpFlow (UserId userId,
                                      Ptr<Node> userNode,
                                      ProxyId targetProxy,
                                      DataRate dataRate,
                                      uint32_t packetSize,
                                      Time interval);

    // =================================================================
    // Primitive 4：低频存在性流量 (Low-Rate Presence Flow)  //实际需要么？ 应该本身就是正常的运行流量，并没有区别，姑且留着
    // 对应原：关联型影子流量 (Covert/Shadow Traffic)
    // =================================================================

    /**
     * \brief 创建低频存在性流量
     *
     * 特征：
     * - 强绑定 UserId
     * - 极低速率/占空比
     * - 几乎不消耗带宽资源，仅在网络层产生通信记录
     *
     * \param userId 关联的用户逻辑 ID
     * \param userNode 用户物理节点
     * \param targetProxy 目标 Proxy ID
     * \param interval 心跳包发送间隔
     * \return FlowHandle 流句柄
     */
    FlowHandle CreateLowRatePresenceFlow (UserId userId,
                                          Ptr<Node> userNode,
                                          ProxyId targetProxy,
                                          Time interval);

    // =================================================================
    // 生命周期与关联管理 (Management API)
    // =================================================================

    /**
     * \brief 终止指定 Handle 的流量
     * \param flow 需要销毁的流句柄
     * \note 执行底层 Socket Close 或 Application Stop
     */
    void TerminateFlow (FlowHandle flow);

    /**
     * \brief 终止指定 User 关联的所有流量
     * \param userId 目标用户 ID
     * \note 此接口仅执行批量终止操作，不包含任何策略含义（如封禁/隔离等由上层决定）
     */
    void TerminateFlowsByUser (UserId userId);

    /**
     * \brief 查询某用户当前关联的所有流句柄
     * \param userId 用户 ID
     * \return 活跃的流句柄列表
     */
    std::vector<FlowHandle> GetFlowsByUser (UserId userId) const;

    /**
     * \brief 查询某 Flow 当前连接的目标 Proxy
     * \param flow 流句柄
     * \return 目标 Proxy ID
     */
    ProxyId GetTargetProxy (FlowHandle flow) const;

private:
    // 内部结构体：保存流的物理实体状态
    struct FlowEntity {
        FlowHandle id;
        UserId userId;          // 若为 USER_ID_NONE 则表示无归属
        ProxyId targetProxyId;
        Ptr<Application> app;   // 底层 ns-3 应用实例
        bool isActive;
        Time startTime;  //用于 区分“Application 已 Stop” 和 “Flow 被逻辑终止”
        Time stopTime;  // 若未停止则为 Time::Max()
    };

    FlowHandle GetNextFlowId ();
    
    // 核心数据结构：Flow 注册表
    std::map<FlowHandle, FlowEntity> m_flowMap;
    
    // 辅助索引：User -> Flows (一对多)
    std::map<UserId, std::vector<FlowHandle>> m_userFlowIndex;

    Ptr<MtdNetworkHelper> m_netHelper;
    uint32_t m_flowIdCounter;
};

} // namespace mtd
} // namespace ns3

#endif /* MTD_TRAFFIC_HELPER_H */