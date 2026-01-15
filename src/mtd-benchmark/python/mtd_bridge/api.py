"""High-level API for MTD-Benchmark Python algorithms.

This module provides convenient wrappers for common operations,
making it easy for algorithm developers to interact with the MTD system.
"""

from __future__ import annotations

from typing import Optional, Sequence
import json

from .types import BridgeContext, Algorithm


class MtdApi:
    """High-level API wrapper for MTD operations."""

    def __init__(self, ctx: BridgeContext):
        self.ctx = ctx

    # ==================== Domain Operations ====================

    def get_all_domains(self) -> list[int]:
        """Get all domain IDs."""
        if self.ctx.domain_manager is None:
            return []
        return list(self.ctx.domain_manager.GetAllDomainIds())

    def get_domain_users(self, domain_id: int) -> list[int]:
        """Get all user IDs in a domain."""
        if self.ctx.domain_manager is None:
            return []
        return list(self.ctx.domain_manager.GetDomainUsers(domain_id))

    def get_domain_proxies(self, domain_id: int) -> list[int]:
        """Get all proxy IDs in a domain."""
        if self.ctx.domain_manager is None:
            return []
        return list(self.ctx.domain_manager.GetDomainProxies(domain_id))

    def split_domain(self, domain_id: int) -> Optional[int]:
        """Split a domain. Returns new domain ID or None on failure."""
        if self.ctx.domain_manager is None:
            return None
        return self.ctx.domain_manager.SplitDomain(domain_id)

    def merge_domains(self, domain_id_a: int, domain_id_b: int) -> Optional[int]:
        """Merge two domains. Returns merged domain ID or None on failure."""
        if self.ctx.domain_manager is None:
            return None
        return self.ctx.domain_manager.MergeDomain(domain_id_a, domain_id_b)

    # ==================== Shuffle Operations ====================

    def trigger_shuffle(
        self,
        domain_id: int,
        mode: str = "SCORE_DRIVEN",
        reason: str = "algorithm_decision",
    ) -> bool:
        """Trigger a shuffle for a domain.

        Args:
            domain_id: Domain to shuffle
            mode: Shuffle mode ("RANDOM", "SCORE_DRIVEN", "ROUND_ROBIN", etc.)
            reason: Reason for shuffle (for logging)

        Returns:
            True if shuffle was triggered
        """
        if self.ctx.shuffle_controller is None:
            return False

        # Map string to enum
        mode_map = {
            "RANDOM": self.ctx.ns.mtd.ShuffleMode.RANDOM,
            "SCORE_DRIVEN": self.ctx.ns.mtd.ShuffleMode.SCORE_DRIVEN,
            "ROUND_ROBIN": self.ctx.ns.mtd.ShuffleMode.ROUND_ROBIN,
            "ATTACKER_AVOID": self.ctx.ns.mtd.ShuffleMode.ATTACKER_AVOID,
            "LOAD_BALANCED": self.ctx.ns.mtd.ShuffleMode.LOAD_BALANCED,
        }
        shuffle_mode = mode_map.get(mode.upper(), self.ctx.ns.mtd.ShuffleMode.SCORE_DRIVEN)

        try:
            self.ctx.shuffle_controller.TriggerShuffle(domain_id, shuffle_mode, reason)
            return True
        except Exception:
            return False

    def get_users_on_proxy(self, proxy_id: int) -> list[int]:
        """Get all user IDs currently assigned to a proxy."""
        if self.ctx.shuffle_controller is None:
            return []
        return list(self.ctx.shuffle_controller.GetUsersOnProxy(proxy_id))

    def get_proxy_assignment(self, user_id: int) -> Optional[int]:
        """Get the proxy ID assigned to a user."""
        if self.ctx.shuffle_controller is None:
            return None
        return self.ctx.shuffle_controller.GetProxyAssignment(user_id)

    # ==================== Score Operations ====================

    def get_user_score(self, user_id: int) -> Optional[float]:
        """Get current risk score for a user (0.0-1.0)."""
        if self.ctx.score_manager is None:
            return None
        return self.ctx.score_manager.GetScore(user_id)

    def get_user_risk_level(self, user_id: int) -> Optional[str]:
        """Get risk level for a user ("LOW", "MEDIUM", "HIGH", "CRITICAL")."""
        if self.ctx.score_manager is None:
            return None

        risk_map = {
            0: "LOW",
            1: "MEDIUM",
            2: "HIGH",
            3: "CRITICAL",
        }
        risk_enum = self.ctx.score_manager.GetRiskLevel(user_id)
        return risk_map.get(int(risk_enum), "UNKNOWN")

    def update_user_score(
        self, user_id: int, delta: float, reason: str = "algorithm_update"
    ) -> Optional[float]:
        """Update a user's score by adding delta.

        Args:
            user_id: User ID
            delta: Score change (can be negative)
            reason: Reason for update (for logging)

        Returns:
            New score or None on failure
        """
        if self.ctx.score_manager is None:
            return None
        return self.ctx.score_manager.AddScore(user_id, delta, reason)

    def ban_user(self, user_id: int, reason: str = "algorithm_ban") -> bool:
        """Ban a user (remove from domain)."""
        if self.ctx.domain_manager is None:
            return False
        return self.ctx.domain_manager.BanUser(user_id, reason)

    def get_score_distribution(self) -> dict[str, int]:
        """Get score distribution across all users.

        Returns:
            Dict mapping risk level to user count
        """
        if self.ctx.score_manager is None:
            return {}

        dist = self.ctx.score_manager.GetScoreDistribution()
        return {
            "LOW": dist.get(self.ctx.ns.mtd.RiskLevel.LOW, 0),
            "MEDIUM": dist.get(self.ctx.ns.mtd.RiskLevel.MEDIUM, 0),
            "HIGH": dist.get(self.ctx.ns.mtd.RiskLevel.HIGH, 0),
            "CRITICAL": dist.get(self.ctx.ns.mtd.RiskLevel.CRITICAL, 0),
        }

    # ==================== Attack Operations ====================

    def start_attack(
        self,
        attack_type: str,
        target_proxy_id: int,
        rate_pps: float = 5000.0,
        packet_size: int = 512,
        duration: float = 5.0,
    ) -> bool:
        """Start an attack.

        Args:
            attack_type: "UDP_FLOOD", "SYN_FLOOD", "DOS", etc.
            target_proxy_id: Target proxy ID
            rate_pps: Packets per second
            packet_size: Bytes per packet
            duration: Duration in seconds (0 = indefinite)

        Returns:
            True if attack started
        """
        if self.ctx.attack_generator is None:
            return False

        # Map string to enum
        type_map = {
            "UDP_FLOOD": self.ctx.ns.mtd.AttackType.UDP_FLOOD,
            "SYN_FLOOD": self.ctx.ns.mtd.AttackType.SYN_FLOOD,
            "DOS": self.ctx.ns.mtd.AttackType.DOS,
            "PROBE": self.ctx.ns.mtd.AttackType.PROBE,
            "PORT_SCAN": self.ctx.ns.mtd.AttackType.PORT_SCAN,
        }
        attack_type_enum = type_map.get(attack_type.upper(), self.ctx.ns.mtd.AttackType.UDP_FLOOD)

        params = self.ctx.ns.mtd.AttackParams()
        params.type = attack_type_enum
        params.targetProxyId = target_proxy_id
        params.rate = rate_pps
        params.packetSize = packet_size
        params.duration = duration

        try:
            self.ctx.attack_generator.Configure(params)
            return self.ctx.attack_generator.Start()
        except Exception:
            return False

    def stop_attack(self, reason: str = "algorithm_stop") -> bool:
        """Stop current attack."""
        if self.ctx.attack_generator is None:
            return False
        try:
            self.ctx.attack_generator.Stop(reason)
            return True
        except Exception:
            return False

    def is_attack_active(self) -> bool:
        """Check if an attack is currently active."""
        if self.ctx.attack_generator is None:
            return False
        return self.ctx.attack_generator.IsActive()

    def get_attack_history(self) -> list[dict]:
        """Get attack history as list of dicts."""
        if self.ctx.attack_generator is None:
            return []

        history = self.ctx.attack_generator.GetAttackHistory()
        result = []
        for record in history:
            result.append({
                "attackId": record.attackId,
                "type": str(record.type),
                "targetProxyId": record.targetProxyId,
                "ratePps": record.ratePps,
                "packetSize": record.packetSize,
                "startTime": record.startTime,
                "endTime": record.endTime,
                "durationActual": record.durationActual,
                "packetsSent": record.packetsSent,
                "bytesSent": record.bytesSent,
                "defenseTriggered": record.defenseTriggered,
            })
        return result

    # ==================== Event Utilities ====================

    def parse_event_metadata(self, event) -> dict:
        """Parse event metadata dict from C++ event object."""
        if not hasattr(event, "metadata"):
            return {}

        result = {}
        for key, value in event.metadata.items():
            # Try to parse JSON if it's a groundTruth field
            if key == "groundTruth":
                try:
                    result[key] = json.loads(value)
                except json.JSONDecodeError:
                    result[key] = value
            else:
                result[key] = value
        return result

    def get_event_type_name(self, event) -> str:
        """Get human-readable event type name."""
        if not hasattr(event, "type"):
            return "UNKNOWN"
        return str(event.type)


__all__ = ["MtdApi"]
