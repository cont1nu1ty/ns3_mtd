/**
 * \defgroup mtd-benchmark MTD Benchmark Module
 * 
 * \brief Moving Target Defense Performance Measurement Platform for NS-3
 * 
 * This module provides a comprehensive framework for evaluating DDoS defense
 * algorithms using proxy-switching Moving Target Defense (MTD) strategies.
 * 
 * \section mtd-benchmark-overview Overview
 * 
 * The MTD-benchmark module implements a standardized, extensible platform for:
 * - Quantitative evaluation of defense algorithms (availability, latency, switching cost, attack blocking rate)
 * - Closed-loop attack-defense verification with dynamic attack traffic adjustment
 * - Reproducible experiments through standardized logging and snapshot export
 * - Modular testing framework with decoupled algorithm implementation
 * 
 * \section mtd-benchmark-architecture Architecture
 * 
 * The module consists of the following components:
 * 
 * \subsection mtd-benchmark-detection Attack Detection Layer
 * 
 * Multi-level detection with:
 * - **LocalDetector**: Per-proxy threshold-based detection (fast, initial filtering)
 * - **CrossAgentDetector**: Cross-proxy comparative analysis (statistical anomaly detection)
 * - **GlobalDetector**: ML-based global pattern detection (higher accuracy, higher latency)
 * 
 * \subsection mtd-benchmark-scoring Score Manager
 * 
 * Risk scoring with the formula:
 * ```
 * score = α·rate + β·anomaly + γ·persistence + δ·feedback
 * ```
 * 
 * Features:
 * - Time decay: `score_t+1 = score_t * exp(-λΔt) + new_obs_weight`
 * - Risk level mapping (Low/Medium/High/Critical) for shuffle frequency control
 * 
 * \subsection mtd-benchmark-domain Domain Manager
 * 
 * Logical domain operations:
 * - User-domain association queries
 * - Cross-domain user migration
 * - Dynamic domain split/merge based on load thresholds
 * - Multiple assignment strategies (consistent hash, traffic clustering, load-aware)
 * 
 * \subsection mtd-benchmark-shuffle Shuffle Controller
 * 
 * MTD proxy switching with:
 * - Multiple shuffle strategies (random, score-driven, round-robin, attacker-avoid)
 * - Adaptive frequency: `f_domain = clamp(f_base * (1 + k·risk_factor), f_min, f_max)`
 * - Session affinity support for long-lived connections
 * 
 * \subsection mtd-benchmark-attack Attack Generator
 * 
 * Dynamic attack simulation:
 * - Multiple attack types (DOS, SYN flood, UDP flood, HTTP flood, port scan)
 * - Adaptive behavior (reacts to defense events)
 * - Cooldown mechanism to prevent oscillation
 * 
 * \subsection mtd-benchmark-export Export API
 * 
 * Experiment data export:
 * - Complete experiment snapshot (topology, configuration, random seed)
 * - Traffic traces (JSON/CSV)
 * - Domain state and user distribution
 * - Shuffle and attack event logs
 * 
 * \subsection mtd-benchmark-eventbus Event Bus
 * 
 * Decoupled inter-module communication:
 * - Publish-subscribe pattern
 * - Event types: shuffle, domain operations, attack events, proxy switches
 * 
 * \section mtd-benchmark-usage Usage
 * 
 * Basic usage example:
 * 
 * ```cpp
 * #include "ns3/mtd-benchmark-module.h"
 * 
 * using namespace ns3::mtd;
 * 
 * // Create components
 * Ptr<EventBus> eventBus = EventBus::GetInstance();
 * Ptr<DomainManager> domainManager = CreateObject<DomainManager>();
 * Ptr<ScoreManager> scoreManager = CreateObject<ScoreManager>();
 * Ptr<ShuffleController> shuffleController = CreateObject<ShuffleController>();
 * 
 * // Configure
 * shuffleController->SetDomainManager(domainManager);
 * shuffleController->SetScoreManager(scoreManager);
 * shuffleController->SetEventBus(eventBus);
 * 
 * // Create domains
 * uint32_t domainId = domainManager->CreateDomain("TestDomain");
 * domainManager->AddProxy(domainId, 1);
 * domainManager->AddUser(domainId, 100);
 * 
 * // Start periodic shuffling
 * shuffleController->SetFrequency(domainId, 10.0);
 * shuffleController->StartPeriodicShuffle(domainId);
 * ```
 * 
 * \section mtd-benchmark-integration Integration with Defense Algorithms
 * 
 * To integrate a custom defense algorithm:
 * 
 * 1. Subscribe to relevant events via EventBus
 * 2. Implement detection logic using LocalDetector/CrossAgentDetector
 * 3. Update user scores via ScoreManager
 * 4. Trigger shuffles via ShuffleController with custom strategies
 * 
 * Example:
 * 
 * ```cpp
 * // Subscribe to attack detection events
 * eventBus->Subscribe(EventType::THRESHOLD_EXCEEDED, 
 *     [shuffleController](const MtdEvent& event) {
 *         uint32_t domainId = std::stoul(event.metadata.at("domainId"));
 *         shuffleController->TriggerShuffle(domainId, ShuffleMode::SCORE_DRIVEN);
 *     });
 * ```
 * 
 * \section mtd-benchmark-compatibility Compatibility
 * 
 * This module is designed for compatibility with existing DDoS defense research,
 * particularly with the proactive-ddos-defender project structure.
 * 
 * Key compatible interfaces:
 * - Domain-based user grouping
 * - Score-based risk assessment
 * - Event-driven architecture
 * - Configurable shuffle strategies
 */
