/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Test suite
 */

#include "ns3/test.h"
#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/mtd-benchmark-module.h"

#include <algorithm>

using namespace ns3;
using namespace ns3::mtd;

NS_LOG_COMPONENT_DEFINE("MtdBenchmarkTest");

/**
 * \brief Test case for EventBus
 */
class EventBusTestCase : public TestCase
{
public:
    EventBusTestCase();
    ~EventBusTestCase() override;
    
private:
    void DoRun() override;
    void OnEvent(const MtdEvent& event);
    
    bool m_eventReceived;
    EventType m_receivedType;
};

EventBusTestCase::EventBusTestCase()
    : TestCase("EventBus basic functionality test"),
      m_eventReceived(false),
      m_receivedType(EventType::SHUFFLE_TRIGGERED)
{
}

EventBusTestCase::~EventBusTestCase()
{
}

void
EventBusTestCase::OnEvent(const MtdEvent& event)
{
    m_eventReceived = true;
    m_receivedType = event.type;
}

void
EventBusTestCase::DoRun()
{
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    
    // Reset state
    m_eventReceived = false;
    m_receivedType = EventType::SHUFFLE_TRIGGERED;
    
    // Subscribe to an event using MakeCallback
    eventBus->Subscribe(EventType::SHUFFLE_COMPLETED, 
        MakeCallback(&EventBusTestCase::OnEvent, this));
    
    // Publish an event
    MtdEvent event(EventType::SHUFFLE_COMPLETED, 0);
    eventBus->Publish(event);
    
    NS_TEST_ASSERT_MSG_EQ(m_eventReceived, true, "Event should be received");
    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(m_receivedType), 
                          static_cast<int>(EventType::SHUFFLE_COMPLETED), 
                          "Event type should match");
    
    Simulator::Destroy();
}

/**
 * \brief Test case for ScoreManager
 */
class ScoreManagerTestCase : public TestCase
{
public:
    ScoreManagerTestCase();
    ~ScoreManagerTestCase() override;
    
private:
    void DoRun() override;
};

ScoreManagerTestCase::ScoreManagerTestCase()
    : TestCase("ScoreManager basic functionality test")
{
}

ScoreManagerTestCase::~ScoreManagerTestCase()
{
}

void
ScoreManagerTestCase::DoRun()
{
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    
    // Create an observation
    DetectionObservation obs;
    obs.rateAnomaly = 0.8;
    obs.patternAnomaly = 0.7;
    obs.persistenceFactor = 0.5;
    
    // Update score
    scoreManager->UpdateScore(1, obs);
    
    // Check score was updated
    double score = scoreManager->GetScore(1);
    NS_TEST_ASSERT_MSG_GT(score, 0.0, "Score should be greater than 0");
    
    // Check risk level
    RiskLevel level = scoreManager->GetRiskLevel(1);
    NS_TEST_ASSERT_MSG_NE(static_cast<int>(level), 
                          static_cast<int>(RiskLevel::CRITICAL),
                          "Initial risk should not be critical");
    
    Simulator::Destroy();
}

/**
 * \brief Test case for DomainManager
 */
class DomainManagerTestCase : public TestCase
{
public:
    DomainManagerTestCase();
    ~DomainManagerTestCase() override;
    
private:
    void DoRun() override;
};

DomainManagerTestCase::DomainManagerTestCase()
    : TestCase("DomainManager basic functionality test")
{
}

DomainManagerTestCase::~DomainManagerTestCase()
{
}

