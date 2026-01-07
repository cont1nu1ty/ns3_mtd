#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
algorithm/__init__.py - Defense Algorithm Package
=================================================

This package contains all defense algorithm implementations.
Each algorithm should inherit from DefenseAlgorithm base class.

Available Algorithms:
    - PDDAlgorithm: Proactive Detection Defense
        File: pdd.py
        5-step cycle: Perception → Scoring → MTD → Ban → Terminate

Adding New Algorithms:
    1. Create new file: algorithm/my_algorithm.py
    2. Implement class inheriting DefenseAlgorithm
    3. Import here: from .my_algorithm import MyAlgorithm
    4. Add to __all__: __all__ = ['PDDAlgorithm', 'MyAlgorithm']
    5. Register in main.py for CLI support
"""

from .pdd import PDDAlgorithm

__all__ = ['PDDAlgorithm']
