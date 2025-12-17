/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Moving Target Defense Performance Measurement Platform
 * 
 * This module provides a proxy-switching MTD network architecture for
 * evaluating DDoS defense algorithms in NS-3 simulations.
 */

#ifndef MTD_COMMON_H
#define MTD_COMMON_H

#include "ns3/callback.h"

#include <cstdint>
#include <string>
#include <vector>
#include <map>

namespace ns3 {
namespace mtd {

/**
 * \brief Enumeration of MTD switching strategies
 */
enum class SwitchStrategy {
    PERIODIC,     ///< Fixed interval switching
    RANDOM,       ///< Random interval switching
    ADAPTIVE,     ///< Risk-based adaptive switching
    MANUAL        ///< Manually triggered switching
};

/**
 * \brief Enumeration of attack types for detection and simulation
 */
enum class AttackType {
    NONE,           ///< No attack
    DOS,            ///< Denial of Service flood attack
    PROBE,          ///< Network probe/reconnaissance
    PORT_SCAN,      ///< Port scanning attack
    ROUTE_MONITOR,  ///< Route monitoring attack
    SYN_FLOOD,      ///< SYN flood attack
    UDP_FLOOD,      ///< UDP flood attack
    HTTP_FLOOD      ///< HTTP flood attack
};

/**
 * \brief Risk levels for user scoring
 */
enum class RiskLevel {
    LOW,
    MEDIUM,
    HIGH,
    CRITICAL
};

/**
 * \brief Event types for the event bus
 */
enum class EventType {
    SHUFFLE_TRIGGERED,      ///< Shuffle operation started
    SHUFFLE_COMPLETED,      ///< Shuffle operation completed
    DOMAIN_SPLIT,           ///< Domain split operation
    DOMAIN_MERGE,           ///< Domain merge operation
    USER_MIGRATED,          ///< User migrated between domains
    ATTACK_DETECTED,        ///< Attack detected
    ATTACK_STARTED,         ///< Attack simulation started
    ATTACK_STOPPED,         ///< Attack simulation stopped
    PROXY_SWITCHED,         ///< Proxy assignment changed
    THRESHOLD_EXCEEDED,     ///< Detection threshold exceeded
    SCORE_UPDATED           ///< User score updated
};

/**
 * \brief Node types in the MTD network
 */
enum class NodeType {
    CLIENT,
    PROXY,
    SERVER,
    ATTACKER,
    CONTROLLER
};

/**
 * \brief Structure representing a network node in the MTD system
 */
struct MtdNode {
    uint32_t nodeId;
    NodeType type;
    std::string ipAddress;
    uint32_t domainId;
    bool isActive;
    
    MtdNode() : nodeId(0), type(NodeType::CLIENT), domainId(0), isActive(true) {}
    MtdNode(uint32_t id, NodeType t) : nodeId(id), type(t), domainId(0), isActive(true) {}
};

/**
 * \brief Structure representing a domain (logical grouping)
 */
struct Domain {
    uint32_t domainId;
    std::string name;
    std::vector<uint32_t> proxyIds;
    std::vector<uint32_t> userIds;
    double loadFactor;
    double shuffleFrequency;
    
    Domain() : domainId(0), loadFactor(0.0), shuffleFrequency(1.0) {}
    Domain(uint32_t id, const std::string& n) : domainId(id), name(n), loadFactor(0.0), shuffleFrequency(1.0) {}
};

/**
 * \brief Traffic statistics structure
 */
struct TrafficStats {
    uint64_t packetsIn;
    uint64_t packetsOut;
    uint64_t bytesIn;
    uint64_t bytesOut;
    double packetRate;
    double byteRate;
    uint32_t activeConnections;
    double avgLatency;
    
    TrafficStats() : packetsIn(0), packetsOut(0), bytesIn(0), bytesOut(0),
                     packetRate(0.0), byteRate(0.0), activeConnections(0), avgLatency(0.0) {}
};

/**
 * \brief Attack detection observation
 */
struct DetectionObservation {
    double rateAnomaly;
    double connectionAnomaly;
    double patternAnomaly;
    double persistenceFactor;
    AttackType suspectedType;
    double confidence;
    uint64_t timestamp;
    
