/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Basic example simulation
 * 
 * This example demonstrates the basic usage of the MTD-benchmark module
 * for proxy-switching defense evaluation.
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mtd-benchmark-module.h"

using namespace ns3;
using namespace ns3::mtd;

NS_LOG_COMPONENT_DEFINE("MtdBenchmarkExample");

/**
 * Example: Basic MTD simulation with proxy switching
 * 
 * Network topology:
 *   [Clients] --- [Proxies] --- [Servers]
 *                     |
 *               [Attackers]
 *                     |
 *               [Controller]
 * 
 * The simulation demonstrates:
 * 1. Setting up the MTD network topology
 * 2. Creating domains and assigning users/proxies
 * 3. Configuring detection and scoring
 * 4. Running periodic shuffles
 * 5. Simulating attacks
 * 6. Exporting results
 */
int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("MtdBenchmarkExample", LOG_LEVEL_INFO);
    
    // Simulation parameters
    uint32_t numClients = 50;
    uint32_t numProxies = 5;
    uint32_t numServers = 2;
    uint32_t numAttackers = 1;
    double simulationTime = 60.0; // seconds
    double shuffleFrequency = 10.0; // seconds
    
    // Parse command line arguments
    CommandLine cmd;
    cmd.AddValue("clients", "Number of client nodes", numClients);
    cmd.AddValue("proxies", "Number of proxy nodes", numProxies);
    cmd.AddValue("servers", "Number of server nodes", numServers);
    cmd.AddValue("attackers", "Number of attacker nodes", numAttackers);
    cmd.AddValue("time", "Simulation time in seconds", simulationTime);
    cmd.AddValue("shuffle", "Shuffle frequency in seconds", shuffleFrequency);
    cmd.Parse(argc, argv);
    
    NS_LOG_INFO("=== MTD-Benchmark Example Simulation ===");
    NS_LOG_INFO("Clients: " << numClients);
    NS_LOG_INFO("Proxies: " << numProxies);
    NS_LOG_INFO("Servers: " << numServers);
    NS_LOG_INFO("Attackers: " << numAttackers);
    NS_LOG_INFO("Simulation time: " << simulationTime << " seconds");
    NS_LOG_INFO("Shuffle frequency: " << shuffleFrequency << " seconds");
    
    // ================== Create Core Components ==================
    
    // Create event bus
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    eventBus->SetLogging(true);
    
    // Create domain manager
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    domainManager->SetEventBus(eventBus);
    
    // Create score manager
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    scoreManager->SetEventBus(eventBus);
    
    // Create shuffle controller
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    
    // Configure shuffle
    ShuffleConfig shuffleConfig;
    shuffleConfig.baseFrequency = shuffleFrequency;
    shuffleConfig.sessionAffinity = true;
    shuffleController->SetConfig(shuffleConfig);
    
    // Create detectors
    Ptr<LocalDetector> localDetector = CreateObject<LocalDetector>();
    Ptr<CrossAgentDetector> crossAgentDetector = CreateObject<CrossAgentDetector>();
    Ptr<GlobalDetector> globalDetector = CreateObject<GlobalDetector>();
    
    // Create attack generator
    Ptr<AttackGenerator> attackGenerator = CreateObject<AttackGenerator>();
    attackGenerator->SetEventBus(eventBus);
    
    // Configure adaptive attack
    AttackParams attackParams;
    attackParams.type = AttackType::DOS;
    attackParams.rate = 5000.0; // packets per second
    attackParams.adaptToDefense = true;
    attackParams.cooldownPeriod = 10.0;
    attackGenerator->Generate(attackParams);
    attackGenerator->SetBehavior(AttackBehavior::ADAPTIVE);
    
    // Create export API
    Ptr<ExportApi> exportApi = CreateObject<ExportApi>();
    exportApi->SetDomainManager(domainManager);
    exportApi->SetShuffleController(shuffleController);
    exportApi->SetAttackGenerator(attackGenerator);
    exportApi->SetEventBus(eventBus);
    
    // Configure experiment
    ExperimentConfig experimentConfig;
    experimentConfig.experimentId = "mtd_example_001";
    experimentConfig.randomSeed = 42;
    experimentConfig.simulationDuration = simulationTime;
    experimentConfig.numClients = numClients;
    experimentConfig.numProxies = numProxies;
    experimentConfig.numDomains = 3;
    experimentConfig.numAttackers = numAttackers;
    experimentConfig.defaultShuffleFrequency = shuffleFrequency;
    exportApi->SetExperimentConfig(experimentConfig);
    
    // ================== Create Domains ==================
    
    NS_LOG_INFO("Creating domains...");
    
    uint32_t numDomains = 3;
    std::vector<uint32_t> domainIds;
    
    for (uint32_t d = 0; d < numDomains; ++d)
    {
        std::ostringstream name;
        name << "Domain_" << d;
        uint32_t domainId = domainManager->CreateDomain(name.str());
        domainIds.push_back(domainId);
        
        // Assign proxies to domains (round-robin)
        for (uint32_t p = d; p < numProxies; p += numDomains)
        {
            domainManager->AddProxy(domainId, p);
        }
        
        NS_LOG_INFO("Created " << name.str() << " (ID: " << domainId << ")");
    }
    
    // Assign users to domains
    for (uint32_t u = 0; u < numClients; ++u)
    {
        uint32_t domainIdx = u % numDomains;
        domainManager->AddUser(domainIds[domainIdx], u);
    }
    
    NS_LOG_INFO("Assigned " << numClients << " users to " << numDomains << " domains");
    
    // ================== Setup Attack Targets ==================
    
    // Add all proxies as potential attack targets
    for (uint32_t p = 0; p < numProxies; ++p)
    {
        attackGenerator->AddTarget(p);
    }
    
    // ================== Schedule Events ==================
    
    // Start periodic shuffling for each domain
    for (uint32_t domainId : domainIds)
    {
        shuffleController->SetFrequency(domainId, shuffleFrequency);
        Simulator::Schedule(Seconds(shuffleFrequency), 
            &ShuffleController::StartPeriodicShuffle, shuffleController, domainId);
    }
    
    // Start attack after 5 seconds
    Simulator::Schedule(Seconds(5.0), &AttackGenerator::Start, attackGenerator);
    NS_LOG_INFO("Attack scheduled to start at t=5s");
    
    // Stop attack at end
    Simulator::Schedule(Seconds(simulationTime - 5.0), &AttackGenerator::Stop, attackGenerator);
    NS_LOG_INFO("Attack scheduled to stop at t=" << (simulationTime - 5.0) << "s");
    
    // Note: Traffic simulation is simplified in this example.
    // In a real simulation, you would use actual network traffic
    // and callbacks from the network stack.
    
    // Start auto-recording
    exportApi->StartAutoRecording(5.0);
    
    // ================== Run Simulation ==================
    
    NS_LOG_INFO("Starting simulation...");
    
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // ================== Export Results ==================
    
    NS_LOG_INFO("Exporting results...");
    
    exportApi->ExportExperimentSnapshot("mtd_experiment_snapshot.json");
    exportApi->ExportDomainState("mtd_domain_state.json");
    exportApi->ExportShuffleEvents("mtd_shuffle_events.csv");
    exportApi->ExportAttackEvents("mtd_attack_events.csv");
    exportApi->ExportEventHistory("mtd_event_history.json");
    
    // Print summary
    NS_LOG_INFO("=== Simulation Complete ===");
    
    auto summary = exportApi->GetPerformanceSummary();
    NS_LOG_INFO("Performance Summary:");
    for (const auto& pair : summary)
    {
        NS_LOG_INFO("  " << pair.first << ": " << pair.second);
    }
    
    auto shuffleStats = shuffleController->GetShuffleStats();
    NS_LOG_INFO("Shuffle Statistics:");
    for (const auto& pair : shuffleStats)
    {
        NS_LOG_INFO("  " << pair.first << ": " << pair.second);
    }
    
    auto attackStats = attackGenerator->GetStatistics();
    NS_LOG_INFO("Attack Statistics:");
    for (const auto& pair : attackStats)
    {
        NS_LOG_INFO("  " << pair.first << ": " << pair.second);
    }
    
    // Cleanup
    Simulator::Destroy();
    
    return 0;
}
