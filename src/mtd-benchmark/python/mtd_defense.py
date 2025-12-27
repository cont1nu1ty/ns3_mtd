#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MTD-Benchmark Python Defense Algorithm Base Classes

This module provides base classes and utilities for implementing
custom defense algorithms in Python for the NS-3 MTD-Benchmark platform.

Authors: MTD-Benchmark Team
Version: 1.0.0
"""

from abc import ABC, abstractmethod
from dataclasses import dataclass, field
from enum import Enum, auto
from typing import List, Dict, Callable, Optional, Any, Tuple
import json
import logging

# Configure logging
logging.basicConfig(
    level=logging.INFO,
    format='%(asctime)s - %(name)s - %(levelname)s - %(message)s'
)
logger = logging.getLogger('mtd_algorithm')


# ==================== Enums (Python-side mirrors) ====================

class RiskLevel(Enum):
    """User risk levels"""
    LOW = 0
    MEDIUM = 1
    HIGH = 2
    CRITICAL = 3


class ShuffleMode(Enum):
    """Shuffle operation modes"""
    RANDOM = 0
    SCORE_DRIVEN = 1
    ROUND_ROBIN = 2
    ATTACKER_AVOID = 3
    LOAD_BALANCED = 4
    CUSTOM = 5


class AttackType(Enum):
    """Types of network attacks"""
    NONE = 0
    DOS = 1
    PROBE = 2
    PORT_SCAN = 3
    ROUTE_MONITOR = 4
    SYN_FLOOD = 5
    UDP_FLOOD = 6
    HTTP_FLOOD = 7


class ActionType(Enum):
    """Defense action types"""
    NO_ACTION = 0
    TRIGGER_SHUFFLE = 1
    MIGRATE_USER = 2
    SPLIT_DOMAIN = 3
    MERGE_DOMAINS = 4
    UPDATE_SCORE = 5
    CHANGE_FREQUENCY = 6
    CUSTOM = 7


# ==================== Data Classes ====================

@dataclass
class TrafficStats:
    """Traffic statistics for a node"""
    packets_in: int = 0
    packets_out: int = 0
    bytes_in: int = 0
    bytes_out: int = 0
    packet_rate: float = 0.0
    byte_rate: float = 0.0
    active_connections: int = 0
    avg_latency: float = 0.0


@dataclass
class DetectionObservation:
    """Attack detection observation"""
    rate_anomaly: float = 0.0
    connection_anomaly: float = 0.0
    pattern_anomaly: float = 0.0
    persistence_factor: float = 0.0
    suspected_type: AttackType = AttackType.NONE
    confidence: float = 0.0
    timestamp: int = 0


@dataclass
class UserScore:
    """User risk score record"""
    user_id: int = 0
    current_score: float = 0.0
    risk_level: RiskLevel = RiskLevel.LOW
    last_update_time: int = 0


@dataclass
class Domain:
    """Logical domain grouping"""
    domain_id: int = 0
    name: str = ""
    proxy_ids: List[int] = field(default_factory=list)
    user_ids: List[int] = field(default_factory=list)
    load_factor: float = 0.0
    shuffle_frequency: float = 1.0


@dataclass
class MtdEvent:
    """Base event structure"""
    event_type: str = ""
    timestamp: int = 0
    source_node_id: int = 0
    metadata: Dict[str, str] = field(default_factory=dict)


@dataclass
class SimulationState:
    """Complete simulation state snapshot"""
    current_time: int = 0  # nanoseconds
    domains: Dict[int, Domain] = field(default_factory=dict)
    user_scores: Dict[int, UserScore] = field(default_factory=dict)
    proxy_stats: Dict[int, TrafficStats] = field(default_factory=dict)
    observations: Dict[int, DetectionObservation] = field(default_factory=dict)
    recent_events: List[MtdEvent] = field(default_factory=list)
    
    @property
    def time_seconds(self) -> float:
        """Get current time in seconds"""
        return self.current_time / 1e9


@dataclass
class DefenseDecision:
    """Defense decision to execute"""
    action: ActionType = ActionType.NO_ACTION
    target_domain_id: int = 0
    target_user_id: int = 0
    target_proxy_id: int = 0
    secondary_domain_id: int = 0
    new_score: float = 0.0
    new_frequency: float = 0.0
    shuffle_mode: ShuffleMode = ShuffleMode.RANDOM
    custom_params: Dict[str, str] = field(default_factory=dict)
    reason: str = ""
    
    @staticmethod
    def trigger_shuffle(domain_id: int, mode: ShuffleMode = ShuffleMode.RANDOM,
                        reason: str = "") -> 'DefenseDecision':
        """Create a shuffle trigger decision"""
        return DefenseDecision(
            action=ActionType.TRIGGER_SHUFFLE,
            target_domain_id=domain_id,
            shuffle_mode=mode,
            reason=reason
        )
    
    @staticmethod
    def migrate_user(user_id: int, domain_id: int, reason: str = "") -> 'DefenseDecision':
        """Create a user migration decision"""
        return DefenseDecision(
            action=ActionType.MIGRATE_USER,
            target_user_id=user_id,
            target_domain_id=domain_id,
            reason=reason
        )
    
    @staticmethod
    def update_score(user_id: int, score: float, reason: str = "") -> 'DefenseDecision':
        """Create a score update decision"""
        return DefenseDecision(
            action=ActionType.UPDATE_SCORE,
            target_user_id=user_id,
            new_score=score,
            reason=reason
        )
    
    @staticmethod
    def change_frequency(domain_id: int, frequency: float, reason: str = "") -> 'DefenseDecision':
        """Create a frequency change decision"""
        return DefenseDecision(
            action=ActionType.CHANGE_FREQUENCY,
            target_domain_id=domain_id,
            new_frequency=frequency,
            reason=reason
        )
    
    @staticmethod
    def no_action() -> 'DefenseDecision':
        """Create a no-action decision"""
        return DefenseDecision(action=ActionType.NO_ACTION)


# ==================== Abstract Base Classes ====================

class DefenseAlgorithm(ABC):
    """
    Abstract base class for defense algorithms.
    
    Implement this class to create custom defense algorithms that can be
    integrated with the NS-3 MTD-Benchmark simulation.
    
    Example:
        class MyAlgorithm(DefenseAlgorithm):
            def __init__(self):
                super().__init__("MyAlgorithm")
                self.threshold = 0.7
            
            def evaluate(self, state):
                decisions = []
                for domain_id, domain in state.domains.items():
                    if self._check_threat(domain, state):
                        decisions.append(DefenseDecision.trigger_shuffle(
                            domain_id, ShuffleMode.SCORE_DRIVEN))
                return decisions
    """
    
    def __init__(self, name: str = "DefenseAlgorithm"):
        """Initialize the defense algorithm"""
        self.name = name
        self.parameters: Dict[str, Any] = {}
        self._logger = logging.getLogger(f'mtd.{name}')
    
    @abstractmethod
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        """
        Evaluate the current simulation state and return defense decisions.
        
        This is the main entry point for the defense algorithm. It receives
        a complete snapshot of the simulation state and should return a list
        of defense decisions to execute.
        
        Args:
            state: Current simulation state snapshot
            
        Returns:
            List of defense decisions to execute
        """
        pass
    
    def initialize(self, parameters: Dict[str, Any] = None) -> None:
        """
        Initialize the algorithm with parameters.
        
        Override this method to perform custom initialization.
        
        Args:
            parameters: Algorithm-specific parameters
        """
        if parameters:
            self.parameters.update(parameters)
        self._logger.info(f"Initialized {self.name} with parameters: {self.parameters}")
    
    def on_event(self, event: MtdEvent) -> Optional[DefenseDecision]:
        """
        Handle a specific event (optional).
        
        Override this method to react to specific events in real-time.
        
        Args:
            event: The event that occurred
            
        Returns:
            Optional defense decision to execute immediately
        """
        return None
    
    def get_statistics(self) -> Dict[str, float]:
        """
        Get algorithm statistics.
        
        Override to provide custom metrics.
        
        Returns:
            Dictionary of metric name to value
        """
        return {}
    
    def reset(self) -> None:
        """Reset algorithm state"""
        self._logger.info(f"Reset {self.name}")


class ScoreCalculator(ABC):
    """
    Abstract base class for custom score calculation algorithms.
    
    Implement this to replace the default scoring formula.
    
    Example:
        class MyScorer(ScoreCalculator):
            def calculate(self, user_id, observation, current_score):
                return 0.6 * observation.rate_anomaly + 0.4 * current_score
    """
    
    @abstractmethod
    def calculate(self, user_id: int, observation: DetectionObservation,
                  current_score: float) -> float:
        """
        Calculate new score for a user.
        
        Args:
            user_id: The user ID
            observation: Detection observation
            current_score: Current user score
            
        Returns:
            New score value (0.0 to 1.0)
        """
        pass


class RiskClassifier(ABC):
    """
    Abstract base class for custom risk classification.
    
    Implement this to replace the default threshold-based classification.
    
    Example:
        class MyClassifier(RiskClassifier):
            def classify(self, user_id, score):
                if score > 0.8:
                    return RiskLevel.CRITICAL
                return RiskLevel.LOW
    """
    
    @abstractmethod
    def classify(self, user_id: int, score: float) -> RiskLevel:
        """
        Classify user risk level based on score.
        
        Args:
            user_id: The user ID
            score: Current user score
            
        Returns:
            Risk level classification
        """
        pass


class ShuffleStrategy(ABC):
    """
    Abstract base class for custom shuffle strategies.
    
    Implement this to define custom proxy selection logic.
    
    Example:
        class MyStrategy(ShuffleStrategy):
            def select_proxy(self, user_id, proxy_ids, user_score):
                if user_score.risk_level == RiskLevel.HIGH:
                    return proxy_ids[-1]  # Isolate to last proxy
                return proxy_ids[0]
    """
    
    @abstractmethod
    def select_proxy(self, user_id: int, proxy_ids: List[int],
                     user_score: UserScore) -> int:
        """
        Select a proxy for user assignment.
        
        Args:
            user_id: The user ID
            proxy_ids: Available proxy IDs
            user_score: User's current score
            
        Returns:
            Selected proxy ID
        """
        pass


class DomainAssigner(ABC):
    """
    Abstract base class for custom domain assignment.
    
    Implement this to define custom user-to-domain assignment logic.
    
    Example:
        class MyAssigner(DomainAssigner):
            def assign(self, user_id, domains):
                # Assign to least loaded domain
                min_load = float('inf')
                best_domain = 0
                for domain_id, domain in domains.items():
                    if domain.load_factor < min_load:
                        min_load = domain.load_factor
                        best_domain = domain_id
                return best_domain
    """
    
    @abstractmethod
    def assign(self, user_id: int, domains: Dict[int, Domain]) -> int:
        """
        Assign a user to a domain.
        
        Args:
            user_id: The user ID
            domains: Available domains
            
        Returns:
            Assigned domain ID
        """
        pass


# ==================== Utility Functions ====================

def load_algorithm_config(config_path: str) -> Dict[str, Any]:
    """
    Load algorithm configuration from JSON file.
    
    Args:
        config_path: Path to configuration file
        
    Returns:
        Configuration dictionary
    """
    with open(config_path, 'r') as f:
        return json.load(f)


def save_algorithm_config(config: Dict[str, Any], config_path: str) -> None:
    """
    Save algorithm configuration to JSON file.
    
    Args:
        config: Configuration dictionary
        config_path: Path to save configuration
    """
    with open(config_path, 'w') as f:
        json.dump(config, f, indent=2)


def calculate_anomaly_score(stats: TrafficStats, 
                            baseline: TrafficStats,
                            weights: Dict[str, float] = None) -> float:
    """
    Calculate anomaly score by comparing current stats to baseline.
    
    Args:
        stats: Current traffic statistics
        baseline: Baseline statistics
        weights: Optional weights for each metric
        
    Returns:
        Anomaly score (0.0 to 1.0)
    """
    if weights is None:
        weights = {
            'packet_rate': 0.4,
            'byte_rate': 0.3,
            'connections': 0.2,
            'latency': 0.1
        }
    
    score = 0.0
    
    # Packet rate deviation
    if baseline.packet_rate > 0:
        rate_dev = abs(stats.packet_rate - baseline.packet_rate) / baseline.packet_rate
        score += weights['packet_rate'] * min(rate_dev, 1.0)
    
    # Byte rate deviation
    if baseline.byte_rate > 0:
        byte_dev = abs(stats.byte_rate - baseline.byte_rate) / baseline.byte_rate
        score += weights['byte_rate'] * min(byte_dev, 1.0)
    
    # Connection count deviation
    if baseline.active_connections > 0:
        conn_dev = abs(stats.active_connections - baseline.active_connections) / baseline.active_connections
        score += weights['connections'] * min(conn_dev, 1.0)
    
    # Latency deviation
    if baseline.avg_latency > 0:
        lat_dev = abs(stats.avg_latency - baseline.avg_latency) / baseline.avg_latency
        score += weights['latency'] * min(lat_dev, 1.0)
    
    return min(score, 1.0)


def get_high_risk_users(state: SimulationState, 
                        min_level: RiskLevel = RiskLevel.HIGH) -> List[int]:
    """
    Get users with risk level at or above specified level.
    
    Args:
        state: Simulation state
        min_level: Minimum risk level to include
        
    Returns:
        List of user IDs
    """
    result = []
    for user_id, score in state.user_scores.items():
        if score.risk_level.value >= min_level.value:
            result.append(user_id)
    return result


def get_domain_metrics_summary(state: SimulationState) -> Dict[int, Dict[str, float]]:
    """
    Get summary metrics for all domains.
    
    Args:
        state: Simulation state
        
    Returns:
        Dictionary mapping domain_id to metrics dict
    """
    result = {}
    for domain_id, domain in state.domains.items():
        metrics = {
            'user_count': len(domain.user_ids),
            'proxy_count': len(domain.proxy_ids),
            'load_factor': domain.load_factor,
            'shuffle_frequency': domain.shuffle_frequency,
            'avg_user_score': 0.0,
            'high_risk_count': 0
        }
        
        # Calculate average score and high risk count
        scores = []
        for user_id in domain.user_ids:
            if user_id in state.user_scores:
                user_score = state.user_scores[user_id]
                scores.append(user_score.current_score)
                if user_score.risk_level.value >= RiskLevel.HIGH.value:
                    metrics['high_risk_count'] += 1
        
        if scores:
            metrics['avg_user_score'] = sum(scores) / len(scores)
        
        result[domain_id] = metrics
    
    return result


# ==================== Example Implementations ====================

class SimpleThresholdAlgorithm(DefenseAlgorithm):
    """
    Simple threshold-based defense algorithm.
    
    Triggers shuffles when average domain score exceeds threshold.
    """
    
    def __init__(self, threshold: float = 0.6):
        super().__init__("SimpleThreshold")
        self.threshold = threshold
        self.parameters['threshold'] = threshold
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        decisions = []
        domain_metrics = get_domain_metrics_summary(state)
        
        for domain_id, metrics in domain_metrics.items():
            if metrics['avg_user_score'] > self.threshold:
                decisions.append(DefenseDecision.trigger_shuffle(
                    domain_id,
                    ShuffleMode.SCORE_DRIVEN,
                    f"Average score {metrics['avg_user_score']:.2f} > {self.threshold}"
                ))
                self._logger.info(f"Triggered shuffle for domain {domain_id}")
        
        return decisions


class AdaptiveFrequencyAlgorithm(DefenseAlgorithm):
    """
    Adaptive frequency algorithm that adjusts shuffle rate based on risk.
    """
    
    def __init__(self, base_frequency: float = 30.0,
                 min_frequency: float = 5.0,
                 max_frequency: float = 120.0):
        super().__init__("AdaptiveFrequency")
        self.base_frequency = base_frequency
        self.min_frequency = min_frequency
        self.max_frequency = max_frequency
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        decisions = []
        domain_metrics = get_domain_metrics_summary(state)
        
        for domain_id, metrics in domain_metrics.items():
            domain = state.domains[domain_id]
            
            # Calculate risk factor
            risk_factor = metrics['avg_user_score'] + 0.1 * metrics['high_risk_count']
            
            # Calculate new frequency (higher risk = more frequent shuffles)
            new_frequency = self.base_frequency / (1 + risk_factor)
            new_frequency = max(self.min_frequency, min(self.max_frequency, new_frequency))
            
            # Only update if significantly different
            if abs(new_frequency - domain.shuffle_frequency) > 1.0:
                decisions.append(DefenseDecision.change_frequency(
                    domain_id,
                    new_frequency,
                    f"Risk factor {risk_factor:.2f}"
                ))
        
        return decisions


class IsolationAlgorithm(DefenseAlgorithm):
    """
    Isolation algorithm that migrates high-risk users to separate domains.
    """
    
    def __init__(self, isolation_domain_name: str = "isolation"):
        super().__init__("Isolation")
        self.isolation_domain_name = isolation_domain_name
        self.isolation_domain_id: Optional[int] = None
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        decisions = []
        
        # Find or identify isolation domain
        for domain_id, domain in state.domains.items():
            if domain.name == self.isolation_domain_name:
                self.isolation_domain_id = domain_id
                break
        
        if self.isolation_domain_id is None:
            self._logger.warning("Isolation domain not found")
            return decisions
        
        # Find high-risk users not in isolation domain
        high_risk_users = get_high_risk_users(state, RiskLevel.HIGH)
        
        for user_id in high_risk_users:
            # Check current domain
            current_domain = None
            for domain_id, domain in state.domains.items():
                if user_id in domain.user_ids:
                    current_domain = domain_id
                    break
            
            if current_domain != self.isolation_domain_id:
                decisions.append(DefenseDecision.migrate_user(
                    user_id,
                    self.isolation_domain_id,
                    f"Isolating high-risk user {user_id}"
                ))
        
        return decisions


# ==================== Factory Function ====================

def create_algorithm(algorithm_type: str, **kwargs) -> DefenseAlgorithm:
    """
    Factory function to create defense algorithms.
    
    Args:
        algorithm_type: Type of algorithm ('threshold', 'adaptive', 'isolation')
        **kwargs: Algorithm-specific parameters
        
    Returns:
        Defense algorithm instance
    """
    algorithms = {
        'threshold': SimpleThresholdAlgorithm,
        'adaptive': AdaptiveFrequencyAlgorithm,
        'isolation': IsolationAlgorithm,
    }
    
    if algorithm_type not in algorithms:
        raise ValueError(f"Unknown algorithm type: {algorithm_type}")
    
    return algorithms[algorithm_type](**kwargs)
