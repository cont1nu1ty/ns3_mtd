/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Full Defense Test
 * 
 * Comprehensive attack-defense cycle demonstration:
 * 
 * Attack Model:
 *   - AttackGenerator attacks proxies in round-robin
 *   - When SHUFFLE_COMPLETED is detected, attacker enters cooldown
 * 
 * Defense Model:
 *   - LocalDetector monitors proxy traffic periodically
 *   - On ATTACK_DETECTED: users on that proxy get score +1
 *   - Shuffle triggered for affected domain
 *   - When score >= BAN_THRESHOLD, user is banned
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/mtd-benchmark-module.h"
#include "ns3/mtd-network-helper.h"
#include "ns3/mtd-export-api.h"

#include <set>

using namespace ns3;
using namespace ns3::mtd;

NS_LOG_COMPONENT_DEFINE("MtdFullDefenseTest");

// ==================== Configuration Constants ====================

static const double BAN_THRESHOLD = 0.99;           // Score threshold for banning (max 1.0)
static const double DETECTION_INTERVAL = 1.0;       // Detection check interval (seconds)
static const double ATTACK_COOLDOWN = 5.0;          // Attacker cooldown after shuffle (seconds)
static const double ATTACK_RATE = 15000.0;          // Attack packet rate

// ==================== Global State ====================

struct TestStatistics {
    uint32_t attacksDetected = 0;
    uint32_t shufflesTriggered = 0;
    uint32_t usersBanned = 0;
    uint32_t attackCooldowns = 0;
};

static TestStatistics g_stats;

// Global pointers for scheduled callbacks
static Ptr<LocalDetector> g_detector;
static Ptr<EventBus> g_eventBus;
static Ptr<ShuffleController> g_shuffleController;
static Ptr<DomainManager> g_domainManager;
static Ptr<AttackGenerator> g_attackGenerator;
static Ptr<ScoreManager> g_scoreManager;
static std::vector<uint32_t> g_proxyIds;

// Attacker is a user - can only attack the proxy they're assigned to
static const uint32_t ATTACKER_USER_ID = 999;

// ==================== Score Management ====================
// Simple +0.1 per attack, 10 attacks = banned
static const double SCORE_INCREMENT = 0.1;

// ==================== Event Handlers ====================

void OnAttackDetected(const MtdEvent& event)
{
    g_stats.attacksDetected++;
    uint32_t proxyId = event.sourceNodeId;
    
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] ATTACK_DETECTED on proxy " << proxyId);
    
    // Find domain for this proxy
    uint32_t domainId = 0;
    for (uint32_t dId : g_domainManager->GetAllDomainIds()) {
        Domain info = g_domainManager->GetDomainInfo(dId);
        for (uint32_t pId : info.proxyIds) {
            if (pId == proxyId) {
                domainId = dId;
                break;
            }
        }
        if (domainId != 0) break;
    }
    
    if (domainId == 0) {
        NS_LOG_INFO("  Proxy " << proxyId << " not found in any domain");
        return;
    }
    
    // Get users on the ATTACKED PROXY only (not entire domain)
    std::vector<uint32_t> proxyUsers = g_shuffleController->GetUsersOnProxy(proxyId);
    NS_LOG_INFO("  Proxy " << proxyId << " has " << proxyUsers.size() << " users");
    
    // Filter out already banned users
    std::vector<uint32_t> activeUsers;
    for (uint32_t userId : proxyUsers) {
        if (!g_domainManager->IsUserBanned(userId)) {
            activeUsers.push_back(userId);
        }
    }
    
    if (activeUsers.empty()) {
        NS_LOG_INFO("  No active users on proxy " << proxyId);
        // Still trigger shuffle
        if (domainId != 0) {
            g_shuffleController->TriggerShuffle(domainId, ShuffleMode::RANDOM, "attack_response");
            g_stats.shufflesTriggered++;
        }
        return;
    }
    
    // Add score to users on the attacked proxy only
    std::map<uint32_t, double> newScores = g_scoreManager->AddScoreToUsers(
        activeUsers, SCORE_INCREMENT, "proxy_under_attack");
    
    // Check for users to ban
    std::vector<uint32_t> usersToBan;
    for (const auto& [userId, newScore] : newScores) {
        NS_LOG_INFO("  User " << userId << " score: " << newScore);
        if (newScore >= BAN_THRESHOLD) {
            usersToBan.push_back(userId);
        }
    }
    
    // Ban users who exceeded threshold
    for (uint32_t userId : usersToBan) {
        NS_LOG_INFO("  -> BANNING user " << userId << " (score >= " << BAN_THRESHOLD << ")");
        g_domainManager->BanUser(userId, "Score threshold exceeded");
        g_stats.usersBanned++;
    }
    
    // Trigger shuffle for the affected domain
    if (domainId != 0) {
        NS_LOG_INFO("  -> Triggering shuffle for domain " << domainId);
        g_shuffleController->TriggerShuffle(domainId, ShuffleMode::RANDOM, "attack_response");
        g_stats.shufflesTriggered++;
    }
}

