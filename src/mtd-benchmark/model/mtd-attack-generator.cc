/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Attack Generator implementation
 */

#include "mtd-attack-generator.h"
#include "ns3/log.h"
#include "ns3/simulator.h"

#include <algorithm>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdAttackGenerator");

// ==================== AttackGenerator ====================

NS_OBJECT_ENSURE_REGISTERED(AttackGenerator);

TypeId
AttackGenerator::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::AttackGenerator")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<AttackGenerator>();
    return tid;
}

AttackGenerator::AttackGenerator()
    : m_behavior(AttackBehavior::STATIC),
      m_active(false),
      m_paused(false),
      m_eventSubscriptionId(0),
      m_lastCooldownEnd(0),
      m_cooldownPeriod(10.0),
      m_packetCount(0),
      m_byteCount(0),
      m_attackCount(0),
      m_nextCallbackId(1),
      m_roundRobinIdx(0)
{
    NS_LOG_FUNCTION(this);
    
    m_rng = CreateObject<UniformRandomVariable>();
}

AttackGenerator::~AttackGenerator()
{
    NS_LOG_FUNCTION(this);
    Stop();
}

void
AttackGenerator::Generate(const AttackParams& params)
{
    NS_LOG_FUNCTION(this);
    
    m_params = params;
    
    if (params.targetProxyId > 0)
    {
        AddTarget(params.targetProxyId);
    }
    
    for (uint32_t targetId : params.targetProxyIds)
    {
        AddTarget(targetId);
    }
    
    m_cooldownPeriod = params.cooldownPeriod;
}

void
AttackGenerator::Update(const AttackParams& params)
{
    NS_LOG_FUNCTION(this);
    
    m_params = params;
    
    // If active, the new parameters will take effect on next attack cycle
    NS_LOG_INFO("Attack parameters updated: rate=" << params.rate 
                << " pps, type=" << static_cast<int>(params.type));
}

uint32_t
AttackGenerator::SubscribeDefenseEvents(DefenseEventCallback callback)
{
    NS_LOG_FUNCTION(this);
    
    uint32_t id = m_nextCallbackId++;
    m_callbacks[id] = callback;
    return id;
}

void
AttackGenerator::UnsubscribeDefenseEvents(uint32_t subscriptionId)
{
    NS_LOG_FUNCTION(this << subscriptionId);
    m_callbacks.erase(subscriptionId);
}

void
AttackGenerator::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    
    // Unsubscribe from old event bus
    if (m_eventBus != nullptr && m_eventSubscriptionId > 0)
    {
        m_eventBus->Unsubscribe(m_eventSubscriptionId);
    }
    
    m_eventBus = eventBus;
    
    // Subscribe to defense events
    if (m_eventBus != nullptr && m_params.adaptToDefense)
    {
        m_eventSubscriptionId = m_eventBus->Subscribe(
            EventType::SHUFFLE_COMPLETED,
            MakeCallback(&AttackGenerator::OnDefenseEvent, this)
        );
        
        // Also subscribe to proxy switch events
        m_eventBus->Subscribe(
            EventType::PROXY_SWITCHED,
            MakeCallback(&AttackGenerator::OnDefenseEvent, this)
        );
    }
}

void
AttackGenerator::Start()
{
    NS_LOG_FUNCTION(this);
    
    if (m_active)
    {
        return;
    }
    
    m_active = true;
    m_paused = false;
    
    // Start attack
    PerformAttack();
    
    // Publish attack started event
    if (m_eventBus != nullptr)
    {
        MtdEvent event(EventType::ATTACK_STARTED, Simulator::Now().GetMilliSeconds());
        event.metadata["type"] = std::to_string(static_cast<int>(m_params.type));
        event.metadata["rate"] = std::to_string(m_params.rate);
        m_eventBus->Publish(event);
    }
    
    NS_LOG_INFO("Attack started with rate " << m_params.rate << " pps");
}

