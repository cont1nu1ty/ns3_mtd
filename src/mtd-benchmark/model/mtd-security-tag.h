/* -*- Mode:C++; c-file-style:"gnu"; indent-tabs-mode:nil; -*- */
/*
 * MTD-Benchmark: Security tag for deterministic RCA attribution
 */

#ifndef MTD_SECURITY_TAG_H
#define MTD_SECURITY_TAG_H

#include "ns3/tag.h"
#include "ns3/type-id.h"

#include <cstdint>

namespace ns3 {
namespace mtd {

/**
 * \brief Packet tag carrying security metadata for RCA.
 *
 * Carries the current defense epoch and the sender userId.
 */
class MtdSecurityTag : public Tag
{
  public:
    MtdSecurityTag();
    MtdSecurityTag(uint32_t epochId, uint32_t userId);

    static TypeId GetTypeId();
    TypeId GetInstanceTypeId() const override;

    uint32_t GetSerializedSize() const override;
    void Serialize(TagBuffer i) const override;
    void Deserialize(TagBuffer i) override;
    void Print(std::ostream& os) const override;

    void SetEpochId(uint32_t epochId);
    uint32_t GetEpochId() const;

    void SetUserId(uint32_t userId);
    uint32_t GetUserId() const;

  private:
    uint32_t m_epochId{0};
    uint32_t m_userId{0};
};

} // namespace mtd
} // namespace ns3

#endif // MTD_SECURITY_TAG_H
