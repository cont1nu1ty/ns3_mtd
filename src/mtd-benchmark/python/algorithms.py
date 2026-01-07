#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
algorithms.py - Defense Algorithm Entry Point
============================================

This module serves as the main entry point for all defense algorithms.
It re-exports algorithm classes for convenient access and provides
backwards compatibility with NS-3 integration.

Usage:
    # Import specific algorithm
    from algorithms import PDDAlgorithm
    
    # Or run via NS-3
    ./ns3 run "mtd-python-integration --algorithm=algorithms.py"

Available Algorithms:
    - PDDAlgorithm: Proactive Detection Defense (5-step cycle)
    - (More algorithms can be added here)
"""

import sys
import os

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

from algorithm.pdd import PDDAlgorithm, main

PDDDefenseAlgorithm = PDDAlgorithm  # Legacy alias

__all__ = ['PDDAlgorithm', 'PDDDefenseAlgorithm', 'main']

if __name__ == '__main__':
    main()
