/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Python Algorithm Integration Example
 * 
 * This example demonstrates how to integrate Python defense algorithms
 * with NS-3 simulation using the PythonAlgorithmBridge.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mtd-benchmark-module.h"
#include "ns3/mtd-python-interface.h"

using namespace ns3;
using namespace ns3::mtd;

NS_LOG_COMPONENT_DEFINE("MtdPythonIntegration");

/**
 * Example 1: Using callbacks directly in C++
 * 
 * This demonstrates how Python callbacks would be used when
 * the Python bindings invoke C++ code.
 */
void
ExampleWithCallbacks()
{
    NS_LOG_INFO("=== Example 1: Direct Callback Usage ===");
    
    // Create core components
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    Ptr<LocalDetector> localDetector = CreateObject<LocalDetector>();
    
    // Connect components
    domainManager->SetEventBus(eventBus);
    scoreManager->SetEventBus(eventBus);
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    
    // Create Python Algorithm Bridge
    Ptr<PythonAlgorithmBridge> bridge = CreateObject<PythonAlgorithmBridge>();
    bridge->SetDomainManager(domainManager);
    bridge->SetScoreManager(scoreManager);
    bridge->SetShuffleController(shuffleController);
    bridge->SetEventBus(eventBus);
    bridge->SetLocalDetector(localDetector);
    
    // Configure the bridge
    PythonAlgorithmConfig config;
    config.algorithmName = "ExampleAlgorithm";
    config.evaluationInterval = 5.0;  // Evaluate every 5 seconds
    config.maxDecisionsPerEval = 10;
    bridge->SetConfig(config);
    
    // Register custom score calculator (simulating Python callback)
    bridge->RegisterScoreCalculator([](uint32_t userId, 
                                        const DetectionObservation& obs,
                                        double currentScore) -> double {
        // Example: Exponential Moving Average scoring
        double alpha = 0.3;
        double obsScore = 0.5 * obs.rateAnomaly + 0.3 * obs.patternAnomaly + 
                          0.2 * obs.persistenceFactor;
        return alpha * obsScore + (1 - alpha) * currentScore;
    });
    
    // Register custom risk classifier (simulating Python callback)
    bridge->RegisterRiskClassifier([](uint32_t userId, double score) -> RiskLevel {
        if (score > 0.8) return RiskLevel::CRITICAL;
        if (score > 0.6) return RiskLevel::HIGH;
        if (score > 0.3) return RiskLevel::MEDIUM;
        return RiskLevel::LOW;
    });
    
    // Register custom shuffle strategy (simulating Python callback)
    bridge->RegisterShuffleStrategy([](uint32_t userId,
                                       const std::vector<uint32_t>& proxies,
                                       const UserScore& score) -> uint32_t {
        if (proxies.empty()) return 0;
        
        // High-risk users go to last proxy (isolation)
        if (score.riskLevel == RiskLevel::HIGH || score.riskLevel == RiskLevel::CRITICAL) {
            return proxies.back();
        }
        
        // Normal users distributed across other proxies
        return proxies[userId % (proxies.size() - 1)];
    });
    
    // Register main defense evaluator (simulating Python callback)
    bridge->RegisterDefenseEvaluator([](const SimulationState& state) 
                                      -> std::vector<DefenseDecision> {
        std::vector<DefenseDecision> decisions;
        
        // Check each domain
        for (const auto& [domainId, domain] : state.domains) {
            double avgScore = 0.0;
            uint32_t highRiskCount = 0;
            
            // Calculate average score
            for (uint32_t userId : domain.userIds) {
                auto it = state.userScores.find(userId);
                if (it != state.userScores.end()) {
                    avgScore += it->second.currentScore;
                    if (it->second.riskLevel == RiskLevel::HIGH ||
                        it->second.riskLevel == RiskLevel::CRITICAL) {
                        highRiskCount++;
                    }
                }
            }
            if (!domain.userIds.empty()) {
                avgScore /= domain.userIds.size();
            }
            
            // Trigger shuffle if average score is high
            if (avgScore > 0.6) {
                DefenseDecision decision;
                decision.action = DefenseDecision::ActionType::TRIGGER_SHUFFLE;
                decision.targetDomainId = domainId;
                decision.shuffleMode = ShuffleMode::SCORE_DRIVEN;
                decision.reason = "High average risk score";
                decisions.push_back(decision);
            }
            
            // Adjust frequency based on high-risk user count
            if (highRiskCount > domain.userIds.size() * 0.2) {
                DefenseDecision decision;
                decision.action = DefenseDecision::ActionType::CHANGE_FREQUENCY;
                decision.targetDomainId = domainId;
                decision.newFrequency = 10.0;  // More frequent shuffling
                decision.reason = "Many high-risk users";
                decisions.push_back(decision);
            }
        }
        
        return decisions;
    });
    
    // Setup simulation
    uint32_t domainId = domainManager->CreateDomain("TestDomain");
    for (uint32_t i = 0; i < 5; ++i) {
        domainManager->AddProxy(domainId, i);
    }
    for (uint32_t i = 0; i < 20; ++i) {
        domainManager->AddUser(domainId, i);
    }
    
    // Simulate some traffic observations
    TrafficStats stats;
    stats.packetRate = 15000.0;  // High traffic
    stats.byteRate = 10000000.0;
    stats.activeConnections = 500;
    localDetector->UpdateStats(0, stats);
    
    // Get simulation state
    SimulationState state = bridge->GetSimulationState();
    NS_LOG_INFO("Domains: " << state.domains.size());
    NS_LOG_INFO("Users tracked: " << state.userScores.size());
    
    // Manually trigger evaluation
    uint32_t decisionCount = bridge->TriggerEvaluation();
    NS_LOG_INFO("Executed " << decisionCount << " decisions");
    
    // Get statistics
    auto stats_map = bridge->GetStatistics();
    NS_LOG_INFO("Total evaluations: " << stats_map["totalEvaluations"]);
    NS_LOG_INFO("Success rate: " << stats_map["successRate"]);
}