void OnShuffleCompleted(const MtdEvent& event)
{
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] SHUFFLE_COMPLETED domain=" 
                << event.sourceNodeId);
    
    auto it = event.metadata.find("usersAffected");
    if (it != event.metadata.end()) {
        NS_LOG_INFO("  Users shuffled: " << it->second);
    }
    
    // Attacker will detect this via EventBus subscription and enter cooldown
    if (g_attackGenerator && g_attackGenerator->IsInCooldown()) {
        g_stats.attackCooldowns++;
        NS_LOG_INFO("  Attacker entered cooldown");
    }
}

void OnUserBanned(const MtdEvent& event)
{
    auto userIt = event.metadata.find("userId");
    auto reasonIt = event.metadata.find("reason");
    
    NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] USER_BANNED: " 
                << (userIt != event.metadata.end() ? userIt->second : "?")
                << " reason: " << (reasonIt != event.metadata.end() ? reasonIt->second : "?"));
}

void OnProxySwitched(const MtdEvent& event)
{
    // Quiet logging for proxy switches
}

// ==================== Periodic Detection ====================

void PeriodicDetection()
{
    // Check if attack is active
    if (!g_attackGenerator || !g_attackGenerator->IsActive()) {
        Simulator::Schedule(Seconds(DETECTION_INTERVAL), &PeriodicDetection);
        return;
    }
    
    // Check if attacker is in cooldown (lost target after shuffle)
    if (g_attackGenerator->IsInCooldown()) {
        Simulator::Schedule(Seconds(DETECTION_INTERVAL), &PeriodicDetection);
        return;
    }
    
    // Attacker can only attack the proxy they're assigned to
    uint32_t attackerProxy = g_shuffleController->GetProxyAssignment(ATTACKER_USER_ID);
    if (attackerProxy == 0) {
        NS_LOG_INFO("[t=" << Simulator::Now().GetSeconds() << "s] Attacker has no proxy (banned?)");
        Simulator::Schedule(Seconds(DETECTION_INTERVAL), &PeriodicDetection);
        return;
    }
    
    // Simulate attack on attacker's own proxy
    TrafficStats stats;
    stats.packetRate = ATTACK_RATE;
    stats.byteRate = ATTACK_RATE * 512;
    stats.activeConnections = 5000;
    stats.avgLatency = 100.0;
    g_detector->UpdateStats(attackerProxy, stats);
    
    // Analyze and detect
    DetectionObservation obs = g_detector->Analyze(attackerProxy);
    
    // If anomaly detected, publish ATTACK_DETECTED event
    if (obs.patternAnomaly > 0.6) {
        MtdEvent event(EventType::ATTACK_DETECTED, Simulator::Now().GetMilliSeconds());
        event.sourceNodeId = attackerProxy;
        event.metadata["anomalyScore"] = std::to_string(obs.patternAnomaly);
        event.metadata["packetRate"] = std::to_string(stats.packetRate);
        event.metadata["attackerUserId"] = std::to_string(ATTACKER_USER_ID);
        g_eventBus->Publish(event);
    }
    
    // Reschedule
    Simulator::Schedule(Seconds(DETECTION_INTERVAL), &PeriodicDetection);
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
    NS_LOG_INFO("║ Ban Threshold: " << BAN_THRESHOLD << "  Cooldown: " << ATTACK_COOLDOWN << "s");
    NS_LOG_INFO("╚══════════════════════════════════════════════════════════════╝\n");
    
    // ==================== Create Components ====================
    
    g_eventBus = CreateObject<EventBus>();
    g_eventBus->SetLogging(true);
    
    g_domainManager = CreateObject<DomainManager>();
    g_domainManager->SetEventBus(g_eventBus);
    
    g_scoreManager = CreateObject<ScoreManager>();
    g_scoreManager->SetEventBus(g_eventBus);
    // Uses default scoring + AddScore() for simple increments
    
    g_shuffleController = CreateObject<ShuffleController>();
    g_shuffleController->SetDomainManager(g_domainManager);
    g_shuffleController->SetScoreManager(g_scoreManager);
    g_shuffleController->SetEventBus(g_eventBus);
    
    // Link DomainManager to ShuffleController for proxy assignment
    g_domainManager->SetShuffleController(g_shuffleController);
    
    // Configure shuffle
    ShuffleConfig shuffleConfig;
    shuffleConfig.baseFrequency = 30.0;
    shuffleConfig.minFrequency = 5.0;
    shuffleConfig.maxFrequency = 120.0;
    shuffleConfig.sessionAffinity = false;
    g_shuffleController->SetConfig(shuffleConfig);
    
    // Create detector
    g_detector = CreateObject<LocalDetector>();
    DetectionThresholds thresholds;
    thresholds.packetRateThreshold = 5000.0;
    thresholds.byteRateThreshold = 2000000.0;
    thresholds.connectionThreshold = 500.0;
    thresholds.anomalyScoreThreshold = 0.5;
    g_detector->SetThresholds(thresholds);
    
    // Create attack generator with cooldown
    g_attackGenerator = CreateObject<AttackGenerator>();
    g_attackGenerator->SetEventBus(g_eventBus);  // Subscribe to defense events
    g_attackGenerator->SetCooldownPeriod(ATTACK_COOLDOWN);
    g_attackGenerator->SetBehavior(AttackBehavior::ADAPTIVE);
    
    AttackParams attackParams;
    attackParams.type = AttackType::UDP_FLOOD;
    attackParams.rate = ATTACK_RATE;
    attackParams.adaptToDefense = true;
    attackParams.cooldownPeriod = ATTACK_COOLDOWN;
    g_attackGenerator->Generate(attackParams);
    
    // Create export API
    Ptr<ExportApi> exportApi = CreateObject<ExportApi>();
    exportApi->SetEventBus(g_eventBus);
    exportApi->SetDomainManager(g_domainManager);
    exportApi->SetShuffleController(g_shuffleController);
    exportApi->SetAttackGenerator(g_attackGenerator);
    
    ExperimentConfig experimentConfig;
    experimentConfig.experimentId = "mtd_full_defense_test";
    experimentConfig.randomSeed = 42;
    experimentConfig.simulationDuration = simulationTime;
    experimentConfig.numClients = numClients;
    experimentConfig.numProxies = numProxies;
    experimentConfig.numDomains = numDomains;
    exportApi->SetExperimentConfig(experimentConfig);
    
    // Setup output directory and file-based event logging
    std::string outputDir = "output/" + experimentConfig.experimentId;
    exportApi->SetOutputDirectory(outputDir);
    exportApi->SetupEventLogging(FileLogLevel::DEBUG, 50, false);
    
    // ==================== Subscribe to Events ====================
    
    g_eventBus->Subscribe(EventType::ATTACK_DETECTED, MakeCallback(&OnAttackDetected));
    g_eventBus->Subscribe(EventType::SHUFFLE_COMPLETED, MakeCallback(&OnShuffleCompleted));
    g_eventBus->Subscribe(EventType::USER_BANNED, MakeCallback(&OnUserBanned));
    g_eventBus->Subscribe(EventType::PROXY_SWITCHED, MakeCallback(&OnProxySwitched));
    
    // ==================== Setup Domains and Users ====================
    
    NS_LOG_INFO("========== SETUP: Creating Network ==========\n");
    
    std::vector<uint32_t> domainIds;
    
    // Create proxies
    for (uint32_t p = 0; p < numProxies; p++) {
        g_proxyIds.push_back(p + 1);  // Proxy IDs: 1, 2, ..., numProxies
    }
    
    // Create domains and assign proxies
    for (uint32_t d = 0; d < numDomains; d++) {
        std::ostringstream name;
        name << "Domain_" << d;
        uint32_t domainId = g_domainManager->CreateDomain(name.str());
        domainIds.push_back(domainId);
        
        // Round-robin proxy assignment to domains
        for (uint32_t p = d; p < numProxies; p += numDomains) {
            g_domainManager->AddProxy(domainId, g_proxyIds[p]);
        }
        
        NS_LOG_INFO("Created " << name.str() << " (ID: " << domainId << ")");
    }
    
    // Add users to domains
    for (uint32_t u = 0; u < numClients; u++) {
        uint32_t userId = u + 100;  // User IDs: 100, 101, ...
        uint32_t domainIdx = u % numDomains;
        uint32_t domainId = domainIds[domainIdx];
        g_domainManager->AddUser(domainId, userId);
    }
    
    // Add attacker as a user in domain 1
    g_domainManager->AddUser(domainIds[0], ATTACKER_USER_ID);
    
    // Assign all users to proxies (round-robin within each domain)
    uint32_t assigned = g_domainManager->AssignAllUsersToProxies();
    NS_LOG_INFO("Assigned " << assigned << " users to proxies across " << numDomains << " domains");
    NS_LOG_INFO("Attacker (user " << ATTACKER_USER_ID << ") in domain " << domainIds[0] << "\n");
    
    // No need to add targets - attacker attacks their own proxy
    
    // ==================== Schedule Simulation Events ====================
    
    // Start attack at t=5s
    Simulator::Schedule(Seconds(5.0), &AttackGenerator::Start, g_attackGenerator);
    
    // Stop attack at t=55s (5 seconds before simulation end)
    Simulator::Schedule(Seconds(simulationTime - 5.0), &AttackGenerator::Stop, g_attackGenerator);
    
    // Start periodic detection at t=5s
    Simulator::Schedule(Seconds(5.0), &PeriodicDetection);
    
    // ==================== Run Simulation ====================
    
    NS_LOG_INFO("========== RUNNING SIMULATION ==========\n");
    
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // ==================== Export Results ====================
    
    NS_LOG_INFO("\n========== EXPORTING RESULTS ==========\n");
    
    exportApi->ExportExperimentSnapshot("snapshot.json");
    exportApi->ExportDomainState("domains.json");
    exportApi->ExportShuffleEvents("shuffles.csv");
    exportApi->ExportBanEvents("bans.csv");
    exportApi->ExportEventHistory("events.json");
    
    NS_LOG_INFO("Results exported to: " << outputDir);
    
    // ==================== Print Summary ====================
    
    NS_LOG_INFO("\n╔══════════════════════════════════════════════════════════════╗");
    NS_LOG_INFO("║                    TEST RESULTS SUMMARY                       ║");
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    NS_LOG_INFO("║ Attacks Detected:     " << g_stats.attacksDetected);
    NS_LOG_INFO("║ Shuffles Triggered:   " << g_stats.shufflesTriggered);
    NS_LOG_INFO("║ Users Banned:         " << g_stats.usersBanned);
    NS_LOG_INFO("║ Attack Cooldowns:     " << g_stats.attackCooldowns);
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    
    // Final user scores (from ScoreManager)
    std::vector<uint32_t> trackedUsers = g_scoreManager->GetTrackedUsers();
    uint32_t highScoreUsers = 0;
    double maxScore = 0.0;
    for (uint32_t userId : trackedUsers) {
        double score = g_scoreManager->GetScore(userId);
        if (score > 0) highScoreUsers++;
        if (score > maxScore) maxScore = score;
    }
    NS_LOG_INFO("║ Users with score > 0: " << highScoreUsers);
    NS_LOG_INFO("║ Max user score:       " << maxScore);
    
    // Attack stats
    auto attackStats = g_attackGenerator->GetStatistics();
    NS_LOG_INFO("║ Attack Packets:       " << attackStats["packetCount"]);
    
    // Shuffle stats
    auto shuffleStats = g_shuffleController->GetShuffleStats();
    NS_LOG_INFO("║ Total Shuffles:       " << shuffleStats["totalShuffles"]);
    
    NS_LOG_INFO("╠══════════════════════════════════════════════════════════════╣");
    NS_LOG_INFO("║ Output directory: " << outputDir);
    NS_LOG_INFO("╚══════════════════════════════════════════════════════════════╝");
    
    // Cleanup
    Simulator::Destroy();
    
    return 0;
}