void
AttackGenerator::Stop()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_active)
    {
        return;
    }
    
    m_active = false;
    m_paused = false;
    
    Simulator::Cancel(m_attackEvent);
    
    // Publish attack stopped event
    if (m_eventBus != nullptr)
    {
        MtdEvent event(EventType::ATTACK_STOPPED, Simulator::Now().GetMilliSeconds());
        event.metadata["packetsGenerated"] = std::to_string(m_packetCount);
        event.metadata["bytesGenerated"] = std::to_string(m_byteCount);
        m_eventBus->Publish(event);
    }
    
    NS_LOG_INFO("Attack stopped. Total packets: " << m_packetCount);
}

void
AttackGenerator::Pause()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_active || m_paused)
    {
        return;
    }
    
    m_paused = true;
    Simulator::Cancel(m_attackEvent);
    
    NS_LOG_INFO("Attack paused");
}

void
AttackGenerator::Resume()
{
    NS_LOG_FUNCTION(this);
    
    if (!m_active || !m_paused)
    {
        return;
    }
    
    m_paused = false;
    PerformAttack();
    
    NS_LOG_INFO("Attack resumed");
}

bool
AttackGenerator::IsActive() const
{
    return m_active && !m_paused;
}

AttackParams
AttackGenerator::GetCurrentParams() const
{
    return m_params;
}

void
AttackGenerator::SetBehavior(AttackBehavior behavior)
{
    NS_LOG_FUNCTION(this << static_cast<int>(behavior));
    m_behavior = behavior;
}

AttackBehavior
AttackGenerator::GetBehavior() const
{
    return m_behavior;
}

void
AttackGenerator::SetCooldownPeriod(double seconds)
{
    NS_LOG_FUNCTION(this << seconds);
    m_cooldownPeriod = seconds;
    m_params.cooldownPeriod = seconds;
}

double
AttackGenerator::GetCooldownPeriod() const
{
    return m_cooldownPeriod;
}

bool
AttackGenerator::IsInCooldown() const
{
    uint64_t now = Simulator::Now().GetMilliSeconds();
    return now < m_lastCooldownEnd;
}

std::vector<AttackEvent>
AttackGenerator::GetAttackHistory() const
{
    return m_history;
}

std::map<std::string, double>
AttackGenerator::GetStatistics() const
{
    std::map<std::string, double> stats;
    stats["packetCount"] = static_cast<double>(m_packetCount);
    stats["byteCount"] = static_cast<double>(m_byteCount);
    stats["attackCount"] = static_cast<double>(m_attackCount);
    stats["isActive"] = m_active ? 1.0 : 0.0;
    stats["isPaused"] = m_paused ? 1.0 : 0.0;
    stats["targetCount"] = static_cast<double>(m_targets.size());
    stats["currentRate"] = m_params.rate;
    return stats;
}

void
AttackGenerator::SetTargetSelector(Callback<uint32_t, const std::vector<uint32_t>&> callback)
{
    NS_LOG_FUNCTION(this);
    m_targetSelector = callback;
}

void
AttackGenerator::AddTarget(uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << proxyId);
    
    if (std::find(m_targets.begin(), m_targets.end(), proxyId) == m_targets.end())
    {
        m_targets.push_back(proxyId);
    }
}

void
AttackGenerator::RemoveTarget(uint32_t proxyId)
{
    NS_LOG_FUNCTION(this << proxyId);
    
    auto it = std::find(m_targets.begin(), m_targets.end(), proxyId);
    if (it != m_targets.end())
    {
        m_targets.erase(it);
    }
}

void
AttackGenerator::SetTargets(const std::vector<uint32_t>& proxyIds)
{
    NS_LOG_FUNCTION(this);
    m_targets = proxyIds;
}

std::vector<uint32_t>
AttackGenerator::GetTargets() const
{
    return m_targets;
}

uint64_t
AttackGenerator::GetPacketCount() const
{
    return m_packetCount;
}

uint64_t
AttackGenerator::GetByteCount() const
{
    return m_byteCount;
}

