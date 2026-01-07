#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
mtd_api.py - Simulation Context and C++ Bindings
================================================

This module provides the interface between Python algorithms and
the NS-3 C++ simulation backend.

Contents:
    Functions:
        - create_simulation_context(): Factory for simulation context
        - is_cpp_module_available(): Check if C++ bindings loaded
    
    Classes:
        - MockSimulationContext: Standalone testing without NS-3
            - initialize(): Set up simulation state
            - run(duration): Execute simulation loop
            - step(step_size): Advance single time step
            - get_state(): Get current SimulationState snapshot
            - trigger_shuffle(domain_id, mode, reason): Execute shuffle
            - update_score(user_id, score, reason): Update user score
            - export_results(prefix, output_dir): Export logs to files
    
    Enums:
        - EventType: Simulation event types

Exported Log Files (via export_results):
    - {prefix}_events.json:   Complete event history
    - {prefix}_shuffles.csv:  Shuffle operations log
    - {prefix}_attacks.csv:   Attack detection log
    - {prefix}_bans.csv:      User ban log
    - {prefix}_domains.json:  Domain state snapshot
    - {prefix}_snapshot.json: Full experiment snapshot
    - {prefix}_traffic.csv:   Traffic trace data

Usage:
    from mtd_api import create_simulation_context
    
    ctx = create_simulation_context(num_users=100, num_proxies=5)
    ctx.set_defense_evaluator(my_algorithm.evaluate)
    ctx.run(duration=60.0)
    ctx.export_results('my_results')

Note:
    All logging is done via 'reason' parameters passed to C++ backend.
    Python code should NOT use print() or logging for simulation events.