void
DomainManagerTestCase::DoRun()
{
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    
    // Create domains
    uint32_t domain1 = domainManager->CreateDomain("TestDomain1");
    uint32_t domain2 = domainManager->CreateDomain("TestDomain2");
    
    NS_TEST_ASSERT_MSG_GT(domain1, 0u, "Domain ID should be positive");
    NS_TEST_ASSERT_MSG_GT(domain2, 0u, "Domain ID should be positive");
    NS_TEST_ASSERT_MSG_NE(domain1, domain2, "Domain IDs should be different");
    
    // Add users
    NS_TEST_ASSERT_MSG_EQ(domainManager->AddUser(domain1, 100), true, 
                          "Should be able to add user");
    NS_TEST_ASSERT_MSG_EQ(domainManager->AddUser(domain1, 101), true, 
                          "Should be able to add user");
    
    // Check user domain
    NS_TEST_ASSERT_MSG_EQ(domainManager->GetDomain(100), domain1, 
                          "User should be in domain1");
    
    // Move user
    NS_TEST_ASSERT_MSG_EQ(domainManager->MoveUser(100, domain2), true, 
                          "Should be able to move user");
    NS_TEST_ASSERT_MSG_EQ(domainManager->GetDomain(100), domain2, 
                          "User should now be in domain2");
    
    // Check domain info
    Domain info = domainManager->GetDomainInfo(domain1);
    NS_TEST_ASSERT_MSG_EQ(info.userIds.size(), 1u, 
                          "Domain1 should have 1 user after migration");
    
    Simulator::Destroy();
}

/**
 * \brief Test case for ShuffleController
 */
class ShuffleControllerTestCase : public TestCase
{
public:
    ShuffleControllerTestCase();
    ~ShuffleControllerTestCase() override;
    
private:
    void DoRun() override;
};

ShuffleControllerTestCase::ShuffleControllerTestCase()
    : TestCase("ShuffleController basic functionality test")
{
}

ShuffleControllerTestCase::~ShuffleControllerTestCase()
{
}

void
ShuffleControllerTestCase::DoRun()
{
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    
    shuffleController->SetDomainManager(domainManager);
    
    // Create a domain with proxies and users
    uint32_t domainId = domainManager->CreateDomain("TestDomain");
    domainManager->AddProxy(domainId, 1);
    domainManager->AddProxy(domainId, 2);
    domainManager->AddUser(domainId, 100);
    domainManager->AddUser(domainId, 101);
    
    // Assign users to proxies
    shuffleController->AssignUserToProxy(100, 1);
    shuffleController->AssignUserToProxy(101, 2);
    
    // Check assignments
    NS_TEST_ASSERT_MSG_EQ(shuffleController->GetProxyAssignment(100), 1u, 
                          "User 100 should be assigned to proxy 1");
    NS_TEST_ASSERT_MSG_EQ(shuffleController->GetProxyAssignment(101), 2u, 
                          "User 101 should be assigned to proxy 2");
    
    // Trigger shuffle
    ShuffleEvent event = shuffleController->TriggerShuffle(domainId, ShuffleMode::RANDOM);
    NS_TEST_ASSERT_MSG_EQ(event.success, true, "Shuffle should succeed");
    
    Simulator::Destroy();
}

/**
 * \brief Test case for LocalDetector
 */
class LocalDetectorTestCase : public TestCase
{
public:
    LocalDetectorTestCase();
    ~LocalDetectorTestCase() override;
    
private:
    void DoRun() override;
};

LocalDetectorTestCase::LocalDetectorTestCase()
    : TestCase("LocalDetector basic functionality test")
{
}

LocalDetectorTestCase::~LocalDetectorTestCase()
{
}

void
LocalDetectorTestCase::DoRun()
{
    Ptr<LocalDetector> detector = CreateObject<LocalDetector>();
    
    // Set thresholds
    DetectionThresholds thresholds;
    thresholds.packetRateThreshold = 1000.0;
    detector->SetThresholds(thresholds);
    
    // Update stats - normal traffic
    TrafficStats stats;
    stats.packetRate = 500.0;
    stats.byteRate = 500000.0;
    stats.activeConnections = 50;
    detector->UpdateStats(1, stats);
    
    // Analyze
    DetectionObservation obs = detector->Analyze(1);
    NS_TEST_ASSERT_MSG_LT(obs.patternAnomaly, 0.5, 
                          "Normal traffic should have low anomaly score");
    
    // Update stats - attack traffic
    stats.packetRate = 50000.0;
    stats.byteRate = 50000000.0;
    stats.activeConnections = 5000;
    detector->UpdateStats(1, stats);
    
    obs = detector->Analyze(1);
    NS_TEST_ASSERT_MSG_GT(obs.patternAnomaly, 0.5, 
                          "Attack traffic should have high anomaly score");
    
    Simulator::Destroy();
}

