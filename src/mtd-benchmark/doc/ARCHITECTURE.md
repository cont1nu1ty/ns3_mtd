# MTD-Benchmark Architecture Documentation

## Overview

MTD-Benchmark is an NS-3 module that provides a standardized, extensible platform for evaluating DDoS defense algorithms using proxy-switching Moving Target Defense (MTD) strategies. This document describes the project structure, component responsibilities, customization options, and how to configure and run defense algorithms.

## Table of Contents

1. [Project Structure](#project-structure)
2. [Component Overview](#component-overview)
3. [File Descriptions](#file-descriptions)
4. [Data Flow Architecture](#data-flow-architecture)
5. [User Customization Guide](#user-customization-guide)
6. [Configuration Reference](#configuration-reference)
7. [Running Simulations](#running-simulations)
8. [Integration Guide](#integration-guide)

---

## Project Structure

```
src/mtd-benchmark/
├── model/                          # Core component implementations
│   ├── mtd-common.h                # Common types, enums, and data structures
│   ├── mtd-benchmark-module.h      # Main module header (includes all components)
│   ├── mtd-event-bus.h/cc          # Event bus for inter-module communication
│   ├── mtd-detector.h/cc           # Attack detection layer (3 levels)
│   ├── mtd-score-manager.h/cc      # User risk scoring and classification
│   ├── mtd-domain-manager.h/cc     # Logical domain management
│   ├── mtd-shuffle-controller.h/cc # MTD proxy switching controller
│   ├── mtd-attack-generator.h/cc   # Attack traffic simulation
│   └── mtd-export-api.h/cc         # Data export for analysis
├── helper/                         # Simulation setup helpers
│   └── mtd-network-helper.h/cc     # Network topology creation
├── examples/                       # Example simulations
│   ├── mtd-benchmark-example.cc    # Basic component usage
│   └── mtd-basic-simulation.cc     # Complete network simulation
├── test/                           # Unit tests
│   └── mtd-benchmark-test-suite.cc # Test cases for all components
├── doc/                            # Documentation
│   ├── README.md                   # Quick start guide
│   ├── ARCHITECTURE.md             # This document
│   └── mtd-benchmark.h             # Doxygen documentation
├── wscript                         # waf build configuration
└── CMakeLists.txt                  # CMake build configuration
```

---

## Component Overview

### Core Components (model/)

| Component | Purpose | Key Classes |
|-----------|---------|-------------|
| **Event Bus** | Decoupled inter-module communication | `EventBus` |
| **Detector** | Multi-level attack detection | `LocalDetector`, `CrossAgentDetector`, `GlobalDetector` |
| **Score Manager** | User risk scoring | `ScoreManager` |
| **Domain Manager** | Logical domain operations | `DomainManager`, `MetricsApi` |
| **Shuffle Controller** | MTD proxy switching | `ShuffleController`, `TrafficDataApi` |
| **Attack Generator** | Attack traffic simulation | `AttackGenerator`, `AttackCoordinator` |
| **Export API** | Data export | `ExportApi` |

### Helper Components (helper/)

| Component | Purpose |
|-----------|---------|
| **Network Helper** | Creates standard MTD network topology with clients, proxies, servers, and attackers |

---

## File Descriptions

### Core Files

#### `mtd-common.h`
Defines all common types used across the module:
- **Enums**: `SwitchStrategy`, `AttackType`, `RiskLevel`, `EventType`, `NodeType`
- **Structures**: `MtdNode`, `Domain`, `TrafficStats`, `DetectionObservation`, `UserScore`, `DomainMetrics`, `AttackParams`, `ShuffleEvent`, `ProxyAssignment`, `ExperimentConfig`, `MtdEvent`

#### `mtd-event-bus.h/cc`
Provides publish-subscribe event system:
- `Publish(event)`: Broadcast events to subscribers
- `Subscribe(eventType, callback)`: Register for specific event types
- `SubscribeAll(callback)`: Register for all events
- Supports event history logging

#### `mtd-detector.h/cc`
Three-level detection hierarchy:

| Detector | Level | Method | Use Case |
|----------|-------|--------|----------|
| `LocalDetector` | Per-proxy | Threshold-based | Fast initial filtering |
| `CrossAgentDetector` | Cross-proxy | Z-score comparison | Distributed attack detection |
| `GlobalDetector` | Global | ML-based | Complex pattern detection |

#### `mtd-score-manager.h/cc`
Risk scoring with customizable algorithm:
- Default formula: `score = α·rate + β·anomaly + γ·persistence + δ·feedback`
- Time decay: `score_t+1 = score_t * exp(-λΔt) + new_obs_weight`
- Supports custom scoring and risk level callbacks

#### `mtd-domain-manager.h/cc`
Domain lifecycle management:
- Create/delete domains
- Add/remove users and proxies
- Dynamic split/merge based on load
- Multiple assignment strategies (consistent hash, load-aware, custom)

#### `mtd-shuffle-controller.h/cc`
MTD proxy switching:
- Multiple strategies: RANDOM, SCORE_DRIVEN, ROUND_ROBIN, ATTACKER_AVOID, CUSTOM
- Adaptive frequency based on risk
- Session affinity support

#### `mtd-attack-generator.h/cc`
Attack simulation:
- Attack types: DOS, SYN_FLOOD, UDP_FLOOD, HTTP_FLOOD, PROBE, PORT_SCAN
- Behavior modes: STATIC, ADAPTIVE, INTELLIGENT, RANDOM_BURST
- Defense-aware adaptation with cooldown

#### `mtd-export-api.h/cc`
Data export:
- Experiment snapshots (JSON)
- Traffic traces (CSV)
- Domain state, shuffle events, attack events

---

## Data Flow Architecture

```
┌─────────────────────────────────────────────────────────────────┐
│                         Event Bus                                │
│  (Publishes: SHUFFLE_*, DOMAIN_*, ATTACK_*, SCORE_*, etc.)      │
└─────────────────────────────────────────────────────────────────┘
         ▲                    ▲                    ▲
         │                    │                    │
         │ Subscribe          │ Subscribe          │ Subscribe
         │                    │                    │
┌────────┴────────┐  ┌────────┴────────┐  ┌───────┴─────────┐
│  Detection      │  │  Score          │  │  Attack         │
│  Layer          │  │  Manager        │  │  Generator      │
│                 │  │                 │  │                 │
│ LocalDetector   │  │ UpdateScore()   │  │ Subscribe to    │
│ CrossAgent      │──▶│ GetRiskLevel() │  │ defense events  │
│ GlobalDetector  │  │ Custom scoring  │  │ Adapt behavior  │
└────────┬────────┘  └────────┬────────┘  └─────────────────┘
         │                    │
         │ Observations       │ Risk Levels
         ▼                    ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Shuffle Controller                          │
│                                                                   │
│  • Receives detection observations and risk scores               │
│  • Triggers shuffles based on strategy                           │
│  • Supports custom shuffle callbacks                             │
│  • Manages proxy assignments                                     │
└─────────────────────────────────────────────────────────────────┘
         │
         │ Shuffle decisions
         ▼
┌─────────────────────────────────────────────────────────────────┐
│                      Domain Manager                              │
│                                                                   │
│  • Manages user-domain-proxy mappings                            │
│  • Handles domain split/merge                                    │
│  • Executes user migrations                                      │
└─────────────────────────────────────────────────────────────────┘
         │
         │ Metrics & Events
         ▼
┌─────────────────────────────────────────────────────────────────┐
│                        Export API                                │
│                                                                   │
│  • Records all events and metrics                                │
│  • Exports JSON/CSV for analysis                                 │
└─────────────────────────────────────────────────────────────────┘
```

---

## User Customization Guide

The platform supports three levels of customization:

### Level 1: Parameter Tuning

Adjust the default algorithm parameters without changing logic:

```cpp
// 1. Detection thresholds
DetectionThresholds thresholds;
thresholds.packetRateThreshold = 5000.0;    // packets/sec
thresholds.byteRateThreshold = 5000000.0;   // bytes/sec
thresholds.connectionThreshold = 500.0;     // connections/sec
thresholds.anomalyScoreThreshold = 0.6;     // 0-1 scale
localDetector->SetThresholds(thresholds);

// 2. Scoring weights (α, β, γ, δ, λ)
ScoreWeights weights;
weights.alpha = 0.4;   // Rate anomaly weight
weights.beta = 0.3;    // Pattern anomaly weight
weights.gamma = 0.15;  // Persistence factor weight
weights.delta = 0.15;  // Feedback weight
weights.lambda = 0.05; // Time decay factor
scoreManager->SetWeights(weights);

// 3. Risk level thresholds
RiskThresholds riskThresholds;
riskThresholds.lowMax = 0.25;    // Score <= 0.25 → LOW
riskThresholds.mediumMax = 0.55; // Score <= 0.55 → MEDIUM
riskThresholds.highMax = 0.80;   // Score <= 0.80 → HIGH
scoreManager->SetRiskThresholds(riskThresholds);

// 4. Shuffle configuration
ShuffleConfig shuffleConfig;
shuffleConfig.baseFrequency = 30.0;    // Base interval (seconds)
shuffleConfig.minFrequency = 5.0;      // Minimum interval
shuffleConfig.maxFrequency = 120.0;    // Maximum interval
shuffleConfig.riskFactor = 1.5;        // Risk multiplier
shuffleConfig.sessionAffinity = true;  // Preserve active sessions
shuffleConfig.batchSize = 50;          // Max users per shuffle
shuffleController->SetConfig(shuffleConfig);

// 5. Domain thresholds
DomainThresholds domainThresholds;
domainThresholds.splitThreshold = 0.8; // Split when load > 80%
domainThresholds.mergeThreshold = 0.2; // Merge when load < 20%
domainThresholds.minProxies = 2;       // Minimum proxies per domain
domainThresholds.maxUsers = 500;       // Maximum users per domain
domainManager->SetThresholds(domainThresholds);
```

### Level 2: Custom Callbacks

Replace specific algorithm components with custom logic:

```cpp
// Custom scoring algorithm
scoreManager->SetCustomScoreCallback(
    [](uint32_t userId, const DetectionObservation& obs, 
       double currentScore) -> double {
        // Your scoring logic here
        // Example: Exponential moving average
        double alpha = 0.3;
        double observedScore = obs.rateAnomaly * 0.6 + obs.patternAnomaly * 0.4;
        return alpha * observedScore + (1 - alpha) * currentScore;
    });

// Custom risk level classification
scoreManager->SetCustomRiskLevelCallback(
    [](uint32_t userId, double score) -> RiskLevel {
        // Your classification logic here
        // Example: User-specific thresholds
        if (userId < 100) {  // VIP users
            if (score > 0.95) return RiskLevel::CRITICAL;
            return RiskLevel::LOW;
        }
        // Standard users
        if (score > 0.8) return RiskLevel::CRITICAL;
        if (score > 0.6) return RiskLevel::HIGH;
        if (score > 0.3) return RiskLevel::MEDIUM;
        return RiskLevel::LOW;
    });

// Custom shuffle strategy
shuffleController->SetCustomStrategy(
    [](uint32_t userId, const std::vector<uint32_t>& proxies, 
       const UserScore& score) -> uint32_t {
        // Your proxy selection logic here
        // Example: Risk-based isolation
        if (score.riskLevel == RiskLevel::CRITICAL) {
            return proxies.back();  // Isolate to last proxy
        }
        if (score.riskLevel == RiskLevel::HIGH) {
            // Spread high-risk users across fewer proxies
            size_t idx = score.userId % (proxies.size() / 2);
            return proxies[idx];
        }
        // Normal distribution for low-risk users
        return proxies[score.userId % proxies.size()];
    });

// Custom domain assignment strategy
domainManager->SetCustomStrategy(
    [](uint32_t userId, const std::map<uint32_t, Domain>& domains) -> uint32_t {
        // Your domain assignment logic here
        // Example: Geography-based or load-based
        uint32_t bestDomain = 0;
        double minLoad = 1.0;
        for (const auto& [id, domain] : domains) {
            if (domain.loadFactor < minLoad) {
                minLoad = domain.loadFactor;
                bestDomain = id;
            }
        }
        return bestDomain;
    });
```

### Level 3: Component Extension

Create custom detector or controller classes:

```cpp
// Custom detector that inherits from LocalDetector
class MyCustomDetector : public LocalDetector {
public:
    DetectionObservation Analyze(uint32_t agentId) override {
        // Get base analysis
        DetectionObservation obs = LocalDetector::Analyze(agentId);
        
        // Add custom analysis
        obs.patternAnomaly = CustomPatternDetection(agentId);
        
        return obs;
    }
    
private:
    double CustomPatternDetection(uint32_t agentId) {
        // Your custom pattern detection logic
        return 0.0;
    }
};
```

---

## Configuration Reference

### Event Types

| EventType | Description | Metadata |
|-----------|-------------|----------|
| `SHUFFLE_TRIGGERED` | Shuffle started | domainId |
| `SHUFFLE_COMPLETED` | Shuffle finished | domainId, usersAffected, executionTime |
| `DOMAIN_SPLIT` | Domain split | domainId |
| `DOMAIN_MERGE` | Domains merged | domainId |
| `USER_MIGRATED` | User moved | userId, oldDomain, newDomain |
| `ATTACK_DETECTED` | Attack detected | agentId, attackType, confidence |
| `ATTACK_STARTED` | Attack began | type, rate |
| `ATTACK_STOPPED` | Attack ended | packetsGenerated, bytesGenerated |
| `PROXY_SWITCHED` | User switched proxy | userId, oldProxy, newProxy |
| `SCORE_UPDATED` | User score changed | userId, score, riskLevel |

### Attack Types

| AttackType | Description | Characteristics |
|------------|-------------|-----------------|
| `DOS` | Generic DoS | High packet rate |
| `SYN_FLOOD` | TCP SYN flood | High connection rate |
| `UDP_FLOOD` | UDP flood | High byte rate |
| `HTTP_FLOOD` | HTTP request flood | Application layer |
| `PROBE` | Network probe | Low rate, persistent |
| `PORT_SCAN` | Port scanning | Connection patterns |

### Shuffle Modes

| ShuffleMode | Description | Use Case |
|-------------|-------------|----------|
| `RANDOM` | Random proxy assignment | Baseline comparison |
| `SCORE_DRIVEN` | Based on risk scores | Risk-aware defense |
| `ROUND_ROBIN` | Sequential rotation | Predictable switching |
| `ATTACKER_AVOID` | Avoid attack targets | Active defense |
| `LOAD_BALANCED` | Balance proxy load | Performance focus |
| `CUSTOM` | User-defined callback | Custom algorithms |

### Risk Levels

| RiskLevel | Default Threshold | Typical Response |
|-----------|-------------------|------------------|
| `LOW` | score ≤ 0.3 | Normal operation |
| `MEDIUM` | score ≤ 0.6 | Increased monitoring |
| `HIGH` | score ≤ 0.85 | Accelerated shuffling |
| `CRITICAL` | score > 0.85 | Immediate isolation |

---

## Running Simulations

### Basic Simulation Setup

```cpp
#include "ns3/mtd-benchmark-module.h"
#include "ns3/mtd-network-helper.h"

using namespace ns3;
using namespace ns3::mtd;

int main() {
    // 1. Create core components
    Ptr<EventBus> eventBus = EventBus::GetInstance();
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    Ptr<AttackGenerator> attackGenerator = CreateObject<AttackGenerator>();
    Ptr<ExportApi> exportApi = CreateObject<ExportApi>();
    
    // 2. Connect components
    domainManager->SetEventBus(eventBus);
    scoreManager->SetEventBus(eventBus);
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    attackGenerator->SetEventBus(eventBus);
    
    // 3. Configure components (see Configuration Reference)
    // ...
    
    // 4. Create network topology
    MtdNetworkHelper networkHelper;
    TopologyConfig topoConfig;
    topoConfig.numClients = 100;
    topoConfig.numProxies = 10;
    topoConfig.numServers = 5;
    networkHelper.SetTopologyConfig(topoConfig);
    networkHelper.CreateTopology();
    networkHelper.InstallInternetStack();
    networkHelper.AssignIpAddresses();
    
    // 5. Initialize domains
    uint32_t domainId = domainManager->CreateDomain("MainDomain");
    for (int p = 0; p < 10; ++p) domainManager->AddProxy(domainId, p);
    for (int u = 0; u < 100; ++u) domainManager->AddUser(domainId, u);
    
    // 6. Start periodic shuffling
    shuffleController->SetFrequency(domainId, 30.0);
    shuffleController->StartPeriodicShuffle(domainId);
    
    // 7. Configure and start attack simulation
    AttackParams attackParams;
    attackParams.type = AttackType::UDP_FLOOD;
    attackParams.rate = 10000.0;
    attackParams.adaptToDefense = true;
    attackGenerator->Generate(attackParams);
    
    Simulator::Schedule(Seconds(10.0), &AttackGenerator::Start, attackGenerator);
    Simulator::Schedule(Seconds(50.0), &AttackGenerator::Stop, attackGenerator);
    
    // 8. Run simulation
    Simulator::Stop(Seconds(60.0));
    Simulator::Run();
    
    // 9. Export results
    exportApi->ExportExperimentSnapshot("results/experiment.json");
    exportApi->ExportShuffleEvents("results/shuffles.csv");
    exportApi->ExportAttackEvents("results/attacks.csv");
    
    Simulator::Destroy();
    return 0;
}
```

### Command Line Execution

```bash
# Build with waf
cd /path/to/ns-3
./waf configure --enable-examples
./waf build

# Run example
./waf --run "mtd-benchmark-example --clients=100 --proxies=10 --time=60"

# Run with logging
NS_LOG="MtdShuffleController:MtdScoreManager" ./waf --run mtd-benchmark-example
```

### Event-Driven Defense Algorithm

```cpp
// Subscribe to detection events
eventBus->Subscribe(EventType::THRESHOLD_EXCEEDED,
    [scoreManager, shuffleController, domainManager](const MtdEvent& event) {
        uint32_t agentId = std::stoul(event.metadata.at("agentId"));
        
        // Update risk scores for connected users
        std::vector<uint32_t> users = domainManager->GetDomainUsers(
            domainManager->GetDomain(agentId));
        
        DetectionObservation obs;
        obs.rateAnomaly = std::stod(event.metadata.at("rateAnomaly"));
        obs.patternAnomaly = std::stod(event.metadata.at("patternAnomaly"));
        
        for (uint32_t userId : users) {
            scoreManager->UpdateScore(userId, obs);
            
            // Check if shuffle needed
            if (scoreManager->GetRiskLevel(userId) >= RiskLevel::HIGH) {
                uint32_t domainId = domainManager->GetDomain(userId);
                shuffleController->TriggerShuffle(domainId, ShuffleMode::SCORE_DRIVEN);
                break;  // One shuffle per event
            }
        }
    });
```

---

## Integration Guide

### Integrating Custom Defense Algorithms

To integrate your own defense algorithm (e.g., from proactive-ddos-defender):

1. **Map your detection logic** to the DetectionObservation structure:
```cpp
DetectionObservation ConvertFromMyDetector(MyDetectorOutput output) {
    DetectionObservation obs;
    obs.rateAnomaly = output.trafficAnomaly;
    obs.patternAnomaly = output.behaviorAnomaly;
    obs.persistenceFactor = output.attackDuration / 60.0;
    obs.suspectedType = MapToAttackType(output.attackClass);
    obs.confidence = output.confidence;
    return obs;
}
```

2. **Register your scoring algorithm**:
```cpp
scoreManager->SetCustomScoreCallback(
    [this](uint32_t userId, const DetectionObservation& obs, 
           double currentScore) -> double {
        return myDefender->CalculateRiskScore(userId, obs);
    });
```

3. **Register your shuffle strategy**:
```cpp
shuffleController->SetCustomStrategy(
    [this](uint32_t userId, const std::vector<uint32_t>& proxies,
           const UserScore& score) -> uint32_t {
        return myDefender->SelectProxy(userId, proxies, score);
    });
```

4. **Subscribe to events for closed-loop operation**:
```cpp
eventBus->Subscribe(EventType::SHUFFLE_COMPLETED, 
    [this](const MtdEvent& event) {
        myDefender->OnShuffleComplete(event);
    });

eventBus->Subscribe(EventType::ATTACK_DETECTED,
    [this](const MtdEvent& event) {
        myDefender->OnAttackDetected(event);
    });
```

### Performance Metrics

The platform automatically tracks:
- Attack blocking rate
- Average switch latency
- Session interruption rate
- Detection accuracy (if ground truth provided)

Export and analyze with:
```cpp
auto summary = exportApi->GetPerformanceSummary();
std::cout << "Total shuffles: " << summary["totalShuffles"] << std::endl;
std::cout << "Success rate: " << summary["shuffleSuccessRate"] << std::endl;
std::cout << "Avg latency: " << summary["avgLatency"] << " ms" << std::endl;
```

---

## Appendix: Complete Example

See `examples/mtd-benchmark-example.cc` for a complete working example that demonstrates:
- All component initialization
- Custom algorithm integration
- Attack simulation
- Event handling
- Data export

For network-level simulation with actual NS-3 packet flow, see `examples/mtd-basic-simulation.cc`.
