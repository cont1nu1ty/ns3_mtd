/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
#ifndef MTD_ATTACK_GENERATOR_H
#define MTD_ATTACK_GENERATOR_H

#include "ns3/mtd-common.h"
#include "ns3/mtd-traffic-helper.h"
#include "ns3/mtd-network-helper.h"
#include "ns3/mtd-event-bus.h"

#include "ns3/object.h"
#include "ns3/nstime.h"
#include "ns3/event-id.h"

#include <cstdint>
#include <map>
#include <vector>
#include <sstream>

namespace ns3 {
namespace mtd {

/**
 * \brief Convert AttackType enum to human-readable string
 */
inline std::string AttackTypeToString(AttackType type)
{
    switch (type)
    {
        case AttackType::NONE:         return "NONE";
        case AttackType::DOS:          return "DOS";
        case AttackType::PROBE:        return "PROBE";
        case AttackType::PORT_SCAN:    return "PORT_SCAN";
        case AttackType::ROUTE_MONITOR: return "ROUTE_MONITOR";
        case AttackType::SYN_FLOOD:    return "SYN_FLOOD";
        case AttackType::UDP_FLOOD:    return "UDP_FLOOD";
        case AttackType::HTTP_FLOOD:   return "HTTP_FLOOD";
        default:                       return "UNKNOWN";
    }
}

/**
 * \ingroup mtd
 * \brief Attack Traffic Generator (Attack Logic Module)
 *
 * Responsibilities:
 * 1. Manage botnet nodes (Attacker Nodes)
 * 2. Schedule attack start/stop
 * 3. Translate high-level attack intent (AttackType) to low-level traffic (TrafficHelper Primitive)
 * 4. Publish Ground Truth events (ATTACK_STARTED/STOPPED) with full AttackRecord data
 */
class AttackGenerator : public Object
{
public:
    static TypeId GetTypeId();
    
    AttackGenerator();
    ~AttackGenerator() override;

    /**
     * \brief Ground Truth record for attack lifecycle
     * 
     * Contains all configuration and runtime data for a single attack instance.
     * Published via EventBus for logging and analysis.
     */
    struct AttackRecord {
        // === Basic Info ===
        uint64_t attackId{0};       ///< Unique identifier for correlating START/STOP events
        uint64_t startTime{0};      ///< Start timestamp (ms)
        uint64_t endTime{0};        ///< End timestamp (ms), filled on Stop()
    
        // === Attack Configuration ===
        AttackType type{AttackType::NONE};
        uint32_t targetProxyId{0};
        double ratePps{0.0};        ///< Packets per second
        uint32_t packetSize{512};   ///< Bytes per packet (critical for bandwidth calculation)
        uint32_t attackerCount{0};  ///< Number of attacker nodes participating
        std::vector<uint32_t> targetProxyIds;  ///< Multi-target support

        // === Statistics ===
        double durationPlanned{0.0};  ///< Configured duration
        double durationActual{0.0};   ///< Actual runtime (may be truncated by manual Stop)
        uint64_t packetsSent{0};      ///< Estimated packets sent
        uint64_t bytesSent{0};        ///< Estimated bytes sent
        
        // === Interaction State ===
        bool defenseTriggered{false}; ///< Whether defense was triggered (feedback from ScoreManager)
        std::string stopReason;       ///< Why the attack stopped ("duration", "manual", "defense")

        /**
         * \brief Serialize to JSON string for EventBus logging
         * \return JSON-formatted string with all ground truth data
         */
        std::string ToJson() const
        {
            std::ostringstream ss;
            ss << std::fixed;
            ss << "{";
            ss << "\"attackId\":" << attackId << ",";
            ss << "\"type\":\"" << AttackTypeToString(type) << "\",";
            ss << "\"typeId\":" << static_cast<int>(type) << ",";
            ss << "\"targetProxyId\":" << targetProxyId << ",";
            ss << "\"ratePps\":" << ratePps << ",";
            ss << "\"packetSize\":" << packetSize << ",";
            ss << "\"attackerCount\":" << attackerCount << ",";
            ss << "\"startTime\":" << startTime << ",";
            ss << "\"endTime\":" << endTime << ",";
            ss << "\"durationPlanned\":" << durationPlanned << ",";
            ss << "\"durationActual\":" << durationActual << ",";
            ss << "\"packetsSent\":" << packetsSent << ",";
            ss << "\"bytesSent\":" << bytesSent << ",";
            ss << "\"defenseTriggered\":" << (defenseTriggered ? "true" : "false");
            if (!stopReason.empty())
            {
                ss << ",\"stopReason\":\"" << stopReason << "\"";
            }
            ss << "}";
            return ss.str();
        }
        
        /**
         * \brief Calculate bandwidth in Mbps
         */
        double GetBandwidthMbps() const
        {
            return (ratePps * packetSize * 8.0) / 1000000.0;
        }
    };
    
    using AttackHistory = std::vector<AttackRecord>;

    // === Dependency Injection ===
    void SetTrafficHelper(Ptr<MtdTrafficHelper> trafficHelper);
    void SetNetworkHelper(Ptr<MtdNetworkHelper> networkHelper);
    void SetEventBus(Ptr<EventBus> eventBus);

    /**
     * \brief Configure attack parameters
     * \param params Attack configuration
     */
    void Configure(const AttackParams& params);

    /**
     * \brief Start attack immediately
     * \return true if successfully started
     * 
     * Publishes ATTACK_STARTED event with full Ground Truth data.
     */
    bool Start();

    /**
     * \brief Stop current attack
     * \param reason Optional reason for stopping ("manual", "defense", etc.)
     * 
     * Publishes ATTACK_STOPPED event with full Ground Truth data.
     */
    void Stop(const std::string& reason = "manual");

    /**
     * \brief Check if attack is active
     */
    bool IsActive() const;

    /**
     * \brief Get attack statistics
     */
    std::map<std::string, double> GetStatistics() const;

    /**
     * \brief Get attack history
     */
    const AttackHistory& GetAttackHistory() const;
    
    /**
     * \brief Get current attack record (if active)
     */
    const AttackRecord& GetCurrentRecord() const;
    
    /**
     * \brief Mark defense as triggered (called by external defense system)
     */
    void MarkDefenseTriggered();

private:
    // Dependencies
    Ptr<MtdTrafficHelper> m_trafficHelper;
    Ptr<MtdNetworkHelper> m_networkHelper;
    Ptr<EventBus> m_eventBus;

    // Configuration & State
    AttackParams m_params;
    bool m_isActive{false};
    Time m_attackStartTime;
    uint64_t m_totalPacketsSent{0};
    uint64_t m_totalBytesSent{0};
    
    // Current attack tracking
    AttackRecord m_currentRecord;
    uint64_t m_nextAttackId{1};
    
    // Active flow handles for cleanup
    std::vector<MtdTrafficHelper::FlowHandle> m_activeFlows;
    
    // Auto-stop timer
    EventId m_stopEvent;

    // Attack history
    AttackHistory m_attackHistory;

    // Internal helpers
    MtdTrafficHelper::StatelessTransport GetTransportProfile() const;
    void PublishGroundTruthEvent(EventType type);
};

} // namespace mtd
} // namespace ns3

#endif // MTD_ATTACK_GENERATOR_H
