/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark Example: Demonstrates MTD proxy-switching defense
 * 
 * Key Features:
 * - Automatic Ground Truth logging via AttackGenerator
 * - EventBus auto-initializes logging to logs/YYYYMMDD_HHMMSS/
 * - All output goes to log files, minimal console output
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/applications-module.h"
#include "ns3/mtd-benchmark-module.h"

using namespace ns3;
using namespace ns3::mtd;

// Global pointers
Ptr<ScoreManager> g_scoreManager;
Ptr<ShuffleController> g_shuffleController;
Ptr<DomainManager> g_domainManager;
Ptr<EventBus> g_eventBus;
Ptr<LocalDetector> g_detector;
Ptr<AttackGenerator> g_attackGen;

// Simulation state
const double RISK_THRESHOLD = 0.7;
uint32_t g_bannedUserCount = 0;
uint32_t g_totalUsers = 0;

// Handler for attack detection - updates scores and triggers defense
void 
OnAttackDetected(uint32_t proxyId)
{
    std::vector<uint32_t> users = g_shuffleController->GetUsersOnProxy(proxyId);

  for (auto userId : users)
    {
        double newScore = g_scoreManager->AddScore(userId, 0.15, "attack_detected");

        if (newScore >= RISK_THRESHOLD && !g_domainManager->IsUserBanned(userId))
            {
            g_domainManager->BanUser(userId, "risk_threshold_exceeded");
              g_bannedUserCount++;
              
            if (g_attackGen && g_attackGen->IsActive())
            {
                g_attackGen->MarkDefenseTriggered();
            }
        }
    }

    // Trigger shuffle
    g_shuffleController->TriggerShuffle(1, ShuffleMode::SCORE_DRIVEN, "attack_response");

  if (g_bannedUserCount >= g_totalUsers && g_totalUsers > 0)
    {
        Simulator::Stop();
    }
}

// Periodic detection check
void
PeriodicDetectionCheck()
{
    for (auto agentId : g_detector->GetMonitoredAgents())
    {
        DetectionObservation obs = g_detector->Analyze(agentId);
        double anomalyScore = (obs.rateAnomaly + obs.connectionAnomaly + obs.patternAnomaly) / 3.0;
        
        if (anomalyScore > 0.5)
        {
            OnAttackDetected(agentId);
        }
    }
    
    Simulator::Schedule(Seconds(1.0), &PeriodicDetectionCheck);
}

// Simulate detection during active attack
void
OngoingAttackDetection()
{
    if (g_attackGen && g_attackGen->IsActive())
    {
        OnAttackDetected(g_attackGen->GetCurrentRecord().targetProxyId);
    }
}

int 
main(int argc, char* argv[])
{
    // Parse command line
    uint32_t numClients = 20;
    uint32_t numProxies = 3;
    double simTime = 30.0;
    
  CommandLine cmd;
    cmd.AddValue("clients", "Number of client nodes", numClients);
    cmd.AddValue("proxies", "Number of proxy nodes", numProxies);
    cmd.AddValue("simTime", "Simulation time in seconds", simTime);
    cmd.Parse(argc, argv);
  
    Time::SetResolution(Time::NS);
    g_totalUsers = numClients;
    
    // 1. Create Network Topology
    Ptr<MtdNetworkHelper> networkHelper = CreateObject<MtdNetworkHelper>();
    
    TopologyConfig topoConfig;
    topoConfig.numClients = numClients;
    topoConfig.numProxies = numProxies;
    topoConfig.numServers = 1;
    topoConfig.numAttackers = 1;
    
    networkHelper->SetTopologyConfig(topoConfig);
    networkHelper->CreateTopology();
    networkHelper->InstallInternetStack();
    networkHelper->AssignIpAddresses();
    networkHelper->SetupRouting();
    
    // 2. Create MTD Core Components
    g_eventBus = CreateObject<EventBus>();
    g_domainManager = CreateObject<DomainManager>();
    g_scoreManager = CreateObject<ScoreManager>();
    g_shuffleController = CreateObject<ShuffleController>();
    g_detector = CreateObject<LocalDetector>();
  
    // Wire up dependencies
    g_domainManager->SetEventBus(g_eventBus);
    g_scoreManager->SetEventBus(g_eventBus);
    g_shuffleController->SetEventBus(g_eventBus);
    g_shuffleController->SetDomainManager(g_domainManager);
    g_shuffleController->SetScoreManager(g_scoreManager);
    g_detector->SetEventBus(g_eventBus);
    
    // 3. Initialize Domain and User Placement
    uint32_t domainId = g_domainManager->CreateDomain("default");
    
    for (uint32_t i = 0; i < numProxies; i++)
    {
        g_domainManager->AddProxy(domainId, i);
    }
    
    for (uint32_t i = 0; i < numClients; i++)
    {
        g_domainManager->AddUser(domainId, i);
    }
    
    g_domainManager->SetShuffleController(g_shuffleController);
    g_domainManager->AssignUsersToProxies(domainId);

    // 4. Configure Shuffle Controller
    ShuffleConfig shuffleConfig;
    shuffleConfig.baseFrequency = 10.0;
    shuffleConfig.sessionAffinity = true;
    g_shuffleController->SetConfig(shuffleConfig);
    g_shuffleController->StartPeriodicShuffle(domainId);
    
    // 5. Set up Detection
    DetectionThresholds thresholds;
    thresholds.packetRateThreshold = 5000.0;
    thresholds.anomalyScoreThreshold = 0.5;
    g_detector->SetThresholds(thresholds);
    
    for (uint32_t i = 0; i < numProxies; i++)
    {
        TrafficStats stats;
        stats.packetsIn = 0;
        stats.bytesIn = 0;
        g_detector->UpdateStats(i, stats);
    }
    
    Simulator::Schedule(Seconds(1.0), &PeriodicDetectionCheck);
    
    // 6. Configure Attack Generator
    g_attackGen = CreateObject<AttackGenerator>();
    g_attackGen->SetEventBus(g_eventBus);
    g_attackGen->SetNetworkHelper(networkHelper);
    
    // First attack: UDP Flood
    AttackParams attackParams;
    attackParams.type = AttackType::UDP_FLOOD;
    attackParams.targetProxyId = 0;
    attackParams.rate = 5000.0;
    attackParams.packetSize = 512;
    attackParams.duration = 5.0;
    
    g_attackGen->Configure(attackParams);
    Simulator::Schedule(Seconds(5.0), &AttackGenerator::Start, g_attackGen);

    // Second attack: SYN Flood
    Simulator::Schedule(Seconds(15.0), [&]() {
        AttackParams params2;
        params2.type = AttackType::SYN_FLOOD;
        params2.targetProxyId = 1;
        params2.rate = 10000.0;
        params2.packetSize = 64;
        params2.duration = 3.0;
        
        g_attackGen->Configure(params2);
        g_attackGen->Start();
    });

    // Schedule detection during attacks
    Simulator::Schedule(Seconds(6.0), &OngoingAttackDetection);
    Simulator::Schedule(Seconds(16.0), &OngoingAttackDetection);

    // 7. Demonstrate system events (for system/ directory)
    Simulator::Schedule(Seconds(20.0), [&]() {
        // Trigger a domain split to generate DOMAIN_SPLIT event
        g_domainManager->SplitDomain(domainId);
    });
    
    // 8. Run Simulation
    Simulator::Stop(Seconds(simTime));
    Simulator::Run();
    
    // Finalize logging
    g_eventBus->FinalizeLogging();
    
    Simulator::Destroy();
    
  return 0;
}
