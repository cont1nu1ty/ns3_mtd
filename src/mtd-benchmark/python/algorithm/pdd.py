#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
algorithm/pdd.py - Proactive Domain Defense (PDD) Algorithm
============================================================

This module implements the PDD defense algorithm, which uses a 5-step
cycle to detect and ban attackers based on proxy-based scoring.

Algorithm Flow:
    1. Perception: Detect attacked proxies via state.observations
    2. Scoring: Increment internal score for users on attacked proxies  
    3. MTD: Trigger shuffle to reassign user-proxy mappings
    4. Ban: Ban users whose score exceeds threshold
    5. Terminate: Stop when all attackers are banned

Key Concepts:
    - Users on attacked proxies get +1 score each round
    - Normal users' scores reset after shuffle (they move away)
    - Attackers accumulate score (they follow the attack target)
    - When score > threshold, user is banned

Parameters:
    - ban_threshold: Score threshold for banning (default: 10)

Usage:
    from algorithm.pdd import PDDAlgorithm
    
    pdd = PDDAlgorithm(ban_threshold=10)
    pdd.initialize()
    
    # Use with simulation context
    ctx.set_defense_evaluator(pdd.evaluate)
    ctx.run(duration=60.0)
    
    # Check results
    print(f\"Banned users: {pdd.banned_users}\")
    print(f\"Attacker caught: {pdd.is_attacker_banned()}\")

Note:
    All logging is via 'reason' parameters in API calls.
    Python code does NOT use print/logging for simulation events.
"""

import sys
import os
from typing import Dict, List, Set, Any

sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.abspath(__file__))))

from mtd_defense import (
    DefenseAlgorithm,
    SimulationState,
    DefenseDecision,
    ShuffleMode,
)


class PDDAlgorithm(DefenseAlgorithm):
    """
    PDD (Proactive Domain Defense) Algorithm
    
    Score tracking in Python, actions via C++ API with reason strings.
    """
    
    ATTACKER_ID = 100  # Convention: user 100 is the attacker
    
    def __init__(self, ban_threshold: int = 10):
        super().__init__("PDD")
        self.ban_threshold = ban_threshold
        self.user_malice_scores: Dict[int, int] = {}
        self.banned_users: Set[int] = set()
        self.round_count: int = 0
        self.total_shuffles: int = 0
    
    def initialize(self, parameters: Dict[str, Any] = None) -> None:
        """Initialize with optional parameters."""
        super().initialize(parameters)
        if parameters:
            self.ban_threshold = parameters.get('ban_threshold', self.ban_threshold)
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        """
        Main evaluation - called each simulation step.
        Returns decisions with descriptive reasons for C++ logging.
        """
        self.round_count += 1
        decisions: List[DefenseDecision] = []
        
        # Step 1: Perception - detect attacked proxies
        attacked_proxies = self._detect_attacked_proxies(state)
        if not attacked_proxies:
            return decisions
        
        # Step 2: Scoring - increment scores for users on attacked proxies
        affected_users = self._update_scores(state, attacked_proxies)
        
        # Step 3: MTD - trigger shuffle with reason
        for domain_id in state.domains:
            reason = f"PDD: Attack on proxy {attacked_proxies}, {len(affected_users)} users scored"
            decisions.append(DefenseDecision.trigger_shuffle(
                domain_id, ShuffleMode.RANDOM, reason
            ))
            self.total_shuffles += 1
        
        # Step 4: Ban - check and ban users exceeding threshold
        self._check_and_ban_users(decisions)
        
        return decisions
    
    def _detect_attacked_proxies(self, state: SimulationState) -> List[int]:
        """Detect proxies under attack (confidence > 0.5 and rate_anomaly > 0.5)."""
        attacked = []
        for node_id, obs in state.observations.items():
            confidence = getattr(obs, 'confidence', 0.0)
            rate_anomaly = getattr(obs, 'rate_anomaly', 0.0)
            if confidence > 0.5 and rate_anomaly > 0.5:
                attacked.append(node_id)
        return attacked
    
    def _get_users_on_proxies(self, state: SimulationState, proxy_ids: List[int]) -> Set[int]:
        """Get users on specified proxies."""
        users = set()
        
        # Method 1: Direct proxy_to_users mapping
        if hasattr(state, 'proxy_to_users') and state.proxy_to_users:
            for proxy_id in proxy_ids:
                users.update(state.proxy_to_users.get(proxy_id, []))
            return users
        
        # Method 2: From domain information
        for domain in state.domains.values():
            domain_proxies = getattr(domain, 'proxy_ids', [])
            for proxy_id in proxy_ids:
                if proxy_id in domain_proxies:
                    users.update(getattr(domain, 'user_ids', []))
        return users
    
    def _update_scores(self, state: SimulationState, attacked_proxies: List[int]) -> Set[int]:
        """Increment scores for users on attacked proxies."""
        users = self._get_users_on_proxies(state, attacked_proxies)
        affected = set()
        
        for user_id in users:
            if user_id in self.banned_users:
                continue
            self.user_malice_scores[user_id] = self.user_malice_scores.get(user_id, 0) + 1
            affected.add(user_id)
        
        return affected
    
    def _check_and_ban_users(self, decisions: List[DefenseDecision]) -> List[int]:
        """Ban users with score > threshold."""
        newly_banned = []
        
        for user_id, score in list(self.user_malice_scores.items()):
            if score > self.ban_threshold and user_id not in self.banned_users:
                self.banned_users.add(user_id)
                newly_banned.append(user_id)
                
                reason = f"PDD: Ban user {user_id} (score {score} > {self.ban_threshold})"
                decisions.append(DefenseDecision.update_score(user_id, 1.0, reason))
        
        return newly_banned
    
    def is_attacker_banned(self) -> bool:
        """Check if attacker has been banned."""
        return self.ATTACKER_ID in self.banned_users
    
    def get_statistics(self) -> Dict[str, float]:
        """Get algorithm statistics."""
        return {
            'rounds': float(self.round_count),
            'banned_count': float(len(self.banned_users)),
            'total_shuffles': float(self.total_shuffles),
            'max_score': float(max(self.user_malice_scores.values())) if self.user_malice_scores else 0.0,
            'users_tracked': float(len(self.user_malice_scores)),
            'attacker_banned': float(self.is_attacker_banned()),
        }
    
    def reset(self) -> None:
        """Reset algorithm state."""
        super().reset()
        self.user_malice_scores.clear()
        self.banned_users.clear()
        self.round_count = 0
        self.total_shuffles = 0


# =============================================================================
# Standalone entry point
# =============================================================================

def main():
    """Run PDD algorithm standalone."""
    import argparse
    
    parser = argparse.ArgumentParser(description='PDD Defense Algorithm')
    parser.add_argument('--users', type=int, default=100)
    parser.add_argument('--proxies', type=int, default=5)
    parser.add_argument('--threshold', type=int, default=10)
    parser.add_argument('--duration', type=float, default=60.0)
    parser.add_argument('--seed', type=int, default=42)
    args = parser.parse_args()
    
    from mtd_api import create_simulation_context
    
    ctx = create_simulation_context(
        num_users=args.users,
        num_proxies=args.proxies,
        num_domains=1,
        num_attackers=1
    )
    ctx.set_random_seed(args.seed)
    
    pdd = PDDAlgorithm(ban_threshold=args.threshold)
    pdd.initialize()
    ctx.set_defense_evaluator(pdd.evaluate)
    
    ctx.run(duration=args.duration)
    
    # Output results as JSON for unified processing
    import json
    results = {
        'simulation': ctx.get_results(),
        'algorithm': pdd.get_statistics(),
        'success': pdd.is_attacker_banned(),
        'banned_users': list(pdd.banned_users),
    }
    print(json.dumps(results, indent=2))
    
    ctx.export_results("pdd_results")


if __name__ == '__main__':
    main()
