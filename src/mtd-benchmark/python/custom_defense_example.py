#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MTD-Benchmark Example: Custom Defense Algorithm

This example demonstrates how to implement a custom defense algorithm
using the MTD-Benchmark Python interface.

Usage:
    python custom_defense_example.py --config config.json

Authors: MTD-Benchmark Team
"""

import argparse
import json
import logging
from typing import List, Dict, Any

# Import MTD defense base classes
from mtd_defense import (
    DefenseAlgorithm, ScoreCalculator, RiskClassifier, ShuffleStrategy,
    SimulationState, DefenseDecision, DetectionObservation, UserScore, Domain,
    RiskLevel, ShuffleMode, ActionType, TrafficStats,
    get_high_risk_users, get_domain_metrics_summary
)

# Configure logging
logging.basicConfig(level=logging.INFO)
logger = logging.getLogger('custom_defense')


# ==================== Custom Score Calculator ====================

class ExponentialMovingAverageScorer(ScoreCalculator):
    """
    Score calculator using exponential moving average.
    
    score_new = alpha * observation_score + (1 - alpha) * current_score
    """
    
    def __init__(self, alpha: float = 0.3,
                 rate_weight: float = 0.5,
                 pattern_weight: float = 0.3,
                 persistence_weight: float = 0.2):
        self.alpha = alpha
        self.rate_weight = rate_weight
        self.pattern_weight = pattern_weight
        self.persistence_weight = persistence_weight
    
    def calculate(self, user_id: int, observation: DetectionObservation,
                  current_score: float) -> float:
        # Calculate observation score
        obs_score = (
            self.rate_weight * observation.rate_anomaly +
            self.pattern_weight * observation.pattern_anomaly +
            self.persistence_weight * observation.persistence_factor
        )
        
        # Apply EMA
        new_score = self.alpha * obs_score + (1 - self.alpha) * current_score
        
        # Clamp to [0, 1]
        return max(0.0, min(1.0, new_score))


# ==================== Custom Risk Classifier ====================

class TieredRiskClassifier(RiskClassifier):
    """
    Risk classifier with configurable tiers and VIP user handling.
    """
    
    def __init__(self, 
                 low_threshold: float = 0.25,
                 medium_threshold: float = 0.50,
                 high_threshold: float = 0.75,
                 vip_users: List[int] = None):
        self.low_threshold = low_threshold
        self.medium_threshold = medium_threshold
        self.high_threshold = high_threshold
        self.vip_users = set(vip_users) if vip_users else set()
    
    def classify(self, user_id: int, score: float) -> RiskLevel:
        # VIP users get more lenient classification
        if user_id in self.vip_users:
            if score > 0.95:
                return RiskLevel.CRITICAL
            if score > 0.85:
                return RiskLevel.HIGH
            return RiskLevel.LOW
        
        # Standard classification
        if score > self.high_threshold:
            return RiskLevel.CRITICAL
        if score > self.medium_threshold:
            return RiskLevel.HIGH
        if score > self.low_threshold:
            return RiskLevel.MEDIUM
        return RiskLevel.LOW


# ==================== Custom Shuffle Strategy ====================

class RiskAwareShuffleStrategy(ShuffleStrategy):
    """
    Shuffle strategy that isolates high-risk users to dedicated proxies.
    """
    
    def __init__(self, isolation_ratio: float = 0.2):
        """
        Args:
            isolation_ratio: Ratio of proxies reserved for high-risk users
        """
        self.isolation_ratio = isolation_ratio
    
    def select_proxy(self, user_id: int, proxy_ids: List[int],
                     user_score: UserScore) -> int:
        if not proxy_ids:
            raise ValueError("No proxies available")
        
        num_proxies = len(proxy_ids)
        isolation_count = max(1, int(num_proxies * self.isolation_ratio))
        
        if user_score.risk_level in [RiskLevel.HIGH, RiskLevel.CRITICAL]:
            # High-risk users go to isolation proxies (last N proxies)
            isolation_proxies = proxy_ids[-isolation_count:]
            return isolation_proxies[user_id % len(isolation_proxies)]
        else:
            # Normal users use the remaining proxies
            normal_proxies = proxy_ids[:-isolation_count] if isolation_count < num_proxies else proxy_ids
            return normal_proxies[user_id % len(normal_proxies)]


# ==================== Custom Defense Algorithm ====================

class MultiLevelDefenseAlgorithm(DefenseAlgorithm):
    """
    Multi-level defense algorithm combining multiple strategies.
    
    Features:
    - Threshold-based shuffle triggering
    - Adaptive frequency adjustment
    - High-risk user isolation
    - Proactive defense based on attack patterns
    """
    
    def __init__(self,
                 shuffle_threshold: float = 0.6,
                 base_frequency: float = 30.0,
                 min_frequency: float = 5.0,
                 max_frequency: float = 120.0,
                 isolation_threshold: RiskLevel = RiskLevel.HIGH):
        super().__init__("MultiLevelDefense")
        
        self.shuffle_threshold = shuffle_threshold
        self.base_frequency = base_frequency
        self.min_frequency = min_frequency
        self.max_frequency = max_frequency
        self.isolation_threshold = isolation_threshold
        
        # State tracking
        self.last_shuffle_time: Dict[int, float] = {}
        self.domain_risk_history: Dict[int, List[float]] = {}
        self.attack_detected = False
        
    def initialize(self, parameters: Dict[str, Any] = None) -> None:
        super().initialize(parameters)
        if parameters:
            self.shuffle_threshold = parameters.get('shuffle_threshold', self.shuffle_threshold)
            self.base_frequency = parameters.get('base_frequency', self.base_frequency)
            self.min_frequency = parameters.get('min_frequency', self.min_frequency)
            self.max_frequency = parameters.get('max_frequency', self.max_frequency)
    
    def evaluate(self, state: SimulationState) -> List[DefenseDecision]:
        """
        Main evaluation logic combining multiple defense strategies.
        """
        decisions = []
        current_time = state.time_seconds
        
        # Get domain metrics
        domain_metrics = get_domain_metrics_summary(state)
        
        for domain_id, metrics in domain_metrics.items():
            domain = state.domains[domain_id]
            
            # Update risk history
            if domain_id not in self.domain_risk_history:
                self.domain_risk_history[domain_id] = []
            self.domain_risk_history[domain_id].append(metrics['avg_user_score'])
            
            # Keep only recent history
            max_history = 100
            if len(self.domain_risk_history[domain_id]) > max_history:
                self.domain_risk_history[domain_id] = self.domain_risk_history[domain_id][-max_history:]
            
            # Strategy 1: Threshold-based shuffle
            if metrics['avg_user_score'] > self.shuffle_threshold:
                last_shuffle = self.last_shuffle_time.get(domain_id, 0)
                if current_time - last_shuffle > self.min_frequency:
                    decisions.append(DefenseDecision.trigger_shuffle(
                        domain_id,
                        ShuffleMode.SCORE_DRIVEN,
                        f"Avg score {metrics['avg_user_score']:.2f} > threshold {self.shuffle_threshold}"
                    ))
                    self.last_shuffle_time[domain_id] = current_time
                    self._logger.info(f"Triggered shuffle for domain {domain_id}")
            
            # Strategy 2: Adaptive frequency
            new_frequency = self._calculate_adaptive_frequency(domain_id, metrics)
            if abs(new_frequency - domain.shuffle_frequency) > 2.0:
                decisions.append(DefenseDecision.change_frequency(
                    domain_id, new_frequency,
                    f"Adaptive frequency adjustment based on risk trend"
                ))
            
            # Strategy 3: Detect attack patterns
            if self._detect_attack_pattern(domain_id, state):
                if not self.attack_detected:
                    self.attack_detected = True
                    self._logger.warning(f"Attack pattern detected in domain {domain_id}")
                    decisions.append(DefenseDecision.trigger_shuffle(
                        domain_id,
                        ShuffleMode.ATTACKER_AVOID,
                        "Attack pattern detected"
                    ))
        
        # Strategy 4: User isolation
        isolation_decisions = self._evaluate_user_isolation(state)
        decisions.extend(isolation_decisions)
        
        return decisions
    
    def _calculate_adaptive_frequency(self, domain_id: int, 
                                       metrics: Dict[str, float]) -> float:
        """Calculate adaptive shuffle frequency based on risk"""
        risk_history = self.domain_risk_history.get(domain_id, [])
        
        if len(risk_history) < 5:
            return self.base_frequency
        
        # Calculate trend
        recent = risk_history[-5:]
        trend = (recent[-1] - recent[0]) / 5 if len(recent) >= 2 else 0
        
        # Current risk factor
        risk_factor = metrics['avg_user_score'] + 0.1 * metrics['high_risk_count']
        
        # Adjust frequency based on trend
        if trend > 0.1:  # Rising risk
            risk_factor *= 1.5
        elif trend < -0.1:  # Falling risk
            risk_factor *= 0.7
        
        # Calculate new frequency
        new_frequency = self.base_frequency / (1 + risk_factor)
        return max(self.min_frequency, min(self.max_frequency, new_frequency))
    
    def _detect_attack_pattern(self, domain_id: int, state: SimulationState) -> bool:
        """Detect potential attack patterns based on observations"""
        # Check proxy observations
        for proxy_id, obs in state.observations.items():
            if obs.confidence > 0.8 and obs.rate_anomaly > 0.7:
                return True
        
        # Check risk history for sudden spikes
        history = self.domain_risk_history.get(domain_id, [])
        if len(history) >= 3:
            recent_change = history[-1] - history[-3]
            if recent_change > 0.3:  # Sudden increase
                return True
        
        return False
    
    def _evaluate_user_isolation(self, state: SimulationState) -> List[DefenseDecision]:
        """Evaluate and isolate high-risk users"""
        decisions = []
        
        high_risk_users = get_high_risk_users(state, self.isolation_threshold)
        
        # Find domains with space
        domain_capacities = {}
        for domain_id, domain in state.domains.items():
            capacity = 500 - len(domain.user_ids)  # Assuming max 500 users per domain
            domain_capacities[domain_id] = capacity
        
        for user_id in high_risk_users[:5]:  # Limit migrations per evaluation
            current_domain = None
            for domain_id, domain in state.domains.items():
                if user_id in domain.user_ids:
                    current_domain = domain_id
                    break
            
            if current_domain is not None:
                # Find a domain with fewer high-risk users
                best_domain = current_domain
                min_risk_count = float('inf')
                
                domain_metrics = get_domain_metrics_summary(state)
                for domain_id, metrics in domain_metrics.items():
                    if domain_id != current_domain and domain_capacities.get(domain_id, 0) > 0:
                        if metrics['high_risk_count'] < min_risk_count:
                            min_risk_count = metrics['high_risk_count']
                            best_domain = domain_id
                
                if best_domain != current_domain:
                    decisions.append(DefenseDecision.migrate_user(
                        user_id, best_domain,
                        f"Distributing high-risk user {user_id}"
                    ))
        
        return decisions
    
    def get_statistics(self) -> Dict[str, float]:
        """Return algorithm statistics"""
        stats = {
            'attack_detected': 1.0 if self.attack_detected else 0.0,
            'domains_tracked': len(self.domain_risk_history),
        }
        
        # Average risk across all domains
        all_risks = []
        for history in self.domain_risk_history.values():
            if history:
                all_risks.append(history[-1])
        if all_risks:
            stats['avg_current_risk'] = sum(all_risks) / len(all_risks)
        
        return stats
    
    def reset(self) -> None:
        super().reset()
        self.last_shuffle_time.clear()
        self.domain_risk_history.clear()
        self.attack_detected = False


# ==================== Main Entry Point ====================

def create_defense_components(config: Dict[str, Any]):
    """
    Create all defense components from configuration.
    
    Returns:
        Tuple of (algorithm, scorer, classifier, strategy)
    """
    # Create score calculator
    scorer_config = config.get('scorer', {})
    scorer = ExponentialMovingAverageScorer(
        alpha=scorer_config.get('alpha', 0.3),
        rate_weight=scorer_config.get('rate_weight', 0.5),
        pattern_weight=scorer_config.get('pattern_weight', 0.3),
        persistence_weight=scorer_config.get('persistence_weight', 0.2)
    )
    
    # Create risk classifier
    classifier_config = config.get('classifier', {})
    classifier = TieredRiskClassifier(
        low_threshold=classifier_config.get('low_threshold', 0.25),
        medium_threshold=classifier_config.get('medium_threshold', 0.50),
        high_threshold=classifier_config.get('high_threshold', 0.75),
        vip_users=classifier_config.get('vip_users', [])
    )
    
    # Create shuffle strategy
    strategy_config = config.get('strategy', {})
    strategy = RiskAwareShuffleStrategy(
        isolation_ratio=strategy_config.get('isolation_ratio', 0.2)
    )
    
    # Create main algorithm
    algorithm_config = config.get('algorithm', {})
    algorithm = MultiLevelDefenseAlgorithm(
        shuffle_threshold=algorithm_config.get('shuffle_threshold', 0.6),
        base_frequency=algorithm_config.get('base_frequency', 30.0),
        min_frequency=algorithm_config.get('min_frequency', 5.0),
        max_frequency=algorithm_config.get('max_frequency', 120.0)
    )
    algorithm.initialize(algorithm_config)
    
    return algorithm, scorer, classifier, strategy


def main():
    parser = argparse.ArgumentParser(description='Custom Defense Algorithm Example')
    parser.add_argument('--config', type=str, default=None,
                        help='Path to configuration file')
    parser.add_argument('--threshold', type=float, default=0.6,
                        help='Shuffle threshold')
    args = parser.parse_args()
    
    # Load or create configuration
    if args.config:
        with open(args.config, 'r') as f:
            config = json.load(f)
    else:
        config = {
            'algorithm': {
                'shuffle_threshold': args.threshold,
                'base_frequency': 30.0,
            },
            'scorer': {
                'alpha': 0.3,
            },
            'classifier': {
                'low_threshold': 0.25,
            },
            'strategy': {
                'isolation_ratio': 0.2,
            }
        }
    
    # Create components
    algorithm, scorer, classifier, strategy = create_defense_components(config)
    
    logger.info(f"Created defense algorithm: {algorithm.name}")
    logger.info(f"Configuration: {json.dumps(config, indent=2)}")
    
    # Example: Simulate evaluation
    # In actual use, this would be called by the NS-3 simulation
    example_state = SimulationState(
        current_time=1000000000,  # 1 second
        domains={
            1: Domain(1, "Domain1", [1, 2, 3], [1, 2, 3, 4, 5], 0.5, 30.0)
        },
        user_scores={
            1: UserScore(1, 0.3, RiskLevel.LOW, 0),
            2: UserScore(2, 0.7, RiskLevel.HIGH, 0),
            3: UserScore(3, 0.2, RiskLevel.LOW, 0),
        },
        proxy_stats={
            1: TrafficStats(1000, 800, 500000, 400000, 100.0, 50000.0, 50, 5.0)
        },
        observations={
            1: DetectionObservation(0.3, 0.2, 0.1, 0.0)
        }
    )
    
    # Evaluate
    decisions = algorithm.evaluate(example_state)
    
    logger.info(f"Generated {len(decisions)} decisions:")
    for decision in decisions:
        logger.info(f"  - {decision.action.name}: {decision.reason}")
    
    # Get statistics
    stats = algorithm.get_statistics()
    logger.info(f"Statistics: {stats}")


if __name__ == '__main__':
    main()
