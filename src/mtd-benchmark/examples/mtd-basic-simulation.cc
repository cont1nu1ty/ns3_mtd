/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Basic simulation with network topology
 * 
 * This example creates a complete NS-3 simulation with:
 * - Network nodes and connections
 * - MTD proxy switching
 * - Attack simulation
 * - Metrics collection
 */

#include "ns3/core-module.h"
#include "ns3/network-module.h"
#include "ns3/internet-module.h"
#include "ns3/point-to-point-module.h"
#include "ns3/applications-module.h"
#include "ns3/mtd-benchmark-module.h"
#include "ns3/mtd-network-helper.h"

using namespace ns3;
using namespace ns3::mtd;

NS_LOG_COMPONENT_DEFINE("MtdBasicSimulation");

int main(int argc, char *argv[])
{
    // Enable logging
    LogComponentEnable("MtdBasicSimulation", LOG_LEVEL_INFO);
    LogComponentEnable("MtdNetworkHelper", LOG_LEVEL_INFO);
    
    // Parameters
    uint32_t numClients = 20;
    uint32_t numProxies = 4;
    uint32_t numServers = 2;
    uint32_t numAttackers = 1;
    double simulationTime = 30.0;
    
    CommandLine cmd;
    cmd.AddValue("clients", "Number of clients", numClients);
    cmd.AddValue("proxies", "Number of proxies", numProxies);
    cmd.AddValue("time", "Simulation time", simulationTime);
    cmd.Parse(argc, argv);
    
    NS_LOG_INFO("=== MTD Basic Simulation ===");
    
    // ================== Create MTD Components ==================
    
    // Event bus
    Ptr<EventBus> eventBus = CreateObject<EventBus>();
    eventBus->SetLogging(true);
    
    // Domain manager
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    domainManager->SetEventBus(eventBus);
    
    // Score manager
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    scoreManager->SetEventBus(eventBus);
    
    // Shuffle controller
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    
    // Attack generator
    Ptr<AttackGenerator> attackGenerator = CreateObject<AttackGenerator>();
    attackGenerator->SetEventBus(eventBus);
    
    // ================== Create Network Topology ==================
    
    MtdNetworkHelper networkHelper;
    
    TopologyConfig topoConfig;
    topoConfig.numClients = numClients;
    topoConfig.numProxies = numProxies;
    topoConfig.numServers = numServers;
    topoConfig.numAttackers = numAttackers;
    
    networkHelper.SetTopologyConfig(topoConfig);
    networkHelper.CreateTopology();
    networkHelper.InstallInternetStack();
    networkHelper.AssignIpAddresses();
    networkHelper.SetupRouting();
    
    // Initialize MTD controller with network nodes
    networkHelper.InitializeMtdController(domainManager, shuffleController, scoreManager);
    
    // Print topology summary
    networkHelper.PrintTopologySummary();
    
    // ================== Configure Attack ==================
    
    // Add proxies as targets
    NodeContainer proxyNodes = networkHelper.GetProxyNodes();
    for (uint32_t i = 0; i < proxyNodes.GetN(); ++i)
    {
        attackGenerator->AddTarget(proxyNodes.Get(i)->GetId());
    }
    
    AttackParams attackParams;
    attackParams.type = AttackType::UDP_FLOOD;
    attackParams.rate = 10000.0;
    attackParams.adaptToDefense = true;
    attackGenerator->Generate(attackParams);
    attackGenerator->SetBehavior(AttackBehavior::ADAPTIVE);
    
    // ================== Schedule Simulation Events ==================
    
    // Start periodic shuffling
    std::vector<uint32_t> domainIds = domainManager->GetAllDomainIds();
    for (uint32_t domainId : domainIds)
    {
        shuffleController->SetFrequency(domainId, 5.0);
        Simulator::Schedule(Seconds(2.0), 
            &ShuffleController::StartPeriodicShuffle, shuffleController, domainId);
    }
    
    // Start attack
    Simulator::Schedule(Seconds(5.0), &AttackGenerator::Start, attackGenerator);
    NS_LOG_INFO("Attack will start at t=5s");
    
    // Stop attack
    Simulator::Schedule(Seconds(simulationTime - 2.0), &AttackGenerator::Stop, attackGenerator);
    NS_LOG_INFO("Attack will stop at t=" << (simulationTime - 2.0) << "s");
    
    // ================== Run ==================
    
    Simulator::Stop(Seconds(simulationTime));
    Simulator::Run();
    
    // ================== Results ==================
    
    NS_LOG_INFO("=== Simulation Complete ===");
    
    auto shuffleStats = shuffleController->GetShuffleStats();
    NS_LOG_INFO("Total shuffles: " << shuffleStats["totalShuffles"]);
    NS_LOG_INFO("Success rate: " << shuffleStats["successRate"]);
    
    auto attackStats = attackGenerator->GetStatistics();
    NS_LOG_INFO("Attack packets: " << attackStats["packetCount"]);
    
    Simulator::Destroy();
    
    return 0;
}