/**
 * Example 2: Using SimulationContext for simplified API
 */
void
ExampleWithSimulationContext()
{
    NS_LOG_INFO("=== Example 2: SimulationContext API ===");
    
    // Create components
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    Ptr<PythonAlgorithmBridge> bridge = CreateObject<PythonAlgorithmBridge>();
    
    // Connect components
    domainManager->SetEventBus(eventBus);
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    
    bridge->SetDomainManager(domainManager);
    bridge->SetScoreManager(scoreManager);
    bridge->SetShuffleController(shuffleController);
    bridge->SetEventBus(eventBus);
    
    // Create SimulationContext
    Ptr<SimulationContext> context = CreateObject<SimulationContext>();
    context->Initialize(bridge, domainManager, scoreManager, shuffleController, eventBus);
    
    // Setup domain
    uint32_t domainId = domainManager->CreateDomain("ContextDomain");
    for (uint32_t i = 0; i < 3; ++i) {
        domainManager->AddProxy(domainId, i);
    }
    for (uint32_t i = 0; i < 10; ++i) {
        domainManager->AddUser(domainId, i);
    }
    
    // Use context API
    NS_LOG_INFO("Current time: " << context->GetCurrentTime() << " seconds");
    NS_LOG_INFO("Domain IDs: " << context->GetDomainIds().size());
    NS_LOG_INFO("User IDs: " << context->GetUserIds().size());
    NS_LOG_INFO("Proxy IDs: " << context->GetProxyIds().size());
    
    // Log from context
    context->Log(1, "This is an info message from SimulationContext");
    
    // Direct actions via bridge
    bridge->TriggerShuffle(domainId, ShuffleMode::RANDOM);
    bridge->ChangeShuffleFrequency(domainId, 15.0);
}

/**
 * Example 3: Decision execution
 */
void
ExampleDecisionExecution()
{
    NS_LOG_INFO("=== Example 3: Decision Execution ===");
    
    // Create components
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    
    Ptr<PythonAlgorithmBridge> bridge = CreateObject<PythonAlgorithmBridge>();
    bridge->SetDomainManager(domainManager);
    bridge->SetScoreManager(scoreManager);
    bridge->SetShuffleController(shuffleController);
    bridge->SetEventBus(eventBus);
    
    // Setup
    uint32_t domain1 = domainManager->CreateDomain("Domain1");
    uint32_t domain2 = domainManager->CreateDomain("Domain2");
    domainManager->AddProxy(domain1, 0);
    domainManager->AddProxy(domain1, 1);
    domainManager->AddProxy(domain2, 2);
    domainManager->AddUser(domain1, 100);
    domainManager->AddUser(domain1, 101);
    
    // Create and execute decisions
    std::vector<DefenseDecision> decisions;
    
    // Decision 1: Trigger shuffle
    DefenseDecision d1;
    d1.action = DefenseDecision::ActionType::TRIGGER_SHUFFLE;
    d1.targetDomainId = domain1;
    d1.shuffleMode = ShuffleMode::RANDOM;
    d1.reason = "Test shuffle";
    decisions.push_back(d1);
    
    // Decision 2: Migrate user
    DefenseDecision d2;
    d2.action = DefenseDecision::ActionType::MIGRATE_USER;
    d2.targetUserId = 100;
    d2.targetDomainId = domain2;
    d2.reason = "Isolate user";
    decisions.push_back(d2);
    
    // Decision 3: Change frequency
    DefenseDecision d3;
    d3.action = DefenseDecision::ActionType::CHANGE_FREQUENCY;
    d3.targetDomainId = domain1;
    d3.newFrequency = 10.0;
    d3.reason = "Increase shuffle rate";
    decisions.push_back(d3);
    
    // Execute all decisions
    uint32_t successCount = bridge->ExecuteDecisions(decisions);
    NS_LOG_INFO("Executed " << successCount << "/" << decisions.size() << " decisions");
    
    // Check decision history
    auto history = bridge->GetDecisionHistory(10);
    NS_LOG_INFO("Decision history entries: " << history.size());
    
    // Verify user migration
    uint32_t userDomain = domainManager->GetDomain(100);
    NS_LOG_INFO("User 100 is now in domain: " << userDomain);
}

int
main(int argc, char *argv[])
{
    LogComponentEnable("MtdPythonIntegration", LOG_LEVEL_INFO);
    
    NS_LOG_INFO("MTD-Benchmark Python Integration Examples");
    NS_LOG_INFO("==========================================");
    
    ExampleWithCallbacks();
    NS_LOG_INFO("");
    
    ExampleWithSimulationContext();
    NS_LOG_INFO("");
    
    ExampleDecisionExecution();
    
    NS_LOG_INFO("");
    NS_LOG_INFO("All examples completed successfully!");
    
    return 0;
}