"""

import sys
from typing import Dict, List, Callable, Optional, Set
from dataclasses import dataclass, field
from enum import Enum
import random

# ============================================================================
# Import C++ module
# ============================================================================

_CPP_MODULE_AVAILABLE = False
_mtd_cpp = None

try:
    import mtd_benchmark as _mtd_cpp
    _CPP_MODULE_AVAILABLE = True
except ImportError:
    try:
        sys.path.insert(0, '/home/moon/ns-3-dev/build/lib')
        import mtd_benchmark as _mtd_cpp
        _CPP_MODULE_AVAILABLE = True
    except ImportError:
        _CPP_MODULE_AVAILABLE = False

# ============================================================================
# Re-export C++ types or use Python equivalents
# ============================================================================

if _CPP_MODULE_AVAILABLE:
    ShuffleMode = _mtd_cpp.ShuffleMode
    RiskLevel = _mtd_cpp.RiskLevel
    ActionType = _mtd_cpp.ActionType
    AttackType = _mtd_cpp.AttackType
    EventType = _mtd_cpp.EventType
    SimulationState = _mtd_cpp.SimulationState
    DefenseDecision = _mtd_cpp.DefenseDecision
    Domain = _mtd_cpp.Domain
    UserScore = _mtd_cpp.UserScore
    TrafficStats = _mtd_cpp.TrafficStats
    DetectionObservation = _mtd_cpp.DetectionObservation
    MtdEvent = _mtd_cpp.MtdEvent
    SimulationContext = _mtd_cpp.SimulationContext
else:
    from mtd_defense import (
        ShuffleMode, RiskLevel, ActionType, AttackType,
        SimulationState, DefenseDecision, Domain, UserScore,
        TrafficStats, DetectionObservation, MtdEvent
    )
    
    class EventType(Enum):
        SHUFFLE_TRIGGERED = 0
        SHUFFLE_COMPLETED = 1
        DOMAIN_SPLIT = 2
        DOMAIN_MERGE = 3
        USER_MIGRATED = 4
        ATTACK_DETECTED = 5
        ATTACK_STARTED = 6
        ATTACK_STOPPED = 7
        PROXY_SWITCHED = 8
        THRESHOLD_EXCEEDED = 9
        SCORE_UPDATED = 10


# ============================================================================
# Mock SimulationContext for standalone testing
# ============================================================================

class MockSimulationContext:
    """Mock simulation context for testing without NS-3."""
    
    def __init__(self, num_users: int = 100, num_proxies: int = 5,
                 num_domains: int = 1, num_attackers: int = 1):
        self._num_users = num_users
        self._num_proxies = num_proxies
        self._num_domains = num_domains
        self._num_attackers = num_attackers
        self._current_time = 0.0
        self._initialized = False
        self._running = False
        self._random_seed = 42
        self._shuffle_frequency = 10.0
        
        self._user_to_proxy: Dict[int, int] = {}
        self._proxy_to_users: Dict[int, Set[int]] = {}
        self._user_scores: Dict[int, float] = {}
        self._banned_users: Set[int] = set()
        self._attacker_ids: Set[int] = set()
        self._defense_evaluator: Optional[Callable] = None
        self._events: List[Dict] = []
        self._shuffle_count = 0
        
    def initialize(self) -> None:
        if self._initialized:
            return
        random.seed(self._random_seed)
        self._proxy_to_users = {i: set() for i in range(1, self._num_proxies + 1)}
        for i in range(self._num_users):
            user_id = 100 + i
            self._user_scores[user_id] = 0.0
            proxy_id = random.randint(1, self._num_proxies)
            self._user_to_proxy[user_id] = proxy_id
            self._proxy_to_users[proxy_id].add(user_id)
        for i in range(self._num_attackers):
            self._attacker_ids.add(100 + i)
        self._initialized = True
    
    def set_random_seed(self, seed: int) -> None:
        self._random_seed = seed
        random.seed(seed)
    
    def set_shuffle_frequency(self, frequency: float) -> None:
        self._shuffle_frequency = frequency
    
    def set_attack_rate(self, rate: float) -> None:
        pass
    
    def set_defense_evaluator(self, evaluator: Callable) -> None:
        self._defense_evaluator = evaluator
    
    def set_score_calculator(self, calculator: Callable) -> None:
        pass
    
    def run(self, duration: float = 60.0) -> None:
        if not self._initialized:
            self.initialize()
        self._running = True
        while self._current_time < duration and self._running:
            self.step(1.0)
    
    def step(self, step_size: float = 1.0) -> None:
        """Advance simulation by one step."""
        if not self._initialized:
            self.initialize()
        self._current_time += step_size
        
        # Record attack events for active attackers
        for attacker_id in self._attacker_ids:
            if attacker_id not in self._banned_users and attacker_id in self._user_to_proxy:
                proxy_id = self._user_to_proxy[attacker_id]
                self._events.append({
                    'type': 'ATTACK_DETECTED',
                    'time': self._current_time,
                    'attacker_id': attacker_id,
                    'proxy_id': proxy_id,
                    'attack_type': 'PROBE',
                    'reason': f'Attack from user {attacker_id} on proxy {proxy_id}'
                })
        
        if self._defense_evaluator:
            state = self.get_state()
            decisions = self._defense_evaluator(state)
            self._execute_decisions(decisions)
    
    def _execute_decisions(self, decisions: List) -> None:
        for decision in decisions:
            action = decision.action if hasattr(decision, 'action') else decision.get('action')
            reason = getattr(decision, 'reason', '') if hasattr(decision, 'reason') else decision.get('reason', '')
            
            if action == ActionType.TRIGGER_SHUFFLE:
                self._do_shuffle(reason)
            elif action == ActionType.UPDATE_SCORE:
                user_id = decision.target_user_id if hasattr(decision, 'target_user_id') else decision.get('target_user_id')
                score = decision.new_score if hasattr(decision, 'new_score') else decision.get('new_score')
                if score >= 1.0:
                    self._ban_user(user_id, reason)
                else:
                    self._user_scores[user_id] = score
    
    def _do_shuffle(self, reason: str = "") -> None:
        self._shuffle_count += 1
        for proxy_id in self._proxy_to_users:
            self._proxy_to_users[proxy_id].clear()
        for user_id in self._user_scores.keys():
            if user_id not in self._banned_users:
                proxy_id = random.randint(1, self._num_proxies)
                self._user_to_proxy[user_id] = proxy_id
                self._proxy_to_users[proxy_id].add(user_id)
        self._events.append({
            'type': 'SHUFFLE_COMPLETED',
            'time': self._current_time,
            'reason': reason
        })
    
    def _ban_user(self, user_id: int, reason: str = "") -> None:
        if user_id in self._banned_users:
            return
        self._banned_users.add(user_id)
        if user_id in self._user_to_proxy:
            proxy_id = self._user_to_proxy[user_id]
            self._proxy_to_users[proxy_id].discard(user_id)
            del self._user_to_proxy[user_id]
        self._events.append({
            'type': 'USER_BANNED',
            'time': self._current_time,
            'user_id': user_id,
            'reason': reason
        })
        if self._attacker_ids.issubset(self._banned_users):
            self._running = False
    
    def get_state(self) -> SimulationState:
        if not _CPP_MODULE_AVAILABLE:
            state = SimulationState()
            state.current_time = int(self._current_time * 1e9)
            domain = Domain()
            domain.domain_id = 1
            domain.name = "MainDomain"
            domain.proxy_ids = list(range(1, self._num_proxies + 1))
            domain.user_ids = [u for u in self._user_scores.keys() if u not in self._banned_users]
            state.domains = {1: domain}
            state.user_scores = {}
            for user_id, score in self._user_scores.items():
                if user_id not in self._banned_users:
                    us = UserScore()
                    us.user_id = user_id
                    us.current_score = score
                    us.risk_level = RiskLevel.LOW
                    state.user_scores[user_id] = us
            state.observations = {}
            for attacker_id in self._attacker_ids:
                if attacker_id not in self._banned_users and attacker_id in self._user_to_proxy:
                    proxy_id = self._user_to_proxy[attacker_id]
                    obs = DetectionObservation()
                    obs.rate_anomaly = 0.8
                    obs.connection_anomaly = 0.6
                    obs.pattern_anomaly = 0.5
                    obs.confidence = 0.9
                    state.observations[proxy_id] = obs
            state.proxy_to_users = {k: list(v) for k, v in self._proxy_to_users.items()}
            return state
        else:
            raise NotImplementedError("C++ state building in mock context")
    
    def trigger_shuffle(self, domain_id: int, mode=None, reason: str = "") -> None:
        self._do_shuffle(reason)
    
    def update_score(self, user_id: int, score: float, reason: str = "") -> None:
        if score >= 1.0:
            self._ban_user(user_id, reason)
        else:
            self._user_scores[user_id] = score
    
    def migrate_user(self, user_id: int, new_domain_id: int) -> None:
        pass
    
    def get_results(self) -> Dict[str, float]:
        return {
            'simulation_time': self._current_time,
            'num_users': float(self._num_users),
            'num_proxies': float(self._num_proxies),
            'banned_count': float(len(self._banned_users)),
            'shuffle_count': float(self._shuffle_count),
            'attackers_banned': float(len(self._attacker_ids & self._banned_users))
        }
    
    def export_results(self, prefix: str = "mtd_results", output_dir: str = None) -> None:
        import json
        import os
        
        # Default to ns3-dev root directory
        if output_dir is None:
            output_dir = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..', '..'))
        
        os.makedirs(output_dir, exist_ok=True)
        
        # 1. Export events log (EventHistory)
        events_path = os.path.join(output_dir, f"{prefix}_events.json")
        with open(events_path, 'w') as f:
            json.dump({
                'events': self._events,
                'summary': {
                    'total_events': len(self._events),
                    'shuffle_count': self._shuffle_count,
                    'banned_count': len(self._banned_users),
                }
            }, f, indent=2)
        
        # 2. Export shuffle events (ShuffleEvents)
        shuffles_path = os.path.join(output_dir, f"{prefix}_shuffles.csv")
        with open(shuffles_path, 'w') as f:
            f.write("timestamp,domainId,strategy,usersAffected,executionTime,success,reason\n")
            for evt in self._events:
                if evt.get('type') == 'SHUFFLE_COMPLETED':
                    f.write(f"{evt.get('time', 0)},1,SCORE_DRIVEN,{self._num_users},0.001,true,\"{evt.get('reason', '')}\"\n")
        
        # 3. Export attack events (AttackEvents)  
        attacks_path = os.path.join(output_dir, f"{prefix}_attacks.csv")
        with open(attacks_path, 'w') as f:
            f.write("timestamp,attackerId,targetProxy,attackType,detected,reason\n")
            for evt in self._events:
                if evt.get('type') == 'ATTACK_DETECTED':
                    f.write(f"{evt.get('time', 0)},{evt.get('attacker_id', 0)},{evt.get('proxy_id', 0)},{evt.get('attack_type', 'PROBE')},true,\"{evt.get('reason', '')}\"\n")
        
        # 4. Export ban history (custom, part of events)
        bans_path = os.path.join(output_dir, f"{prefix}_bans.csv")
        with open(bans_path, 'w') as f:
            f.write("timestamp,user_id,score,reason\n")
            for evt in self._events:
                if evt.get('type') == 'USER_BANNED':
                    f.write(f"{evt.get('time', 0)},{evt.get('user_id', 0)},{evt.get('score', 0)},\"{evt.get('reason', '')}\"\n")
        
        # 5. Export domain state (DomainState)
        domains_path = os.path.join(output_dir, f"{prefix}_domains.json")
        with open(domains_path, 'w') as f:
            domains_list = []
            proxies_per_domain = max(1, self._num_proxies // self._num_domains)
            for d in range(self._num_domains):
                domain_proxies = list(range(d * proxies_per_domain + 1, (d + 1) * proxies_per_domain + 1))
                domain_users = [u for u, p in self._user_to_proxy.items() 
                               if p in domain_proxies and u not in self._banned_users]
                domains_list.append({
                    'domain_id': d + 1,
                    'name': f'Domain_{d + 1}',
                    'proxy_ids': domain_proxies,
                    'user_ids': domain_users,
                    'shuffle_count': self._shuffle_count // self._num_domains,
                })
            json.dump({
                'domains': domains_list,
                'timestamp': self._current_time,
            }, f, indent=2)
        
        # 6. Export experiment snapshot (ExperimentSnapshot)
        snapshot_path = os.path.join(output_dir, f"{prefix}_snapshot.json")
        with open(snapshot_path, 'w') as f:
            json.dump({
                'simulation_time': self._current_time,
                'num_users': self._num_users,
                'num_proxies': self._num_proxies,
                'num_domains': self._num_domains,
                'banned_users': list(self._banned_users),
                'attacker_ids': list(self._attacker_ids),
                'shuffle_count': self._shuffle_count,
                'user_scores': dict(self._user_scores),
                'performance': {
                    'total_shuffles': self._shuffle_count,
                    'total_bans': len(self._banned_users),
                    'attackers_caught': len(self._attacker_ids & self._banned_users),
                }
            }, f, indent=2)
        
        # 7. Export traffic trace (TrafficTrace) - simplified mock
        traffic_path = os.path.join(output_dir, f"{prefix}_traffic.csv")
        with open(traffic_path, 'w') as f:
            f.write("timestamp,domainId,proxyId,packetsIn,packetsOut,bytesIn,bytesOut\n")
            # Mock traffic data for each second
            for t in range(int(self._current_time) + 1):
                for p in range(1, self._num_proxies + 1):
                    f.write(f"{t},1,{p},100,80,15000,12000\n")
    
    def stop(self) -> None:
        self._running = False
    
    @property
    def current_time(self) -> float:
        return self._current_time
    
    @property
    def num_users(self) -> int:
        return self._num_users
    
    @property
    def num_proxies(self) -> int:
        return self._num_proxies
    
    @property
    def num_domains(self) -> int:
        return self._num_domains
    
    @property
    def is_running(self) -> bool:
        return self._running
    
    def is_attacker_banned(self) -> bool:
        return self._attacker_ids.issubset(self._banned_users)


# ============================================================================
# Factory function
# ============================================================================

def create_simulation_context(num_users: int = 100, num_proxies: int = 5,
                              num_domains: int = 1, num_attackers: int = 1):
    """Create SimulationContext (C++ if available, else Mock)."""
    if _CPP_MODULE_AVAILABLE:
        return SimulationContext(num_users, num_proxies, num_domains, num_attackers)
    else:
        return MockSimulationContext(num_users, num_proxies, num_domains, num_attackers)


def is_cpp_module_available() -> bool:
    return _CPP_MODULE_AVAILABLE


# ============================================================================
# Exports
# ============================================================================

__all__ = [
    'is_cpp_module_available',
    'create_simulation_context',
    'SimulationContext',
    'MockSimulationContext',
    'ShuffleMode',
    'RiskLevel', 
    'ActionType',
    'AttackType',
    'EventType',
    'SimulationState',
    'DefenseDecision',
    'Domain',
    'UserScore',
    'TrafficStats',
    'DetectionObservation',
    'MtdEvent',
]
