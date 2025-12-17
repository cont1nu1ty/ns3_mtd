# MTD-Benchmark NS-3 Module

Moving Target Defense Performance Measurement Platform for NS-3 simulation.

## Overview

MTD-benchmark is an NS-3 module that provides a standardized, extensible platform for evaluating DDoS defense algorithms using proxy-switching Moving Target Defense (MTD) strategies.

## Features

- **Multi-level Attack Detection**: Local (per-proxy), Cross-agent (comparative), and Global (ML-based) detection
- **Risk Scoring System**: Time-decay scoring with configurable weights
- **Domain Management**: Dynamic domain split/merge/migration operations
- **Shuffle Controller**: Multiple MTD switching strategies with session affinity
- **Attack Simulation**: Adaptive attack generation with defense-aware behavior
- **Data Export**: JSON/CSV export for experiment reproducibility

## Installation

### Prerequisites

- NS-3 (version 3.35 or later)
- C++17 compatible compiler
- CMake 3.16+ (for CMake builds) or waf (for waf builds)

### Building with waf

```bash
# Copy module to ns-3 src directory
cp -r src/mtd-benchmark /path/to/ns-3/src/

# Configure and build
cd /path/to/ns-3
./waf configure --enable-examples --enable-tests
./waf build
```

### Building with CMake

```bash
cd /path/to/ns-3
cmake -B build -DCMAKE_BUILD_TYPE=Release -DNS3_EXAMPLES=ON -DNS3_TESTS=ON
cmake --build build
```

## Quick Start

```cpp
#include "ns3/mtd-benchmark-module.h"

using namespace ns3::mtd;

int main() {
    // Create core components
    Ptr<EventBus> eventBus = EventBus::GetInstance();
    Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
    Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
    Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
    
    // Connect components
    domainManager->SetEventBus(eventBus);
    shuffleController->SetDomainManager(domainManager);
    shuffleController->SetScoreManager(scoreManager);
    shuffleController->SetEventBus(eventBus);
    
    // Create a domain with proxies and users
    uint32_t domainId = domainManager->CreateDomain("MainDomain");
    for (int p = 0; p < 5; ++p) domainManager->AddProxy(domainId, p);
    for (int u = 0; u < 100; ++u) domainManager->AddUser(domainId, u);
    
    // Configure and start shuffling
    shuffleController->SetFrequency(domainId, 10.0); // Every 10 seconds
    shuffleController->StartPeriodicShuffle(domainId);
    
    // Run simulation...
    Simulator::Run();
    Simulator::Destroy();
    
    return 0;
}
```

## Architecture

### Core Components

| Component | Description |
|-----------|-------------|
| **EventBus** | Publish-subscribe event system for decoupled communication |
| **LocalDetector** | Per-proxy threshold-based attack detection |
| **CrossAgentDetector** | Cross-proxy comparative analysis |
| **GlobalDetector** | ML-based global pattern detection |
| **ScoreManager** | User risk scoring with time decay |
| **DomainManager** | Logical domain operations (split/merge/migrate) |
| **ShuffleController** | MTD proxy switching with multiple strategies |
| **AttackGenerator** | Adaptive attack simulation |
| **ExportApi** | Experiment data export |

### Shuffle Strategies

| Strategy | Description |
|----------|-------------|
| `RANDOM` | Random proxy assignment |
| `SCORE_DRIVEN` | Risk score-based assignment |
| `ROUND_ROBIN` | Sequential proxy rotation |
| `ATTACKER_AVOID` | Avoid suspected attack targets |
| `LOAD_BALANCED` | Balance load across proxies |
| `CUSTOM` | User-defined strategy callback |

### Attack Types

| Type | Description |
|------|-------------|
| `DOS` | Generic denial of service |
| `SYN_FLOOD` | TCP SYN flood attack |
| `UDP_FLOOD` | UDP flood attack |
| `HTTP_FLOOD` | HTTP request flood |
| `PORT_SCAN` | Port scanning reconnaissance |
| `PROBE` | Network probing |

