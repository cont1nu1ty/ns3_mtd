/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Full Defense Test
 * 
 * This is a comprehensive end-to-end test that demonstrates
 * a complete attack-defense cycle with all MTD components:
 * 
 * 1. Attack Detection (LocalDetector, CrossAgentDetector, GlobalDetector)
 * 2. Risk Scoring and Classification (ScoreManager)
 * 3. Domain Management (DomainManager - split/merge/migrate)
 * 4. Shuffle Operations (ShuffleController - multiple strategies)
 * 5. Adaptive Attack Response (AttackGenerator)
 * 6. Event-driven Architecture (EventBus)
 * 7. Metrics Export (ExportApi)
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mtd-benchmark-module.h"
#include "ns3/mtd-network-helper.h"
#include "ns3/mtd-export-api.h"

using namespace ns3;
using namespace ns3::mtd;

NS_LOG_COMPONENT_DEFINE("MtdFullDefenseTest");

// ==================== Global Statistics ====================

struct TestStatistics {
    uint32_t attacksDetected = 0;
    uint32_t shufflesTriggered = 0;
    uint32_t usersMigrated = 0;
    uint32_t domainsSplit = 0;
    uint32_t domainsMerged = 0;
    uint32_t attackAdaptations = 0;
    double totalDefenseLatency = 0.0;
    std::vector<double> riskScoreHistory;
};

static TestStatistics g_stats;

// ==================== Event Handlers ====================

void OnAttackDetected(Ptr<ScoreManager> scoreManager, 
                      Ptr<ShuffleController> shuffleController,
                      Ptr<DomainManager> domainManager,
                      const MtdEvent& event)
{
    g_stats.attacksDetected++;
    
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] ATTACK DETECTED on proxy " 
                << event.sourceNodeId);
    
    // Get affected users - use sourceNodeId as proxy ID
    uint32_t proxyId = event.sourceNodeId;
    
    // Find domain containing this proxy
    std::vector<uint32_t> domainIds = domainManager->GetAllDomainIds();
    uint32_t domainId = 0;
    
    for (uint32_t dId : domainIds) {
        Domain info = domainManager->GetDomainInfo(dId);
        for (uint32_t pId : info.proxyIds) {
            if (pId == proxyId) {
                domainId = dId;
                break;
            }
        }
        if (domainId != 0) break;
    }
    
    if (domainId == 0) return;
    
    Domain domainInfo = domainManager->GetDomainInfo(domainId);
    
    // Update scores for all users in affected domain
    for (uint32_t userId : domainInfo.userIds) {
        DetectionObservation obs;
        obs.rateAnomaly = 0.8;
        obs.patternAnomaly = 0.6;
        obs.persistenceFactor = 0.5;
        obs.confidence = 0.85;
        scoreManager->UpdateScore(userId, obs);
        
        RiskLevel level = scoreManager->GetRiskLevel(userId);
        double score = scoreManager->GetScore(userId);
        g_stats.riskScoreHistory.push_back(score);
        
        NS_LOG_INFO("  User " << userId << " score updated: " << score 
                    << " (Risk: " << static_cast<int>(level) << ")");
        
        // If high risk, trigger shuffle
        if (level >= RiskLevel::HIGH) {
            NS_LOG_INFO("  -> HIGH RISK: Triggering shuffle for domain " << domainId);
            shuffleController->TriggerShuffle(domainId, ShuffleMode::SCORE_DRIVEN);
            g_stats.shufflesTriggered++;
        }
    }
}

void OnShuffleCompleted(const MtdEvent& event)
{
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] SHUFFLE COMPLETED for domain " 
                << event.sourceNodeId);
    
    // Check if usersAffected is in metadata
    auto it = event.metadata.find("usersAffected");
    if (it != event.metadata.end()) {
        NS_LOG_INFO("  Users affected: " << it->second);
    }
}

void OnUserMigrated(const MtdEvent& event)
{
    g_stats.usersMigrated++;
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] USER MIGRATED: User " 
                << event.sourceNodeId);
    
    auto it = event.metadata.find("newDomainId");
    if (it != event.metadata.end()) {
        NS_LOG_INFO("  -> Moved to domain " << it->second);
    }
}

void OnDomainSplit(const MtdEvent& event)
{
    g_stats.domainsSplit++;
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] DOMAIN SPLIT: Domain " 
                << event.sourceNodeId);
    
    auto it = event.metadata.find("newDomainId");
    if (it != event.metadata.end()) {
        NS_LOG_INFO("  -> New domain created: " << it->second);
    }
}

