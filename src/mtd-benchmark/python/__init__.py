#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
__init__.py - MTD-Benchmark Python Package Root
===============================================

This is the main package initializer for mtd-benchmark Python interface.
It exports all public APIs, data structures, and algorithm classes.

Package Structure:
    mtd_defense.py  - Base classes and data structures
    mtd_api.py      - Simulation context and C++ bindings
    main.py         - Command-line entry point
    algorithms.py   - Algorithm export entry point
    algorithm/      - Algorithm implementations directory

Usage:
    from mtd_benchmark.python import (
        create_simulation_context,
        PDDAlgorithm,
        DefenseDecision,
        ShuffleMode,
    )

Note:
    All simulation logging is handled by C++ backend via 'reason' parameters.
    Python code should NOT use print/logging for simulation events.
"""

__version__ = "2.0.0"

from .mtd_defense import (
    RiskLevel, ShuffleMode, AttackType, ActionType,
    TrafficStats, DetectionObservation, UserScore, Domain,
    MtdEvent, SimulationState, DefenseDecision,
    DefenseAlgorithm,
)

from .mtd_api import (
    create_simulation_context,
    is_cpp_module_available,
    MockSimulationContext,
    EventType,
)

from .algorithm import PDDAlgorithm

__all__ = [
    '__version__',
    'create_simulation_context',
    'is_cpp_module_available',
    'MockSimulationContext',
    'RiskLevel', 'ShuffleMode', 'AttackType', 'ActionType', 'EventType',
    'TrafficStats', 'DetectionObservation', 'UserScore', 'Domain',
    'MtdEvent', 'SimulationState', 'DefenseDecision',
    'DefenseAlgorithm',
    'PDDAlgorithm',
]