## Integration

### Custom Defense Algorithm

```cpp
// 1. Create detection callback
eventBus->Subscribe(EventType::THRESHOLD_EXCEEDED,
    [scoreManager, shuffleController](const MtdEvent& event) {
        uint32_t userId = std::stoul(event.metadata.at("userId"));
        
        // Update risk score
        DetectionObservation obs;
        obs.patternAnomaly = 0.8;
        scoreManager->UpdateScore(userId, obs);
        
        // Check if shuffle needed
        if (scoreManager->GetRiskLevel(userId) >= RiskLevel::HIGH) {
            uint32_t domainId = std::stoul(event.metadata.at("domainId"));
            shuffleController->TriggerShuffle(domainId, ShuffleMode::SCORE_DRIVEN);
        }
    });
```

### Custom Shuffle Strategy

```cpp
shuffleController->SetCustomStrategy(
    [](uint32_t userId, const std::vector<uint32_t>& proxies, 
       const UserScore& score) -> uint32_t {
        // Implement custom selection logic
        if (score.riskLevel == RiskLevel::HIGH) {
            return proxies.back(); // High risk users go to last proxy
        }
        return proxies.front();
    });
```

### Custom Scoring Algorithm

You can define your own scoring algorithm to replace the default formula:

```cpp
// Set custom scoring callback
scoreManager->SetCustomScoreCallback(
    [](uint32_t userId, const DetectionObservation& obs, 
       double currentScore) -> double {
        // Implement your own scoring algorithm
        // Example: weighted combination with custom logic
        double myScore = 0.5 * obs.rateAnomaly + 
                         0.3 * obs.patternAnomaly + 
                         0.2 * currentScore;
        return myScore;
    });

// Set custom risk level classification
scoreManager->SetCustomRiskLevelCallback(
    [](uint32_t userId, double score) -> RiskLevel {
        // Implement your own risk classification
        if (score > 0.9) return RiskLevel::CRITICAL;
        if (score > 0.7) return RiskLevel::HIGH;
        if (score > 0.4) return RiskLevel::MEDIUM;
        return RiskLevel::LOW;
    });
```

### Configurable Parameters

Alternatively, you can customize the default algorithm's parameters:

```cpp
// Customize scoring weights (α, β, γ, δ, λ)
ScoreWeights weights;
weights.alpha = 0.4;   // Rate anomaly weight
weights.beta = 0.3;    // Pattern anomaly weight
weights.gamma = 0.15;  // Persistence factor weight
weights.delta = 0.15;  // Feedback weight
weights.lambda = 0.05; // Time decay factor
scoreManager->SetWeights(weights);

// Customize risk level thresholds
RiskThresholds thresholds;
thresholds.lowMax = 0.25;    // Score <= 0.25 → LOW
thresholds.mediumMax = 0.55; // Score <= 0.55 → MEDIUM
thresholds.highMax = 0.80;   // Score <= 0.80 → HIGH
scoreManager->SetRiskThresholds(thresholds);
```

## Data Export

Export experiment results for analysis:

```cpp
Ptr<ExportApi> exportApi = CreateObject<ExportApi>();
exportApi->SetDomainManager(domainManager);
exportApi->SetShuffleController(shuffleController);
exportApi->SetEventBus(eventBus);

// Export all data
exportApi->ExportExperimentSnapshot("experiment.json");
exportApi->ExportTrafficTrace("traffic.csv");
exportApi->ExportShuffleEvents("shuffles.csv");
exportApi->ExportAttackEvents("attacks.csv");
```

## Examples

See the `examples/` directory:
- `mtd-benchmark-example.cc` - Basic MTD simulation
- `mtd-basic-simulation.cc` - Complete network simulation with topology

## Testing

```bash
./waf --run "test-runner --suite=mtd-benchmark"
```

## License

This project is released under the GNU GPLv2 license (compatible with NS-3).

## Contributing

Contributions are welcome! Please submit pull requests with:
- Code following NS-3 coding style
- Unit tests for new functionality
- Documentation updates