void
AttackGenerator::PerformAttack()
{
    if (!m_active || m_paused)
    {
        return;
    }
    
    // Check cooldown
    if (IsInCooldown())
    {
        // Schedule next check after cooldown
        double remaining = (m_lastCooldownEnd - Simulator::Now().GetMilliSeconds()) / 1000.0;
        m_attackEvent = Simulator::Schedule(Seconds(remaining),
            &AttackGenerator::PerformAttack, this);
        return;
    }
    
    // Check if attack duration exceeded
    // (In a real implementation, we'd track total attack time)
    
    // Select target
    uint32_t target = SelectTarget();
    
    if (target > 0)
    {
        // Simulate generating packets
        m_packetCount++;
        m_byteCount += m_params.packetSize;
        
        RecordAttackEvent(target);
    }
    
    // Schedule next packet based on rate
    double interval = 1.0 / m_params.rate; // seconds per packet
    m_attackEvent = Simulator::Schedule(Seconds(interval),
        &AttackGenerator::PerformAttack, this);
}

void
AttackGenerator::OnDefenseEvent(const MtdEvent& event)
{
    NS_LOG_FUNCTION(this << static_cast<int>(event.type));
    
    // Notify registered callbacks
    for (const auto& pair : m_callbacks)
    {
        pair.second(event);
    }
    
    // Adapt if enabled
    if (m_params.adaptToDefense)
    {
        AdaptToDefense(event);
    }
}

void
AttackGenerator::AdaptToDefense(const MtdEvent& event)
{
    NS_LOG_FUNCTION(this << static_cast<int>(event.type));
    
    switch (m_behavior)
    {
        case AttackBehavior::ADAPTIVE:
        {
            // React to shuffle by entering cooldown and changing targets
            if (event.type == EventType::SHUFFLE_COMPLETED)
            {
                EnterCooldown();
                
                // Mark recent attack as triggering defense
                if (!m_history.empty())
                {
                    m_history.back().defenseTriggered = true;
                }
                
                NS_LOG_INFO("Defense detected, entering cooldown");
            }
            else if (event.type == EventType::PROXY_SWITCHED)
            {
                // Could adapt targeting based on proxy switch
                auto it = event.metadata.find("newProxy");
                if (it != event.metadata.end())
                {
                    uint32_t newProxy = std::stoul(it->second);
                    // Consider targeting the new proxy
                    AddTarget(newProxy);
                }
            }
            break;
        }
        
        case AttackBehavior::INTELLIGENT:
        {
            // More sophisticated adaptation
            // Could use ML model to predict best response
            if (event.type == EventType::SHUFFLE_COMPLETED)
            {
                // Reduce rate temporarily to avoid detection
                m_params.rate *= 0.7;
                EnterCooldown();
            }
            break;
        }
        
        case AttackBehavior::RANDOM_BURST:
        {
            // Random burst pattern
            double factor = m_rng->GetValue(0.5, 2.0);
            m_params.rate *= factor;
            break;
        }
        
        default:
            // Static behavior - no adaptation
            break;
    }
}

uint32_t
AttackGenerator::SelectTarget()
{
    if (m_targets.empty())
    {
        return 0;
    }
    
    // Use custom selector if provided
    if (!m_targetSelector.IsNull())
    {
        return m_targetSelector(m_targets);
    }
    
    // Default selection based on behavior
    switch (m_behavior)
    {
        case AttackBehavior::RANDOM_BURST:
        {
            std::size_t idx = m_rng->GetInteger(0, m_targets.size() - 1);
            return m_targets[idx];
        }
        
        case AttackBehavior::ADAPTIVE:
        case AttackBehavior::INTELLIGENT:
        {
            // Could use history to select least-defended target
            // For now, use round-robin
            uint32_t target = m_targets[m_roundRobinIdx % m_targets.size()];
            m_roundRobinIdx++;
            return target;
        }
        
        default:
            // Static - always attack first target
            return m_targets[0];
    }
}

