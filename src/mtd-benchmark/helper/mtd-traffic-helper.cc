/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Traffic Helper implementation
 */

#include "mtd-traffic-helper.h"
#include "mtd-network-helper.h"

#include "ns3/log.h"
#include "ns3/simulator.h"
#include "ns3/udp-socket-factory.h"
#include "ns3/tcp-socket-factory.h"
#include "ns3/socket.h"
#include "ns3/packet.h"
#include "ns3/inet-socket-address.h"
#include "ns3/packet-sink-helper.h"
#include "ns3/udp-echo-helper.h"
#include "ns3/bulk-send-helper.h"
#include "ns3/on-off-helper.h"
#include "ns3/type-id.h"
#include "ns3/names.h"
#include "ns3/uinteger.h"
#include "ns3/string.h"
#include "ns3/boolean.h"
#include "ns3/double.h"

#include <fstream>
#include <iomanip>
#include <string>
#include <utility>

namespace ns3 {
namespace mtd {

NS_LOG_COMPONENT_DEFINE("MtdTrafficHelper");
NS_OBJECT_ENSURE_REGISTERED(MtdTrafficHelper);

TypeId
MtdTrafficHelper::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::MtdTrafficHelper")
                            .SetParent<Object>()
                            .SetGroupName("MTD");
    return tid;
}

MtdTrafficHelper::MtdTrafficHelper()
    : m_netHelper(nullptr),
      m_flowIdCounter(1)
{
    NS_LOG_FUNCTION(this);
}

MtdTrafficHelper::~MtdTrafficHelper()
{
    NS_LOG_FUNCTION(this);
    // Cleanup flows
    for (auto& pair : m_flowMap)
    {
        if (pair.second.app && pair.second.isActive)
        {
            pair.second.app->SetStopTime(Simulator::Now());
        }
    }
    m_flowMap.clear();
    m_userFlowIndex.clear();
}

void
MtdTrafficHelper::SetNetworkContext(Ptr<MtdNetworkHelper> netHelper)
{
    NS_LOG_FUNCTION(this << netHelper);
    m_netHelper = netHelper;
}

MtdTrafficHelper::FlowHandle
MtdTrafficHelper::GetNextFlowId()
{
    return m_flowIdCounter++;
}

MtdTrafficHelper::FlowHandle
MtdTrafficHelper::CreateStatelessHighRateFlow(Ptr<Node> sourceNode,
                                              ProxyId targetProxy,
                                              DataRate rate,
                                              StatelessTransport profile)
{
    NS_LOG_FUNCTION(this << sourceNode << targetProxy << rate << profile);

    if (!m_netHelper)
    {
        NS_LOG_ERROR("Network context not set");
        return 0;
    }

    Ipv4Address targetIp = m_netHelper->GetServiceIp(targetProxy);
    if (targetIp == Ipv4Address::GetZero())
    {
        NS_LOG_ERROR("Invalid target proxy IP for proxy " << targetProxy);
        return 0;
    }

    FlowHandle flowId = GetNextFlowId();
    Ptr<Application> app;

    if (profile == STATELESS_UDP)
    {
        // Use OnOffApplication for UDP high-rate flow
        OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address());
        onOffHelper.SetAttribute("DataRate", DataRateValue(rate));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(1024));
        onOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(targetIp, 9))); // Echo port
        app = onOffHelper.Install(sourceNode).Get(0);
    }
    else if (profile == STATELESS_TCP_SYN)
    {
        // For SYN flood, we can use a custom or simplified approach
        // For now, use OnOff with TCP
        OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
        onOffHelper.SetAttribute("DataRate", DataRateValue(rate));
        onOffHelper.SetAttribute("PacketSize", UintegerValue(64)); // Small SYN-like packets
        onOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(targetIp, 80)));
        app = onOffHelper.Install(sourceNode).Get(0);
    }

    if (app)
    {
        app->SetStartTime(Seconds(0.0));
        app->SetStopTime(Seconds(0.0)); // Will be set later if needed

        FlowEntity entity = {flowId, USER_ID_NONE, targetProxy, app, true, Simulator::Now(), Time::Max()};
        m_flowMap[flowId] = entity;
        // No user index for USER_ID_NONE
    }

    return flowId;
}

std::vector<MtdTrafficHelper::FlowHandle>
MtdTrafficHelper::CreateLongLivedTcpConnections(Ptr<Node> sourceNode,
                                                ProxyId targetProxy,
                                                uint32_t concurrency,
                                                Time connectionLifetime)
{
    NS_LOG_FUNCTION(this << sourceNode << targetProxy << concurrency << connectionLifetime);

    std::vector<FlowHandle> handles;

    if (!m_netHelper)
    {
        NS_LOG_ERROR("Network context not set");
        return handles;
    }

    Ipv4Address targetIp = m_netHelper->GetServiceIp(targetProxy);
    if (targetIp == Ipv4Address::GetZero())
    {
        NS_LOG_ERROR("Invalid target proxy IP for proxy " << targetProxy);
        return handles;
    }

    for (uint32_t i = 0; i < concurrency; ++i)
    {
        FlowHandle flowId = GetNextFlowId();

        // Use BulkSendApplication for long-lived TCP
        BulkSendHelper bulkSend("ns3::TcpSocketFactory", Address());
        bulkSend.SetAttribute("Remote", AddressValue(InetSocketAddress(targetIp, 80)));
        bulkSend.SetAttribute("SendSize", UintegerValue(0)); // No data, just connection
        ApplicationContainer apps = bulkSend.Install(sourceNode);
        Ptr<Application> app = apps.Get(0);

        if (app)
        {
            app->SetStartTime(Seconds(0.0));
            app->SetStopTime(connectionLifetime);

            FlowEntity entity = {flowId, USER_ID_NONE, targetProxy, app, true, Simulator::Now(), Simulator::Now() + connectionLifetime};
            m_flowMap[flowId] = entity;
            handles.push_back(flowId);
        }
    }

    return handles;
}