void OnDomainMerge(const MtdEvent& event)
{
    g_stats.domainsMerged++;
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] DOMAIN MERGE event");
}

void OnProxySwitched(const MtdEvent& event)
{
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] PROXY SWITCHED: Node " 
                << event.sourceNodeId);
    
    auto it = event.metadata.find("newProxyId");
    if (it != event.metadata.end()) {
        NS_LOG_INFO("  -> Switched to proxy " << it->second);
    }
}

// ==================== Simulation Phases ====================

void SimulateNormalTraffic(Ptr<LocalDetector> detector, 
                           const std::vector<uint32_t>& proxyIds)
{
    NS_LOG_INFO("\n========== PHASE 1: Normal Traffic ==========");
    
    for (uint32_t proxyId : proxyIds) {
        TrafficStats stats;
        stats.packetRate = 100.0 + (proxyId * 10);
        stats.byteRate = 50000.0;
        stats.activeConnections = 20;
        stats.avgLatency = 5.0;
        detector->UpdateStats(proxyId, stats);
        
        DetectionObservation obs = detector->Analyze(proxyId);
        NS_LOG_INFO("Proxy " << proxyId << " - Normal traffic: anomaly=" 
                    << obs.patternAnomaly << ", confidence=" << obs.confidence);
    }
}

void SimulateAttackTraffic(Ptr<LocalDetector> detector,
                           Ptr<EventBus> eventBus,
                           uint32_t targetProxyId)
{
    NS_LOG_INFO("\n========== PHASE 2: Attack Traffic ==========");
    NS_LOG_INFO("Attacking proxy " << targetProxyId);
    
    // Simulate increasing attack intensity
    for (int intensity = 1; intensity <= 5; intensity++) {
        TrafficStats stats;
        stats.packetRate = 10000.0 * intensity;
        stats.byteRate = 5000000.0 * intensity;
        stats.activeConnections = 1000 * intensity;
        stats.avgLatency = 50.0 * intensity;
        detector->UpdateStats(targetProxyId, stats);
        
        DetectionObservation obs = detector->Analyze(targetProxyId);
        NS_LOG_INFO("  Attack intensity " << intensity << ": packetRate=" 
                    << stats.packetRate << ", anomaly=" << obs.patternAnomaly);
        
        // If anomaly detected, publish event
        if (obs.patternAnomaly > 0.7) {
            MtdEvent event(EventType::ATTACK_DETECTED, Simulator::Now().GetNanoSeconds());
            event.sourceNodeId = targetProxyId;
            event.metadata["anomalyScore"] = std::to_string(obs.patternAnomaly);
            event.metadata["packetRate"] = std::to_string(stats.packetRate);
            eventBus->Publish(event);
        }
    }
}

void SimulateDomainOperations(Ptr<DomainManager> domainManager,
                              Ptr<ShuffleController> shuffleController)
{
    NS_LOG_INFO("\n========== PHASE 3: Domain Operations ==========");
    
    std::vector<uint32_t> domainIds = domainManager->GetAllDomainIds();
    
    if (domainIds.size() >= 2) {
        // Test split
        uint32_t domainToSplit = domainIds[0];
        Domain info = domainManager->GetDomainInfo(domainToSplit);
        
        if (info.userIds.size() >= 4) {
            NS_LOG_INFO("Splitting domain " << domainToSplit << " (has " 
                        << info.userIds.size() << " users)");
            uint32_t newDomainId = domainManager->SplitDomain(domainToSplit);
            NS_LOG_INFO("  -> New domain created: " << newDomainId);
        }
        
        // Test user migration
        if (domainIds.size() >= 2) {
            uint32_t sourceDomain = domainIds[0];
            uint32_t targetDomain = domainIds[1];
            Domain sourceInfo = domainManager->GetDomainInfo(sourceDomain);
            
            if (!sourceInfo.userIds.empty()) {
                uint32_t userToMove = sourceInfo.userIds[0];
                NS_LOG_INFO("Migrating user " << userToMove << " from domain " 
                            << sourceDomain << " to " << targetDomain);
                domainManager->MoveUser(userToMove, targetDomain);
            }
        }
    }
}

