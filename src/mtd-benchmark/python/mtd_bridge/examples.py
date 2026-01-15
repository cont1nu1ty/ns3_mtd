"""Example algorithms for MTD-Benchmark.

These serve as templates for algorithm developers.
"""

from __future__ import annotations

from typing import Optional, Sequence
from .types import BridgeContext, Algorithm
from .api import MtdApi


class SimpleThresholdAlgorithm(Algorithm):
    """Simple threshold-based defense algorithm.

    Triggers shuffle when attack is detected and bans users
    with high risk scores.
"""

    def __init__(self, risk_threshold: float = 0.7, shuffle_on_attack: bool = True):
        self.risk_threshold = risk_threshold
        self.shuffle_on_attack = shuffle_on_attack
        self.api: Optional[MtdApi] = None

    def on_start(self, ctx: BridgeContext) -> None:
        """Initialize algorithm."""
        self.api = MtdApi(ctx)
        print(f"SimpleThresholdAlgorithm started (risk_threshold={self.risk_threshold})")

    def on_events(self, ctx: BridgeContext, events: Sequence[object]) -> None:
        """Process events."""
        if self.api is None:
            return

        for event in events:
            event_type = self.api.get_event_type_name(event)
            metadata = self.api.parse_event_metadata(event)

            # React to attack detection
            if event_type == "ATTACK_DETECTED":
                proxy_id = int(metadata.get("proxyId", 0))
                print(f"Attack detected on proxy {proxy_id}")

                if self.shuffle_on_attack:
                    # Trigger shuffle for all domains
                    for domain_id in self.api.get_all_domains():
                        self.api.trigger_shuffle(domain_id, mode="SCORE_DRIVEN", reason="attack_detected")

            # Check for high-risk users
            elif event_type == "SCORE_UPDATED":
                user_id = int(metadata.get("userId", 0))
                score = float(metadata.get("score", 0.0))
                risk_level = metadata.get("riskLevel", "LOW")

                if score >= self.risk_threshold:
                    print(f"Banning user {user_id} (score={score:.2f}, risk={risk_level})")
                    self.api.ban_user(user_id, reason=f"threshold_exceeded_{self.risk_threshold}")

    def on_tick(self, ctx: BridgeContext, now_ms: int) -> None:
        """Periodic tick processing."""
        if self.api is None:
            return

        # Periodic check for high-risk users
        for domain_id in self.api.get_all_domains():
            for user_id in self.api.get_domain_users(domain_id):
                score = self.api.get_user_score(user_id)
                if score is not None and score >= self.risk_threshold:
                    self.api.ban_user(user_id, reason="periodic_check")


class AdaptiveShuffleAlgorithm(Algorithm):
    """Adaptive shuffle algorithm that adjusts frequency based on attack intensity."""

    def __init__(self, base_interval_ms: int = 10000, min_interval_ms: int = 1000):
        self.base_interval_ms = base_interval_ms
        self.min_interval_ms = min_interval_ms
        self.api: Optional[MtdApi] = None
        self.last_shuffle_ms: dict[int, int] = {}
        self.attack_count = 0

    def on_start(self, ctx: BridgeContext) -> None:
        """Initialize algorithm."""
        self.api = MtdApi(ctx)
        print("AdaptiveShuffleAlgorithm started")

    def on_events(self, ctx: BridgeContext, events: Sequence[object]) -> None:
        """Process events."""
        if self.api is None:
            return

        for event in events:
            event_type = self.api.get_event_type_name(event)

            if event_type == "ATTACK_DETECTED":
                self.attack_count += 1
                # Adaptive: more frequent shuffles when under attack
                interval = max(
                    self.min_interval_ms,
                    self.base_interval_ms // (1 + self.attack_count),
                )
                print(f"Attack #{self.attack_count} detected, adjusting shuffle interval to {interval}ms")

    def on_tick(self, ctx: BridgeContext, now_ms: int) -> None:
        """Periodic tick processing."""
        if self.api is None:
            return

        # Calculate adaptive interval
        interval = max(
            self.min_interval_ms,
            self.base_interval_ms // (1 + self.attack_count),
        )

        # Trigger shuffle for each domain if interval has passed
        for domain_id in self.api.get_all_domains():
            last = self.last_shuffle_ms.get(domain_id, 0)
            if now_ms - last >= interval:
                self.api.trigger_shuffle(domain_id, mode="SCORE_DRIVEN", reason="adaptive_interval")
                self.last_shuffle_ms[domain_id] = now_ms


class LoadBalancingAlgorithm(Algorithm):
    """Load balancing algorithm that splits domains when overloaded."""

    def __init__(self, max_users_per_proxy: int = 10):
        self.max_users_per_proxy = max_users_per_proxy
        self.api: Optional[MtdApi] = None

    def on_start(self, ctx: BridgeContext) -> None:
        """Initialize algorithm."""
        self.api = MtdApi(ctx)
        print(f"LoadBalancingAlgorithm started (max_users_per_proxy={self.max_users_per_proxy})")

    def on_tick(self, ctx: BridgeContext, now_ms: int) -> None:
        """Periodic tick processing."""
        if self.api is None:
            return

        # Check load for each domain
        for domain_id in self.api.get_all_domains():
            proxies = self.api.get_domain_proxies(domain_id)
            users = self.api.get_domain_users(domain_id)

            if not proxies or not users:
                continue

            # Calculate average load
            avg_load = len(users) / len(proxies)

            # Check if any proxy is overloaded
            overloaded = False
            for proxy_id in proxies:
                proxy_users = self.api.get_users_on_proxy(proxy_id)
                if len(proxy_users) > self.max_users_per_proxy:
                    overloaded = True
                    break

            # Split domain if overloaded
            if overloaded and avg_load > self.max_users_per_proxy:
                print(f"Domain {domain_id} overloaded (avg_load={avg_load:.1f}), splitting...")
                new_domain_id = self.api.split_domain(domain_id)
                if new_domain_id:
                    print(f"Split domain {domain_id} -> {new_domain_id}")


__all__ = [
    "SimpleThresholdAlgorithm",
    "AdaptiveShuffleAlgorithm",
    "LoadBalancingAlgorithm",
]