MtdTrafficHelper::FlowHandle
MtdTrafficHelper::CreatePeriodicTcpFlow(UserId userId,
                                        Ptr<Node> userNode,
                                        ProxyId targetProxy,
                                        DataRate dataRate,
                                        uint32_t packetSize,
                                        Time interval)
{
    NS_LOG_FUNCTION(this << userId << userNode << targetProxy << dataRate << packetSize << interval);

    if (!m_netHelper)
    {
        NS_LOG_ERROR("Network context not set");
        return 0;
    }

    Ipv4Address targetIp = m_netHelper->GetServiceIp(targetProxy);
    if (targetIp == Ipv4Address::GetZero())
    {
        NS_LOG_ERROR("Invalid target proxy IP for proxy " << targetProxy);
        return 0;
    }

    FlowHandle flowId = GetNextFlowId();

    // Use OnOffApplication for periodic TCP flow
    OnOffHelper onOffHelper("ns3::TcpSocketFactory", Address());
    onOffHelper.SetAttribute("DataRate", DataRateValue(dataRate));
    onOffHelper.SetAttribute("PacketSize", UintegerValue(packetSize));
    onOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(targetIp, 80)));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.1]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(interval.GetSeconds()) + "]"));
    ApplicationContainer apps = onOffHelper.Install(userNode);
    Ptr<Application> app = apps.Get(0);

    if (app)
    {
        app->SetStartTime(Seconds(0.0));
        app->SetStopTime(Seconds(0.0)); // Indefinite

        FlowEntity entity = {flowId, userId, targetProxy, app, true, Simulator::Now(), Time::Max()};
        m_flowMap[flowId] = entity;
        m_userFlowIndex[userId].push_back(flowId);
    }

    return flowId;
}

MtdTrafficHelper::FlowHandle
MtdTrafficHelper::CreateLowRatePresenceFlow(UserId userId,
                                            Ptr<Node> userNode,
                                            ProxyId targetProxy,
                                            Time interval)
{
    NS_LOG_FUNCTION(this << userId << userNode << targetProxy << interval);

    if (!m_netHelper)
    {
        NS_LOG_ERROR("Network context not set");
        return 0;
    }

    Ipv4Address targetIp = m_netHelper->GetServiceIp(targetProxy);
    if (targetIp == Ipv4Address::GetZero())
    {
        NS_LOG_ERROR("Invalid target proxy IP for proxy " << targetProxy);
        return 0;
    }

    FlowHandle flowId = GetNextFlowId();

    // Use OnOffApplication with very low rate
    OnOffHelper onOffHelper("ns3::UdpSocketFactory", Address());
    onOffHelper.SetAttribute("DataRate", DataRateValue(DataRate("1bps"))); // Very low
    onOffHelper.SetAttribute("PacketSize", UintegerValue(64));
    onOffHelper.SetAttribute("Remote", AddressValue(InetSocketAddress(targetIp, 9)));
    onOffHelper.SetAttribute("OnTime", StringValue("ns3::ConstantRandomVariable[Constant=0.001]"));
    onOffHelper.SetAttribute("OffTime", StringValue("ns3::ConstantRandomVariable[Constant=" + std::to_string(interval.GetSeconds()) + "]"));
    ApplicationContainer apps = onOffHelper.Install(userNode);
    Ptr<Application> app = apps.Get(0);

    if (app)
    {
        app->SetStartTime(Seconds(0.0));
        app->SetStopTime(Seconds(0.0)); // Indefinite

        FlowEntity entity = {flowId, userId, targetProxy, app, true, Simulator::Now(), Time::Max()};
        m_flowMap[flowId] = entity;
        m_userFlowIndex[userId].push_back(flowId);
    }

    return flowId;
}

void
MtdTrafficHelper::TerminateFlow(FlowHandle flow)
{
    NS_LOG_FUNCTION(this << flow);

    auto it = m_flowMap.find(flow);
    if (it == m_flowMap.end())
    {
        NS_LOG_WARN("Flow " << flow << " not found");
        return;
    }

    FlowEntity& entity = it->second;
    if (entity.isActive && entity.app)
    {
        entity.app->SetStopTime(Simulator::Now());
        entity.isActive = false;
        entity.stopTime = Simulator::Now();
    }
}

void
MtdTrafficHelper::TerminateFlowsByUser(UserId userId)
{
    NS_LOG_FUNCTION(this << userId);

    auto userIt = m_userFlowIndex.find(userId);
    if (userIt == m_userFlowIndex.end())
    {
        return;
    }

    for (FlowHandle flow : userIt->second)
    {
        TerminateFlow(flow);
    }

    m_userFlowIndex.erase(userIt);
}

std::vector<MtdTrafficHelper::FlowHandle>
MtdTrafficHelper::GetFlowsByUser(UserId userId) const
{
    NS_LOG_FUNCTION(this << userId);

    auto it = m_userFlowIndex.find(userId);
    if (it != m_userFlowIndex.end())
    {
        return it->second;
    }
    return {};
}

MtdTrafficHelper::ProxyId
MtdTrafficHelper::GetTargetProxy(FlowHandle flow) const
{
    NS_LOG_FUNCTION(this << flow);

    auto it = m_flowMap.find(flow);
    if (it != m_flowMap.end())
    {
        return it->second.targetProxyId;
    }
    return 0; // Invalid
}

} // namespace mtd
} // namespace ns3