    DetectionObservation() : rateAnomaly(0.0), connectionAnomaly(0.0), patternAnomaly(0.0),
                             persistenceFactor(0.0), suspectedType(AttackType::NONE),
                             confidence(0.0), timestamp(0) {}
};

/**
 * \brief User score record
 */
struct UserScore {
    uint32_t userId;
    double currentScore;
    RiskLevel riskLevel;
    std::vector<DetectionObservation> recentObservations;
    uint64_t lastUpdateTime;
    
    UserScore() : userId(0), currentScore(0.0), riskLevel(RiskLevel::LOW), lastUpdateTime(0) {}
    UserScore(uint32_t id) : userId(id), currentScore(0.0), riskLevel(RiskLevel::LOW), lastUpdateTime(0) {}
};

/**
 * \brief Domain metrics for monitoring
 */
struct DomainMetrics {
    uint32_t domainId;
    double throughput;
    double avgLatency;
    uint32_t activeConnections;
    uint32_t userCount;
    uint32_t proxyCount;
    double loadFactor;
    std::map<RiskLevel, uint32_t> scoreDistribution;
    
    DomainMetrics() : domainId(0), throughput(0.0), avgLatency(0.0),
                      activeConnections(0), userCount(0), proxyCount(0), loadFactor(0.0) {}
};

/**
 * \brief Attack parameters for the attack generator
 */
struct AttackParams {
    AttackType type;
    double rate;                    ///< Packets per second
    uint32_t targetProxyId;
    std::vector<uint32_t> targetProxyIds;
    uint32_t packetSize;
    double duration;
    bool adaptToDefense;
    double cooldownPeriod;
    
    AttackParams() : type(AttackType::DOS), rate(1000.0), targetProxyId(0),
                     packetSize(512), duration(60.0), adaptToDefense(true), cooldownPeriod(10.0) {}
};

/**
 * \brief Shuffle event record
 */
struct ShuffleEvent {
    uint32_t domainId;
    uint64_t timestamp;
    SwitchStrategy strategy;
    uint32_t usersAffected;
    double executionTime;
    bool success;
    std::string reason;
    
    ShuffleEvent() : domainId(0), timestamp(0), strategy(SwitchStrategy::PERIODIC),
                     usersAffected(0), executionTime(0.0), success(false) {}
};

/**
 * \brief Proxy assignment record
 */
struct ProxyAssignment {
    uint32_t userId;
    uint32_t oldProxyId;
    uint32_t newProxyId;
    uint64_t timestamp;
    bool sessionPreserved;
    
    ProxyAssignment() : userId(0), oldProxyId(0), newProxyId(0), timestamp(0), sessionPreserved(false) {}
};

/**
 * \brief Experiment configuration
 */
struct ExperimentConfig {
    std::string experimentId;
    uint32_t randomSeed;
    double simulationDuration;
    uint32_t numClients;
    uint32_t numProxies;
    uint32_t numDomains;
    uint32_t numAttackers;
    SwitchStrategy defaultStrategy;
    double defaultShuffleFrequency;
    std::map<std::string, double> parameters;
    
    ExperimentConfig() : randomSeed(1), simulationDuration(300.0), numClients(100),
                         numProxies(10), numDomains(3), numAttackers(1),
                         defaultStrategy(SwitchStrategy::ADAPTIVE), defaultShuffleFrequency(30.0) {}
};

/**
 * \brief Base event structure for the event bus
 */
struct MtdEvent {
    EventType type;
    uint64_t timestamp;
    uint32_t sourceNodeId;
    std::map<std::string, std::string> metadata;
    
    MtdEvent() : type(EventType::SHUFFLE_TRIGGERED), timestamp(0), sourceNodeId(0) {}
    MtdEvent(EventType t, uint64_t ts) : type(t), timestamp(ts), sourceNodeId(0) {}
    
    virtual ~MtdEvent() = default;
};

// Event callback type using ns3::Callback
typedef Callback<void, const MtdEvent&> EventCallback;

// Type aliases for clarity
using NodeId = uint32_t;
using DomainId = uint32_t;
using UserId = uint32_t;
using ProxyId = uint32_t;
using Timestamp = uint64_t;

} // namespace mtd
} // namespace ns3

#endif // MTD_COMMON_H