void
AttackGenerator::RecordAttackEvent(uint32_t targetId)
{
    AttackEvent event;
    event.timestamp = Simulator::Now().GetMilliSeconds();
    event.type = m_params.type;
    event.targetProxyId = targetId;
    event.rate = m_params.rate;
    event.duration = m_params.duration;
    event.defenseTriggered = false;
    
    m_history.push_back(event);
    m_attackCount++;
    
    // Limit history size
    if (m_history.size() > 1000)
    {
        m_history.erase(m_history.begin());
    }
}

void
AttackGenerator::EnterCooldown()
{
    NS_LOG_FUNCTION(this);
    
    m_lastCooldownEnd = Simulator::Now().GetMilliSeconds() + 
                        static_cast<uint64_t>(m_cooldownPeriod * 1000);
}

// ==================== AttackCoordinator ====================

NS_OBJECT_ENSURE_REGISTERED(AttackCoordinator);

TypeId
AttackCoordinator::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::AttackCoordinator")
        .SetParent<Object>()
        .SetGroupName("MtdBenchmark")
        .AddConstructor<AttackCoordinator>();
    return tid;
}

AttackCoordinator::AttackCoordinator()
    : m_nextGeneratorId(1),
      m_staggerInterval(0.0)
{
    NS_LOG_FUNCTION(this);
}

AttackCoordinator::~AttackCoordinator()
{
    NS_LOG_FUNCTION(this);
    StopAll();
}

uint32_t
AttackCoordinator::AddGenerator(Ptr<AttackGenerator> generator)
{
    NS_LOG_FUNCTION(this);
    
    uint32_t id = m_nextGeneratorId++;
    m_generators[id] = generator;
    
    if (m_eventBus != nullptr)
    {
        generator->SetEventBus(m_eventBus);
    }
    
    return id;
}

void
AttackCoordinator::RemoveGenerator(uint32_t generatorId)
{
    NS_LOG_FUNCTION(this << generatorId);
    
    auto it = m_generators.find(generatorId);
    if (it != m_generators.end())
    {
        it->second->Stop();
        m_generators.erase(it);
    }
}

void
AttackCoordinator::StartAll()
{
    NS_LOG_FUNCTION(this);
    
    double delay = 0.0;
    for (auto& pair : m_generators)
    {
        if (m_staggerInterval > 0.0)
        {
            Simulator::Schedule(Seconds(delay), 
                &AttackGenerator::Start, pair.second);
            delay += m_staggerInterval;
        }
        else
        {
            pair.second->Start();
        }
    }
}

void
AttackCoordinator::StopAll()
{
    NS_LOG_FUNCTION(this);
    
    for (auto& pair : m_generators)
    {
        pair.second->Stop();
    }
}

void
AttackCoordinator::SetSynchronizedAttack(const AttackParams& params)
{
    NS_LOG_FUNCTION(this);
    
    for (auto& pair : m_generators)
    {
        pair.second->Generate(params);
    }
}

void
AttackCoordinator::SetStaggeredPattern(double interval)
{
    NS_LOG_FUNCTION(this << interval);
    m_staggerInterval = interval;
}

std::map<std::string, double>
AttackCoordinator::GetAggregateStats() const
{
    std::map<std::string, double> aggregate;
    aggregate["packetCount"] = 0.0;
    aggregate["byteCount"] = 0.0;
    aggregate["attackCount"] = 0.0;
    aggregate["activeGenerators"] = 0.0;
    
    for (const auto& pair : m_generators)
    {
        auto stats = pair.second->GetStatistics();
        aggregate["packetCount"] += stats["packetCount"];
        aggregate["byteCount"] += stats["byteCount"];
        aggregate["attackCount"] += stats["attackCount"];
        if (stats["isActive"] > 0)
        {
            aggregate["activeGenerators"]++;
        }
    }
    
    aggregate["totalGenerators"] = static_cast<double>(m_generators.size());
    
    return aggregate;
}

void
AttackCoordinator::SetEventBus(Ptr<EventBus> eventBus)
{
    NS_LOG_FUNCTION(this);
    
    m_eventBus = eventBus;
    
    for (auto& pair : m_generators)
    {
        pair.second->SetEventBus(eventBus);
    }
}

} // namespace mtd
} // namespace ns3
