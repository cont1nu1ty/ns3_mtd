/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Pybind11 Python Bindings
 * 
 * This file provides Python bindings for the MTD-Benchmark module,
 * enabling Python scripts to implement custom defense algorithms.
 */

#include <pybind11/pybind11.h>
#include <pybind11/stl.h>
#include <pybind11/functional.h>
#include <pybind11/chrono.h>

#include "../model/mtd-common.h"
#include "../model/mtd-event-bus.h"
#include "../model/mtd-detector.h"
#include "../model/mtd-score-manager.h"
#include "../model/mtd-domain-manager.h"
#include "../model/mtd-shuffle-controller.h"
#include "../model/mtd-attack-generator.h"
#include "../model/mtd-export-api.h"
#include "../model/mtd-python-interface.h"

namespace py = pybind11;
using namespace ns3::mtd;

/**
 * Python module definition for mtd_benchmark
 */
PYBIND11_MODULE(mtd_benchmark, m) {
    m.doc() = R"pbdoc(
        MTD-Benchmark Python Bindings
        -----------------------------
        
        This module provides Python bindings for the NS-3 MTD-Benchmark module,
        enabling researchers to implement and test defense algorithms in Python.
        
        Main Components:
        - SimulationState: Current simulation state data
        - DefenseDecision: Defense action to execute
        - PythonAlgorithmBridge: Bridge between Python and NS-3
        - SimulationContext: Simplified simulation interaction API
        
        Example Usage:
            import mtd_benchmark as mtd
            
            # Define custom scoring algorithm
            def my_score_calculator(user_id, observation, current_score):
                return 0.6 * observation.rate_anomaly + 0.4 * current_score
            
            # Register with bridge
            bridge.register_score_calculator(my_score_calculator)
    )pbdoc";

    // ==================== Enums ====================
    
    py::enum_<AttackType>(m, "AttackType", "Types of network attacks")
        .value("NONE", AttackType::NONE)
        .value("DOS", AttackType::DOS)
        .value("PROBE", AttackType::PROBE)
        .value("PORT_SCAN", AttackType::PORT_SCAN)
        .value("ROUTE_MONITOR", AttackType::ROUTE_MONITOR)
        .value("SYN_FLOOD", AttackType::SYN_FLOOD)
        .value("UDP_FLOOD", AttackType::UDP_FLOOD)
        .value("HTTP_FLOOD", AttackType::HTTP_FLOOD)
        .export_values();
    
    py::enum_<RiskLevel>(m, "RiskLevel", "User risk levels")
        .value("LOW", RiskLevel::LOW)
        .value("MEDIUM", RiskLevel::MEDIUM)
        .value("HIGH", RiskLevel::HIGH)
        .value("CRITICAL", RiskLevel::CRITICAL)
        .export_values();
    
    py::enum_<SwitchStrategy>(m, "SwitchStrategy", "MTD switching strategies")
        .value("PERIODIC", SwitchStrategy::PERIODIC)
        .value("RANDOM", SwitchStrategy::RANDOM)
        .value("ADAPTIVE", SwitchStrategy::ADAPTIVE)
        .value("MANUAL", SwitchStrategy::MANUAL)
        .export_values();
    
    py::enum_<ShuffleMode>(m, "ShuffleMode", "Shuffle operation modes")
        .value("RANDOM", ShuffleMode::RANDOM)
        .value("SCORE_DRIVEN", ShuffleMode::SCORE_DRIVEN)
        .value("ROUND_ROBIN", ShuffleMode::ROUND_ROBIN)
        .value("ATTACKER_AVOID", ShuffleMode::ATTACKER_AVOID)
        .value("LOAD_BALANCED", ShuffleMode::LOAD_BALANCED)
        .value("CUSTOM", ShuffleMode::CUSTOM)
        .export_values();
    
    py::enum_<EventType>(m, "EventType", "Event types for event bus")
        .value("SHUFFLE_TRIGGERED", EventType::SHUFFLE_TRIGGERED)
        .value("SHUFFLE_COMPLETED", EventType::SHUFFLE_COMPLETED)
        .value("DOMAIN_SPLIT", EventType::DOMAIN_SPLIT)
        .value("DOMAIN_MERGE", EventType::DOMAIN_MERGE)
        .value("USER_MIGRATED", EventType::USER_MIGRATED)
        .value("ATTACK_DETECTED", EventType::ATTACK_DETECTED)
        .value("ATTACK_STARTED", EventType::ATTACK_STARTED)
        .value("ATTACK_STOPPED", EventType::ATTACK_STOPPED)
        .value("PROXY_SWITCHED", EventType::PROXY_SWITCHED)
        .value("THRESHOLD_EXCEEDED", EventType::THRESHOLD_EXCEEDED)
        .value("SCORE_UPDATED", EventType::SCORE_UPDATED)
        .export_values();
    
    py::enum_<NodeType>(m, "NodeType", "Network node types")
        .value("CLIENT", NodeType::CLIENT)
        .value("PROXY", NodeType::PROXY)
        .value("SERVER", NodeType::SERVER)
        .value("ATTACKER", NodeType::ATTACKER)
        .value("CONTROLLER", NodeType::CONTROLLER)
        .export_values();
    
    py::enum_<DomainStrategy>(m, "DomainStrategy", "Domain assignment strategies")
        .value("CONSISTENT_HASH", DomainStrategy::CONSISTENT_HASH)
        .value("TRAFFIC_CLUSTER", DomainStrategy::TRAFFIC_CLUSTER)
        .value("LOAD_AWARE", DomainStrategy::LOAD_AWARE)
        .value("GEOGRAPHIC", DomainStrategy::GEOGRAPHIC)
        .value("CUSTOM", DomainStrategy::CUSTOM)
        .export_values();
    
    // ==================== Data Structures ====================
    
    py::class_<TrafficStats>(m, "TrafficStats", "Traffic statistics for a node")
        .def(py::init<>())
        .def_readwrite("packets_in", &TrafficStats::packetsIn)
        .def_readwrite("packets_out", &TrafficStats::packetsOut)
        .def_readwrite("bytes_in", &TrafficStats::bytesIn)
        .def_readwrite("bytes_out", &TrafficStats::bytesOut)
        .def_readwrite("packet_rate", &TrafficStats::packetRate)
        .def_readwrite("byte_rate", &TrafficStats::byteRate)
        .def_readwrite("active_connections", &TrafficStats::activeConnections)
        .def_readwrite("avg_latency", &TrafficStats::avgLatency)
        .def("__repr__", [](const TrafficStats& s) {
            return "<TrafficStats packet_rate=" + std::to_string(s.packetRate) + 
                   " byte_rate=" + std::to_string(s.byteRate) + ">";
        });
    
    py::class_<DetectionObservation>(m, "DetectionObservation", "Attack detection observation")
        .def(py::init<>())
        .def_readwrite("rate_anomaly", &DetectionObservation::rateAnomaly)
        .def_readwrite("connection_anomaly", &DetectionObservation::connectionAnomaly)
        .def_readwrite("pattern_anomaly", &DetectionObservation::patternAnomaly)
        .def_readwrite("persistence_factor", &DetectionObservation::persistenceFactor)
        .def_readwrite("suspected_type", &DetectionObservation::suspectedType)
        .def_readwrite("confidence", &DetectionObservation::confidence)
        .def_readwrite("timestamp", &DetectionObservation::timestamp)
        .def("__repr__", [](const DetectionObservation& o) {
            return "<DetectionObservation rate=" + std::to_string(o.rateAnomaly) + 
                   " pattern=" + std::to_string(o.patternAnomaly) + 
                   " confidence=" + std::to_string(o.confidence) + ">";
        });
    
    py::class_<UserScore>(m, "UserScore", "User risk score record")
        .def(py::init<>())
        .def(py::init<uint32_t>())
        .def_readwrite("user_id", &UserScore::userId)
        .def_readwrite("current_score", &UserScore::currentScore)
        .def_readwrite("risk_level", &UserScore::riskLevel)
        .def_readwrite("last_update_time", &UserScore::lastUpdateTime)
        .def("__repr__", [](const UserScore& s) {
            return "<UserScore id=" + std::to_string(s.userId) + 
                   " score=" + std::to_string(s.currentScore) + ">";
        });
    
    py::class_<Domain>(m, "Domain", "Logical domain grouping")
        .def(py::init<>())
        .def(py::init<uint32_t, const std::string&>())
        .def_readwrite("domain_id", &Domain::domainId)
        .def_readwrite("name", &Domain::name)
        .def_readwrite("proxy_ids", &Domain::proxyIds)
        .def_readwrite("user_ids", &Domain::userIds)
        .def_readwrite("load_factor", &Domain::loadFactor)
        .def_readwrite("shuffle_frequency", &Domain::shuffleFrequency)
        .def("__repr__", [](const Domain& d) {
            return "<Domain id=" + std::to_string(d.domainId) + 
                   " name='" + d.name + "' users=" + std::to_string(d.userIds.size()) + ">";
        });
    
    py::class_<DomainMetrics>(m, "DomainMetrics", "Domain performance metrics")
        .def(py::init<>())
        .def_readwrite("domain_id", &DomainMetrics::domainId)
        .def_readwrite("throughput", &DomainMetrics::throughput)
        .def_readwrite("avg_latency", &DomainMetrics::avgLatency)
        .def_readwrite("active_connections", &DomainMetrics::activeConnections)
        .def_readwrite("user_count", &DomainMetrics::userCount)
        .def_readwrite("proxy_count", &DomainMetrics::proxyCount)
        .def_readwrite("load_factor", &DomainMetrics::loadFactor)
        .def_readwrite("score_distribution", &DomainMetrics::scoreDistribution);
    
    py::class_<MtdEvent>(m, "MtdEvent", "Base event structure")
        .def(py::init<>())
        .def(py::init<EventType, uint64_t>())
        .def_readwrite("type", &MtdEvent::type)
        .def_readwrite("timestamp", &MtdEvent::timestamp)
        .def_readwrite("source_node_id", &MtdEvent::sourceNodeId)
        .def_readwrite("metadata", &MtdEvent::metadata)
        .def("__repr__", [](const MtdEvent& e) {
            return "<MtdEvent type=" + std::to_string(static_cast<int>(e.type)) + 
                   " time=" + std::to_string(e.timestamp) + ">";
        });
    
    py::class_<ShuffleEvent>(m, "ShuffleEvent", "Shuffle event record")
        .def(py::init<>())
        .def_readwrite("domain_id", &ShuffleEvent::domainId)
        .def_readwrite("timestamp", &ShuffleEvent::timestamp)
        .def_readwrite("strategy", &ShuffleEvent::strategy)
        .def_readwrite("users_affected", &ShuffleEvent::usersAffected)
        .def_readwrite("execution_time", &ShuffleEvent::executionTime)
        .def_readwrite("success", &ShuffleEvent::success)
        .def_readwrite("reason", &ShuffleEvent::reason);
    
    py::class_<ScoreWeights>(m, "ScoreWeights", "Scoring algorithm weights")
        .def(py::init<>())
        .def_readwrite("alpha", &ScoreWeights::alpha, "Rate anomaly weight")
        .def_readwrite("beta", &ScoreWeights::beta, "Pattern anomaly weight")
        .def_readwrite("gamma", &ScoreWeights::gamma, "Persistence factor weight")
        .def_readwrite("delta", &ScoreWeights::delta, "Feedback weight")
        .def_readwrite("lambda_", &ScoreWeights::lambda, "Time decay factor");
    
    py::class_<RiskThresholds>(m, "RiskThresholds", "Risk level thresholds")
        .def(py::init<>())
        .def_readwrite("low_max", &RiskThresholds::lowMax)
        .def_readwrite("medium_max", &RiskThresholds::mediumMax)
        .def_readwrite("high_max", &RiskThresholds::highMax);
    
    py::class_<DetectionThresholds>(m, "DetectionThresholds", "Detection thresholds")
        .def(py::init<>())
        .def_readwrite("packet_rate_threshold", &DetectionThresholds::packetRateThreshold)
        .def_readwrite("byte_rate_threshold", &DetectionThresholds::byteRateThreshold)
        .def_readwrite("connection_threshold", &DetectionThresholds::connectionThreshold)
        .def_readwrite("anomaly_score_threshold", &DetectionThresholds::anomalyScoreThreshold);
    
    py::class_<ShuffleConfig>(m, "ShuffleConfig", "Shuffle controller configuration")
        .def(py::init<>())
        .def_readwrite("base_frequency", &ShuffleConfig::baseFrequency)
        .def_readwrite("min_frequency", &ShuffleConfig::minFrequency)
        .def_readwrite("max_frequency", &ShuffleConfig::maxFrequency)
        .def_readwrite("risk_factor", &ShuffleConfig::riskFactor)
        .def_readwrite("session_affinity", &ShuffleConfig::sessionAffinity)
        .def_readwrite("session_timeout", &ShuffleConfig::sessionTimeout)
        .def_readwrite("batch_size", &ShuffleConfig::batchSize);
    
    py::class_<DomainThresholds>(m, "DomainThresholds", "Domain thresholds")
        .def(py::init<>())
        .def_readwrite("split_threshold", &DomainThresholds::splitThreshold)
        .def_readwrite("merge_threshold", &DomainThresholds::mergeThreshold)
        .def_readwrite("min_proxies", &DomainThresholds::minProxies)
        .def_readwrite("max_proxies", &DomainThresholds::maxProxies)
        .def_readwrite("min_users", &DomainThresholds::minUsers)
        .def_readwrite("max_users", &DomainThresholds::maxUsers);
    
    py::class_<ExperimentConfig>(m, "ExperimentConfig", "Experiment configuration")
        .def(py::init<>())
        .def_readwrite("experiment_id", &ExperimentConfig::experimentId)
        .def_readwrite("random_seed", &ExperimentConfig::randomSeed)
        .def_readwrite("simulation_duration", &ExperimentConfig::simulationDuration)
        .def_readwrite("num_clients", &ExperimentConfig::numClients)
        .def_readwrite("num_proxies", &ExperimentConfig::numProxies)
        .def_readwrite("num_domains", &ExperimentConfig::numDomains)
        .def_readwrite("num_attackers", &ExperimentConfig::numAttackers)
        .def_readwrite("default_strategy", &ExperimentConfig::defaultStrategy)
        .def_readwrite("default_shuffle_frequency", &ExperimentConfig::defaultShuffleFrequency)
        .def_readwrite("parameters", &ExperimentConfig::parameters);
    
    // ==================== Python Interface Structures ====================
    
    py::class_<SimulationState>(m, "SimulationState", "Complete simulation state snapshot")
        .def(py::init<>())
        .def_readwrite("current_time", &SimulationState::currentTime)
        .def_readwrite("domains", &SimulationState::domains)
        .def_readwrite("user_scores", &SimulationState::userScores)
        .def_readwrite("proxy_stats", &SimulationState::proxyStats)
        .def_readwrite("observations", &SimulationState::observations)
        .def_readwrite("recent_events", &SimulationState::recentEvents)
        .def("get_time_seconds", [](const SimulationState& s) {
            return s.currentTime / 1e9;
        }, "Get current time in seconds");
    
    py::enum_<DefenseDecision::ActionType>(m, "ActionType", "Defense action types")
        .value("NO_ACTION", DefenseDecision::ActionType::NO_ACTION)
        .value("TRIGGER_SHUFFLE", DefenseDecision::ActionType::TRIGGER_SHUFFLE)
        .value("MIGRATE_USER", DefenseDecision::ActionType::MIGRATE_USER)
        .value("SPLIT_DOMAIN", DefenseDecision::ActionType::SPLIT_DOMAIN)
        .value("MERGE_DOMAINS", DefenseDecision::ActionType::MERGE_DOMAINS)
        .value("UPDATE_SCORE", DefenseDecision::ActionType::UPDATE_SCORE)
        .value("CHANGE_FREQUENCY", DefenseDecision::ActionType::CHANGE_FREQUENCY)
        .value("CUSTOM", DefenseDecision::ActionType::CUSTOM)
        .export_values();
    
    py::class_<DefenseDecision>(m, "DefenseDecision", "Defense decision to execute")
        .def(py::init<>())
        .def_readwrite("action", &DefenseDecision::action)
        .def_readwrite("target_domain_id", &DefenseDecision::targetDomainId)
        .def_readwrite("target_user_id", &DefenseDecision::targetUserId)
        .def_readwrite("target_proxy_id", &DefenseDecision::targetProxyId)
        .def_readwrite("secondary_domain_id", &DefenseDecision::secondaryDomainId)
        .def_readwrite("new_score", &DefenseDecision::newScore)
        .def_readwrite("new_frequency", &DefenseDecision::newFrequency)
        .def_readwrite("shuffle_mode", &DefenseDecision::shuffleMode)
        .def_readwrite("custom_params", &DefenseDecision::customParams)
        .def_readwrite("reason", &DefenseDecision::reason)
        .def_static("trigger_shuffle", [](uint32_t domainId, ShuffleMode mode, 
                                          const std::string& reason = "") {
            DefenseDecision d;
            d.action = DefenseDecision::ActionType::TRIGGER_SHUFFLE;
            d.targetDomainId = domainId;
            d.shuffleMode = mode;
            d.reason = reason;
            return d;
        }, "Create a shuffle trigger decision",
           py::arg("domain_id"), py::arg("mode") = ShuffleMode::RANDOM, 
           py::arg("reason") = "")
        .def_static("migrate_user", [](uint32_t userId, uint32_t domainId,
                                       const std::string& reason = "") {
            DefenseDecision d;
            d.action = DefenseDecision::ActionType::MIGRATE_USER;
            d.targetUserId = userId;
            d.targetDomainId = domainId;
            d.reason = reason;
            return d;
        }, "Create a user migration decision")
        .def_static("update_score", [](uint32_t userId, double score,
                                       const std::string& reason = "") {
            DefenseDecision d;
            d.action = DefenseDecision::ActionType::UPDATE_SCORE;
            d.targetUserId = userId;
            d.newScore = score;
            d.reason = reason;
            return d;
        }, "Create a score update decision")
        .def_static("change_frequency", [](uint32_t domainId, double frequency,
                                           const std::string& reason = "") {
            DefenseDecision d;
            d.action = DefenseDecision::ActionType::CHANGE_FREQUENCY;
            d.targetDomainId = domainId;
            d.newFrequency = frequency;
            d.reason = reason;
            return d;
        }, "Create a frequency change decision")
        .def_static("no_action", []() {
            DefenseDecision d;
            d.action = DefenseDecision::ActionType::NO_ACTION;
            return d;
        }, "Create a no-action decision");
    
    py::class_<PythonAlgorithmConfig>(m, "PythonAlgorithmConfig", "Python algorithm configuration")
        .def(py::init<>())
        .def_readwrite("algorithm_name", &PythonAlgorithmConfig::algorithmName)
        .def_readwrite("module_path", &PythonAlgorithmConfig::modulePath)
        .def_readwrite("class_name", &PythonAlgorithmConfig::className)
        .def_readwrite("evaluation_interval", &PythonAlgorithmConfig::evaluationInterval)
        .def_readwrite("enable_parallel", &PythonAlgorithmConfig::enableParallel)
        .def_readwrite("max_decisions_per_eval", &PythonAlgorithmConfig::maxDecisionsPerEval)
        .def_readwrite("parameters", &PythonAlgorithmConfig::parameters);
    
    // ==================== Helper Functions ====================
    
    m.def("shuffle_mode_to_string", &ShuffleModeToString, "Convert ShuffleMode to string");
    m.def("string_to_shuffle_mode", &StringToShuffleMode, "Convert string to ShuffleMode");
    m.def("risk_level_to_string", &RiskLevelToString, "Convert RiskLevel to string");
    m.def("string_to_risk_level", &StringToRiskLevel, "Convert string to RiskLevel");
    m.def("attack_type_to_string", &AttackTypeToString, "Convert AttackType to string");
    m.def("string_to_attack_type", &StringToAttackType, "Convert string to AttackType");
    
    // ==================== Version Info ====================
    
    m.attr("__version__") = "1.0.0";
    m.attr("__ns3_version__") = "3.35+";
}
