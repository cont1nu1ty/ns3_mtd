#include "ns3/mtd-benchmark-module.h"
#include "ns3/test.h"
#include "ns3/simulator.h"

using namespace ns3;
using namespace ns3::mtd;

/**
 * 
 */

// ===========================================================================
// 1. DomainManager 逻辑测试
// ===========================================================================
class MtdDomainTestCase : public TestCase
{
public:
    MtdDomainTestCase() : TestCase("测试 DomainManager 的域操作逻辑") {}
    void DoRun() override {
        Ptr<DomainManager> mgr = CreateObject<DomainManager>();
            
            // 方案 A：手动降低阈值（推荐用于单元测试）
            DomainThresholds thresholds;
            thresholds.minProxies = 1;      // 分裂后每个域仅需 1 个代理
            thresholds.minUsers = 1;       // 分裂后每个域仅需 1 个用户
            thresholds.splitThreshold = 0.5; // 降低负载触发门槛
            mgr->SetThresholds(thresholds);

            uint32_t domId = mgr->CreateDomain("Alpha");

            // 方案 B：或者提供满足默认阈值（minProxies=2, minUsers=10）的资源量
            // 这里我们添加 4 个代理和 20 个用户
            for (uint32_t i = 0; i < 4; ++i) mgr->AddProxy(domId, 100 + i);
            for (uint32_t i = 0; i < 20; ++i) mgr->AddUser(domId, 2000 + i);

            // 模拟触发分裂的负载
            mgr->UpdateLoadFactor(domId, 0.9);
            
            NS_TEST_EXPECT_MSG_EQ(mgr->NeedsRebalancing(), true, "负载 0.9 应触发重平衡");

            // 执行分裂
            uint32_t newDomId = mgr->SplitDomain(domId);
            
            // 验证结果
            NS_TEST_EXPECT_MSG_NE(newDomId, 0, "资源充足且满足最小约束时，SplitDomain 不应返回 0");
            
            // 验证分裂后的资源分布
            std::vector<uint32_t> oldDomUsers = mgr->GetDomainUsers(domId);
            std::vector<uint32_t> newDomUsers = mgr->GetDomainUsers(newDomId);
            NS_TEST_EXPECT_MSG_GT(newDomUsers.size(), 0, "新域中应包含迁移的用户");
    }
};

// ===========================================================================
// 2. ScoreManager 评分逻辑测试
// ===========================================================================
class MtdScoreTestCase : public TestCase
{
public:
    MtdScoreTestCase() : TestCase("测试 ScoreManager 的风险分值计算") {}
    void DoRun() override {
        Ptr<ScoreManager> sm = CreateObject<ScoreManager>();
        uint32_t userId = 2001;

        // 测试初始分数与分值增加
        double initialScore = sm->GetScore(userId);
        NS_TEST_EXPECT_MSG_EQ(initialScore, 0.0, "初始分数应为 0");

        sm->AddScore(userId, 0.5, "检测到端口扫描"); //
        NS_TEST_EXPECT_MSG_EQ(sm->GetScore(userId), 0.5, "分数增加值不正确");

        // 测试风险等级转换
        // 默认 RiskThresholds: MediumMax = 0.6
        sm->AddScore(userId, 0.2, "持续异常流量"); 
        NS_TEST_EXPECT_MSG_EQ(sm->GetRiskLevel(userId) == RiskLevel::HIGH, true, "分数达到 0.7 时应为 HIGH 等级");

        // 测试时间衰减 (ApplyTimeDecay)
        sm->ApplyTimeDecay(1000); // 衰减取决于 lambda 权重
        NS_TEST_EXPECT_MSG_LT(sm->GetScore(userId), 0.7, "时间衰减后分数应下降");
    }
};

// ===========================================================================
// 3. Attack & EventBus 联动测试
// ===========================================================================
class MtdAttackLogicTestCase : public TestCase
{
public:
    MtdAttackLogicTestCase() : TestCase("测试攻击生成与检测事件流") {}
    
    // 模拟事件回调
    void OnAttackDetected(const MtdEvent& ev) {
        m_detected = true;
    }

    void DoRun() override {
        Ptr<EventBus> bus = CreateObject<EventBus>();
        Ptr<LocalDetector> detector = CreateObject<LocalDetector>();
        detector->SetEventBus(bus);
        m_detected = false;

        // 订阅检测事件
        bus->Subscribe(EventType::ATTACK_DETECTED, 
            MakeCallback(&MtdAttackLogicTestCase::OnAttackDetected, this));

        // 构造触发检测的统计数据
        TrafficStats stats;
        stats.packetRate = 20000.0; // 超过默认阈值 10000.0
        detector->UpdateStats(100, stats);
        
        // 执行分析逻辑
        detector->Analyze(100); 

        NS_TEST_ASSERT_MSG_EQ(m_detected, true, "流量超标时 Detector 应发布事件到 EventBus");
    }
private:
    bool m_detected;
};

// ===========================================================================
// 测试套件定义
// ===========================================================================
class MtdModuleTestSuite : public TestSuite
{
public:
    MtdModuleTestSuite() : TestSuite("mtd-benchmark-unit", Type::UNIT) {
        AddTestCase(new MtdDomainTestCase(), TestCase::Duration::QUICK);
        AddTestCase(new MtdScoreTestCase(), TestCase::Duration::QUICK);
        AddTestCase(new MtdAttackLogicTestCase(), TestCase::Duration::QUICK);
    }
};

static MtdModuleTestSuite g_mtdModuleTestSuite;