/**
 * \brief Test case for AttackGenerator
 */
class AttackGeneratorTestCase : public TestCase
{
public:
    AttackGeneratorTestCase();
    ~AttackGeneratorTestCase() override;
    
private:
    void DoRun() override;
};

AttackGeneratorTestCase::AttackGeneratorTestCase()
    : TestCase("AttackGenerator basic functionality test")
{
}

AttackGeneratorTestCase::~AttackGeneratorTestCase()
{
}

void
AttackGeneratorTestCase::DoRun()
{
    Ptr<AttackGenerator> generator = CreateObject<AttackGenerator>();
    
    // Configure attack
    AttackParams params;
    params.type = AttackType::DOS;
    params.rate = 1000.0;
    params.targetProxyId = 1;
    params.duration = 10.0;
    
    generator->Generate(params);
    
    // Check configuration
    NS_TEST_ASSERT_MSG_EQ(generator->IsActive(), false, 
                          "Generator should not be active before Start()");
    
    generator->AddTarget(1);
    generator->AddTarget(2);
    
    auto targets = generator->GetTargets();
    NS_TEST_ASSERT_MSG_EQ(targets.size(), 2u, 
                          "Should have 2 targets");
    
    Simulator::Destroy();
}

/**
 * \brief End-to-end interaction test across detector, scoring, shuffle, and attack adaptation
 */
class MtdEndToEndTestCase : public TestCase
{
public:
    MtdEndToEndTestCase();
    ~MtdEndToEndTestCase() override;

private:
    void DoRun() override;
    void OnShuffleEvent(const MtdEvent& event);
    void OnProxySwitchEvent(const MtdEvent& event);
    
    bool m_shuffleEventReceived;
    bool m_proxySwitchReceived;
};

MtdEndToEndTestCase::MtdEndToEndTestCase()
    : TestCase("MTD integration flow test"),
      m_shuffleEventReceived(false),
      m_proxySwitchReceived(false)
{
}

MtdEndToEndTestCase::~MtdEndToEndTestCase() = default;

void
MtdEndToEndTestCase::OnShuffleEvent(const MtdEvent& /*event*/)
{
    m_shuffleEventReceived = true;
}

void
MtdEndToEndTestCase::OnProxySwitchEvent(const MtdEvent& /*event*/)
{
    m_proxySwitchReceived = true;
}

