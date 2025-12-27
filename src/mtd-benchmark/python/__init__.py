#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
MTD-Benchmark Python Package

This package provides Python interfaces for the NS-3 MTD-Benchmark module,
enabling researchers to implement and test defense algorithms in Python.

Usage:
    from mtd_benchmark_py import DefenseAlgorithm, SimulationState, DefenseDecision
    
    class MyAlgorithm(DefenseAlgorithm):
        def evaluate(self, state):
            # Your algorithm logic
            return []
"""

__version__ = "1.0.0"
__author__ = "MTD-Benchmark Team"

# Import from base module
from .mtd_defense import (
    # Enums
    RiskLevel,
    ShuffleMode,
    AttackType,
    ActionType,
    
    # Data classes
    TrafficStats,
    DetectionObservation,
    UserScore,
    Domain,
    MtdEvent,
    SimulationState,
    DefenseDecision,
    
    # Base classes
    DefenseAlgorithm,
    ScoreCalculator,
    RiskClassifier,
    ShuffleStrategy,
    DomainAssigner,
    
    # Utility functions
    load_algorithm_config,
    save_algorithm_config,
    calculate_anomaly_score,
    get_high_risk_users,
    get_domain_metrics_summary,
    
    # Example implementations
    SimpleThresholdAlgorithm,
    AdaptiveFrequencyAlgorithm,
    IsolationAlgorithm,
    create_algorithm,
)

__all__ = [
    # Version
    '__version__',
    '__author__',
    
    # Enums
    'RiskLevel',
    'ShuffleMode',
    'AttackType',
    'ActionType',
    
    # Data classes
    'TrafficStats',
    'DetectionObservation',
    'UserScore',
    'Domain',
    'MtdEvent',
    'SimulationState',
    'DefenseDecision',
    
    # Base classes
    'DefenseAlgorithm',
    'ScoreCalculator',
    'RiskClassifier',
    'ShuffleStrategy',
    'DomainAssigner',
    
    # Utility functions
    'load_algorithm_config',
    'save_algorithm_config',
    'calculate_anomaly_score',
    'get_high_risk_users',
    'get_domain_metrics_summary',
    
    # Example implementations
    'SimpleThresholdAlgorithm',
    'AdaptiveFrequencyAlgorithm',
    'IsolationAlgorithm',
    'create_algorithm',
]
