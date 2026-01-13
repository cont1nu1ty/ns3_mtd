/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Security tag for deterministic RCA attribution
 */

#include "mtd-security-tag.h"

#include "ns3/tag-buffer.h"

#include <ostream>

namespace ns3 {
namespace mtd {

NS_OBJECT_ENSURE_REGISTERED(MtdSecurityTag);

MtdSecurityTag::MtdSecurityTag() = default;

MtdSecurityTag::MtdSecurityTag(uint32_t epochId, uint32_t userId)
    : m_epochId(epochId), m_userId(userId)
{
}

TypeId
MtdSecurityTag::GetTypeId()
{
    static TypeId tid = TypeId("ns3::mtd::MtdSecurityTag")
                            .SetParent<Tag>()
                            .SetGroupName("MtdBenchmark")
                            .AddConstructor<MtdSecurityTag>();
    return tid;
}

TypeId
MtdSecurityTag::GetInstanceTypeId() const
{
    return GetTypeId();
}

uint32_t
MtdSecurityTag::GetSerializedSize() const
{
    return 8;
}

void
MtdSecurityTag::Serialize(TagBuffer i) const
{
    i.WriteU32(m_epochId);
    i.WriteU32(m_userId);
}

void
MtdSecurityTag::Deserialize(TagBuffer i)
{
    m_epochId = i.ReadU32();
    m_userId = i.ReadU32();
}

void
MtdSecurityTag::Print(std::ostream& os) const
{
    os << "epochId=" << m_epochId << ", userId=" << m_userId;
}

void
MtdSecurityTag::SetEpochId(uint32_t epochId)
{
    m_epochId = epochId;
}

uint32_t
MtdSecurityTag::GetEpochId() const
{
    return m_epochId;
}

void
MtdSecurityTag::SetUserId(uint32_t userId)
{
    m_userId = userId;
}

uint32_t
MtdSecurityTag::GetUserId() const
{
    return m_userId;
}

} // namespace mtd
} // namespace ns3