void
MtdEndToEndTestCase::DoRun()
{
    // Reset state
    m_shuffleEventReceived = false;
    m_proxySwitchReceived = false;
    
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    Ptr<LocalDetector> detector = CreateObject<LocalDetector>();
    Ptr<AttackGenerator> attackGenerator = CreateObject<AttackGenerator>();

    domainManager->SetEventBus(eventBus);
    scoreManager->SetEventBus(eventBus);
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);

    AttackParams params;
    params.targetProxyId = 1;
    attackGenerator->Generate(params);
    attackGenerator->SetBehavior(AttackBehavior::ADAPTIVE);
    attackGenerator->SetEventBus(eventBus);

    eventBus->Subscribe(EventType::SHUFFLE_COMPLETED, 
        MakeCallback(&MtdEndToEndTestCase::OnShuffleEvent, this));
    eventBus->Subscribe(EventType::PROXY_SWITCHED, 
        MakeCallback(&MtdEndToEndTestCase::OnProxySwitchEvent, this));

    uint32_t domainId = domainManager->CreateDomain("integration");
    domainManager->AddProxy(domainId, 1);
    domainManager->AddProxy(domainId, 2);
    domainManager->AddUser(domainId, 100);
    shuffleController->AssignUserToProxy(100, 1);

    DetectionThresholds thresholds;
    thresholds.packetRateThreshold = 100.0;
    thresholds.byteRateThreshold = 1000.0;
    thresholds.connectionThreshold = 50.0;
    thresholds.anomalyScoreThreshold = 0.5;
    detector->SetThresholds(thresholds);

    TrafficStats normalTraffic;
    normalTraffic.packetRate = 50.0;
    normalTraffic.byteRate = 500.0;
    normalTraffic.activeConnections = 20;
    detector->UpdateStats(1, normalTraffic);

    TrafficStats attackTraffic;
    attackTraffic.packetRate = 1000.0;
    attackTraffic.byteRate = 100000.0;
    attackTraffic.activeConnections = 500;
    detector->UpdateStats(1, attackTraffic);

    DetectionObservation observation = detector->Analyze(1);
    observation.rateAnomaly = 1.0;
    observation.patternAnomaly = 1.0;
    observation.persistenceFactor = 1.0;
    scoreManager->UpdateScore(100, observation);

    NS_TEST_ASSERT_MSG_EQ(static_cast<int>(scoreManager->GetRiskLevel(100)),
                          static_cast<int>(RiskLevel::HIGH),
                          "Attack observation should elevate user to HIGH risk");

    ShuffleEvent shuffleEvent = shuffleController->TriggerShuffle(domainId, ShuffleMode::SCORE_DRIVEN);
    NS_TEST_ASSERT_MSG_EQ(shuffleEvent.success, true, "Shuffle should succeed for populated domain");
    NS_TEST_ASSERT_MSG_EQ(shuffleController->GetProxyAssignment(100), 2u,
                          "User should be re-assigned to a different proxy");
    NS_TEST_ASSERT_MSG_GT(shuffleEvent.usersAffected, 0u,
                          "At least one user must be shuffled in integration flow");

    auto targets = attackGenerator->GetTargets();
    bool sawNewTarget = std::find(targets.begin(), targets.end(), 2u) != targets.end();
    NS_TEST_ASSERT_MSG_EQ(attackGenerator->IsInCooldown(), true,
                          "Adaptive attack generator should enter cooldown after defense event");
    NS_TEST_ASSERT_MSG_EQ(sawNewTarget, true,
                          "Attack generator should learn newly switched proxy as target");

    NS_TEST_ASSERT_MSG_EQ(m_shuffleEventReceived, true,
                          "Shuffle completion event should be broadcast on EventBus");
    NS_TEST_ASSERT_MSG_EQ(m_proxySwitchReceived, true,
                          "Proxy switch event should be broadcast on EventBus");

    attackGenerator->Stop();
    Simulator::Destroy();
}

/**
 * \brief MTD-Benchmark test suite
 */
class MtdBenchmarkTestSuite : public TestSuite
{
public:
    MtdBenchmarkTestSuite();
};

MtdBenchmarkTestSuite::MtdBenchmarkTestSuite()
    : TestSuite("mtd-benchmark", UNIT)
{
    AddTestCase(new EventBusTestCase, TestCase::QUICK);
    AddTestCase(new ScoreManagerTestCase, TestCase::QUICK);
    AddTestCase(new DomainManagerTestCase, TestCase::QUICK);
    AddTestCase(new ShuffleControllerTestCase, TestCase::QUICK);
    AddTestCase(new LocalDetectorTestCase, TestCase::QUICK);
    AddTestCase(new AttackGeneratorTestCase, TestCase::QUICK);
    AddTestCase(new MtdEndToEndTestCase, TestCase::QUICK);
}

static MtdBenchmarkTestSuite sMtdBenchmarkTestSuite;