void TestShuffleStrategies(Ptr<ShuffleController> shuffleController,
                           Ptr<DomainManager> domainManager)
{
    NS_LOG_INFO("\n========== PHASE 4: Shuffle Strategies ==========");
    
    std::vector<uint32_t> domainIds = domainManager->GetAllDomainIds();
    if (domainIds.empty()) return;
    
    uint32_t testDomain = domainIds[0];
    
    // Test different shuffle strategies
    std::vector<std::pair<ShuffleMode, std::string>> strategies = {
        {ShuffleMode::RANDOM, "RANDOM"},
        {ShuffleMode::SCORE_DRIVEN, "SCORE_DRIVEN"},
        {ShuffleMode::ROUND_ROBIN, "ROUND_ROBIN"},
        {ShuffleMode::ATTACKER_AVOID, "ATTACKER_AVOID"}
    };
    
    for (const auto& strategy : strategies) {
        NS_LOG_INFO("Testing " << strategy.second << " shuffle strategy");
        ShuffleEvent event = shuffleController->TriggerShuffle(testDomain, strategy.first);
        NS_LOG_INFO("  Result: success=" << event.success 
                    << ", usersAffected=" << event.usersAffected);
    }
}

void TestAdaptiveAttacker(Ptr<AttackGenerator> attackGenerator,
                          Ptr<EventBus> eventBus)
{
    NS_LOG_INFO("\n========== PHASE 5: Adaptive Attacker ==========");
    
    // Configure adaptive attack
    AttackParams params;
    params.type = AttackType::UDP_FLOOD;
    params.rate = 20000.0;
    params.adaptToDefense = true;
    params.cooldownPeriod = 5.0;
    attackGenerator->Generate(params);
    attackGenerator->SetBehavior(AttackBehavior::ADAPTIVE);
    
    // Add initial targets
    attackGenerator->AddTarget(1);
    attackGenerator->AddTarget(2);
    
    NS_LOG_INFO("Starting adaptive attack with targets: 1, 2");
    attackGenerator->Start();
    
    // Simulate defense event via EventBus (attacker is subscribed internally)
    MtdEvent defenseEvent(EventType::SHUFFLE_COMPLETED, Simulator::Now().GetNanoSeconds());
    defenseEvent.sourceNodeId = 1;
    defenseEvent.metadata["usersAffected"] = "5";
    defenseEvent.metadata["newProxy"] = "3";
    
    NS_LOG_INFO("Publishing defense event - shuffle completed");
    eventBus->Publish(defenseEvent);
    
    NS_LOG_INFO("Attacker in cooldown: " << (attackGenerator->IsInCooldown() ? "yes" : "no"));
    
    // Check current attack targets
    auto targets = attackGenerator->GetTargets();
    NS_LOG_INFO("Current attack targets: ");
    for (uint32_t t : targets) {
        NS_LOG_INFO("  - Proxy " << t);
    }
    
    attackGenerator->Stop();
}

// ==================== Main Function ====================

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("MtdFullDefenseTest", LOG_LEVEL_INFO);
    
    // Parameters
    uint32_t numClients = 30;
    uint32_t numProxies = 6;
    uint32_t numDomains = 3;
    double simulationTime = 60.0;
    
    CommandLine cmd;
    cmd.AddValue("clients", "Number of clients", numClients);
    cmd.AddValue("proxies", "Number of proxies", numProxies);
    cmd.AddValue("domains", "Number of domains", numDomains);
    cmd.AddValue("time", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);
    
    NS_LOG_INFO("╔══════════════════════════════════════════════════════════════╗");
    NS_LOG_INFO("║           MTD-BENCHMARK FULL DEFENSE TEST                    ║");
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    NS_LOG_INFO("║ Clients: " << numClients << "  Proxies: " << numProxies 
                << "  Domains: " << numDomains << "  Time: " << simulationTime << "s");
    NS_LOG_INFO("╚══════════════════════════════════════════════════════════════╝\n");
    
    // ==================== Create Components ====================
    
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    eventBus->SetLogging(true);
    
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    domainManager->SetEventBus(eventBus);
    
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    scoreManager->SetEventBus(eventBus);
    
    // Configure scoring weights
    ScoreWeights weights;
    weights.alpha = 0.4;  // rate weight
    weights.beta = 0.3;   // anomaly weight
    weights.gamma = 0.2;  // persistence weight
    weights.delta = 0.1;  // feedback weight
    scoreManager->SetWeights(weights);
    
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    
    // Configure shuffle
    ShuffleConfig shuffleConfig;
    shuffleConfig.baseFrequency = 10.0;
    shuffleConfig.minFrequency = 3.0;
    shuffleConfig.maxFrequency = 60.0;
    shuffleConfig.sessionAffinity = true;
    shuffleController->SetConfig(shuffleConfig);
    
    // Create detectors
    Ptr<LocalDetector> localDetector = CreateObject<LocalDetector>();
    Ptr<CrossAgentDetector> crossAgentDetector = CreateObject<CrossAgentDetector>();
    Ptr<GlobalDetector> globalDetector = CreateObject<GlobalDetector>();
    
    // Configure detection thresholds
    DetectionThresholds thresholds;
    thresholds.packetRateThreshold = 5000.0;
    thresholds.byteRateThreshold = 2000000.0;
    thresholds.connectionThreshold = 500.0;
    thresholds.anomalyScoreThreshold = 0.6;
    localDetector->SetThresholds(thresholds);
    
    // Create attack generator
    Ptr<AttackGenerator> attackGenerator = CreateObject<AttackGenerator>();
    attackGenerator->SetEventBus(eventBus);
    
    // Create export API
    Ptr<ExportApi> exportApi = CreateObject<ExportApi>();
    exportApi->SetEventBus(eventBus);
    exportApi->SetDomainManager(domainManager);
    exportApi->SetShuffleController(shuffleController);
    exportApi->SetAttackGenerator(attackGenerator);
    
    // Configure experiment
    ExperimentConfig experimentConfig;
    experimentConfig.experimentId = "mtd_full_defense_test";
    experimentConfig.randomSeed = 42;
    experimentConfig.simulationDuration = simulationTime;
    experimentConfig.numClients = numClients;
    experimentConfig.numProxies = numProxies;
    experimentConfig.numDomains = numDomains;
    exportApi->SetExperimentConfig(experimentConfig);
    
    // ==================== Subscribe to Events ====================
    
    eventBus->Subscribe(EventType::ATTACK_DETECTED,
        MakeBoundCallback(&OnAttackDetected, scoreManager, shuffleController, domainManager));
    
    eventBus->Subscribe(EventType::SHUFFLE_COMPLETED,
        MakeCallback(&OnShuffleCompleted));
    
    eventBus->Subscribe(EventType::USER_MIGRATED,
        MakeCallback(&OnUserMigrated));
    
    eventBus->Subscribe(EventType::DOMAIN_SPLIT,
        MakeCallback(&OnDomainSplit));
    
    eventBus->Subscribe(EventType::DOMAIN_MERGE,
        MakeCallback(&OnDomainMerge));
    
    eventBus->Subscribe(EventType::PROXY_SWITCHED,
        MakeCallback(&OnProxySwitched));
    
    // ==================== Setup Domains and Users ====================
    
    NS_LOG_INFO("\n========== SETUP: Creating Domains ==========");
    
    std::vector<uint32_t> domainIds;
    std::vector<uint32_t> proxyIds;
    
    // Create proxies
    for (uint32_t p = 0; p < numProxies; p++) {
        proxyIds.push_back(p + 1);  // Proxy IDs start from 1
    }
    
    // Create domains
    for (uint32_t d = 0; d < numDomains; d++) {
        std::ostringstream name;
        name << "Domain_" << d;
        uint32_t domainId = domainManager->CreateDomain(name.str());
        domainIds.push_back(domainId);
        
        // Assign proxies (round-robin)
        for (uint32_t p = d; p < numProxies; p += numDomains) {
            domainManager->AddProxy(domainId, proxyIds[p]);
        }
        
        NS_LOG_INFO("Created " << name.str() << " (ID: " << domainId << ")");
    }
    
    // Assign users
    for (uint32_t u = 0; u < numClients; u++) {
        uint32_t domainIdx = u % numDomains;
        domainManager->AddUser(domainIds[domainIdx], u + 100);  // User IDs start from 100
        
        // Initial proxy assignment
        Domain info = domainManager->GetDomainInfo(domainIds[domainIdx]);
        if (!info.proxyIds.empty()) {
            uint32_t proxyId = info.proxyIds[u % info.proxyIds.size()];
            shuffleController->AssignUserToProxy(u + 100, proxyId);
        }
    }
    
    NS_LOG_INFO("Assigned " << numClients << " users to " << numDomains << " domains");
    
    // Add attack targets
    for (uint32_t proxyId : proxyIds) {
        attackGenerator->AddTarget(proxyId);
    }
    
    // ==================== Run Test Phases ====================
    
    // Phase 1: Normal traffic
    SimulateNormalTraffic(localDetector, proxyIds);
    
    // Phase 2: Attack traffic
    SimulateAttackTraffic(localDetector, eventBus, proxyIds[0]);
    
    // Phase 3: Domain operations
    SimulateDomainOperations(domainManager, shuffleController);
    
    // Phase 4: Test shuffle strategies
    TestShuffleStrategies(shuffleController, domainManager);
    
    // Phase 5: Adaptive attacker
    TestAdaptiveAttacker(attackGenerator, eventBus);
    
    // ==================== Schedule Simulation Events ====================
    
    // Start periodic shuffling
    for (uint32_t domainId : domainIds) {
        shuffleController->SetFrequency(domainId, 10.0);
        Simulator::Schedule(Seconds(5.0), 
            &ShuffleController::StartPeriodicShuffle, shuffleController, domainId);
    }
    
    // Schedule attack
    AttackParams attackParams;
    attackParams.type = AttackType::UDP_FLOOD;
    attackParams.rate = 15000.0;
    attackParams.adaptToDefense = true;
    attackGenerator->Generate(attackParams);
    attackGenerator->SetBehavior(AttackBehavior::ADAPTIVE);
    
    Simulator::Schedule(Seconds(10.0), &AttackGenerator::Start, attackGenerator);
    Simulator::Schedule(Seconds(simulationTime - 5.0), &AttackGenerator::Stop, attackGenerator);
    
    // Start auto-recording
    exportApi->StartAutoRecording(5.0);
    
    // ==================== Run Simulation ====================
    
    NS_LOG_INFO("\n========== RUNNING SIMULATION ==========");
    
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // ==================== Export Results ====================
    
    NS_LOG_INFO("\n========== EXPORTING RESULTS ==========");
    
    exportApi->ExportExperimentSnapshot("mtd_full_test_snapshot.json");
    exportApi->ExportDomainState("mtd_full_test_domains.json");
    exportApi->ExportShuffleEvents("mtd_full_test_shuffles.csv");
    exportApi->ExportAttackEvents("mtd_full_test_attacks.csv");
    exportApi->ExportEventHistory("mtd_full_test_events.json");
    
    // ==================== Print Summary ====================
    
    NS_LOG_INFO("\n╔══════════════════════════════════════════════════════════════╗");
    NS_LOG_INFO("║                    TEST RESULTS SUMMARY                       ║");
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    NS_LOG_INFO("║ Attacks Detected:     " << g_stats.attacksDetected);
    NS_LOG_INFO("║ Shuffles Triggered:   " << g_stats.shufflesTriggered);
    NS_LOG_INFO("║ Users Migrated:       " << g_stats.usersMigrated);
    NS_LOG_INFO("║ Domains Split:        " << g_stats.domainsSplit);
    NS_LOG_INFO("║ Domains Merged:       " << g_stats.domainsMerged);
    NS_LOG_INFO("║ Attack Adaptations:   " << g_stats.attackAdaptations);
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    
    // Calculate average risk score
    double avgRiskScore = 0.0;
    if (!g_stats.riskScoreHistory.empty()) {
        for (double score : g_stats.riskScoreHistory) {
            avgRiskScore += score;
        }
        avgRiskScore /= g_stats.riskScoreHistory.size();
    }
    NS_LOG_INFO("║ Average Risk Score:   " << avgRiskScore);
    
    // Shuffle controller stats
    auto shuffleStats = shuffleController->GetShuffleStats();
    NS_LOG_INFO("║ Total Shuffles:       " << shuffleStats["totalShuffles"]);
    NS_LOG_INFO("║ Shuffle Success Rate: " << shuffleStats["successRate"]);
    
    // Attack stats
    auto attackStats = attackGenerator->GetStatistics();
    NS_LOG_INFO("║ Attack Packets:       " << attackStats["packetCount"]);
    
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    NS_LOG_INFO("║ Results exported to: mtd_full_test_*.csv/json                ║");
    NS_LOG_INFO("╚══════════════════════════════════════════════════════════════╝");
    
    // Cleanup
    Simulator::Destroy();
    
    return 0;
}
