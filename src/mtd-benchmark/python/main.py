#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""
main.py - Command-Line Entry Point
===================================

This is the main entry point for running MTD defense simulations
from the command line.

Usage:
    python3 main.py --algorithm pdd [options]

Options:
    --algorithm, -a   Algorithm name (required): pdd, threshold, adaptive
    --users, -u       Number of users (default: 100)
    --proxies, -p     Number of proxies (default: 5)
    --domains         Number of domains (default: 1)
    --attackers       Number of attackers (default: 1)
    --threshold, -t   Ban threshold score (default: 10)
    --duration, -d    Simulation duration in seconds (default: 60.0)
    --seed, -s        Random seed (default: 42)
    --config, -c      JSON config file path
    --output, -o      Output JSON file path

Examples:
    # Basic PDD simulation
    python3 main.py -a pdd --users 100 --proxies 5 --duration 30
    
    # Multi-attacker scenario
    python3 main.py -a pdd --attackers 3 --domains 2 --proxies 10
    
    # Load from config file
    python3 main.py -a pdd --config experiment.json --output results.json

Output:
    JSON results to stdout (or file if --output specified)
    Log files to ns3-dev root: mtd_pdd_results_*.{json,csv}
"""

import argparse
import json
import sys
from typing import Dict, Any


def run_pdd(config: Dict[str, Any]) -> Dict[str, Any]:
    """Run PDD algorithm and return results."""
    from mtd_api import create_simulation_context
    from algorithm.pdd import PDDAlgorithm
    
    ctx = create_simulation_context(
        num_users=config.get('num_users', 100),
        num_proxies=config.get('num_proxies', 5),
        num_domains=config.get('num_domains', 1),
        num_attackers=config.get('num_attackers', 1)
    )
    ctx.set_random_seed(config.get('seed', 42))
    
    pdd = PDDAlgorithm(ban_threshold=config.get('ban_threshold', 10))
    pdd.initialize()
    ctx.set_defense_evaluator(pdd.evaluate)
    
    ctx.run(duration=config.get('duration', 60.0))
    
    results = {
        'simulation': ctx.get_results(),
        'algorithm': pdd.get_statistics(),
        'success': pdd.is_attacker_banned(),
        'banned_users': list(pdd.banned_users),
    }
    
    ctx.export_results(config.get('output_prefix', 'mtd_pdd_results'))
    
    return results


def main():
    """Main entry point."""
    parser = argparse.ArgumentParser(description='MTD-Benchmark Runner')
    parser.add_argument('--algorithm', '-a', required=True, choices=['pdd', 'threshold', 'adaptive'])
    parser.add_argument('--users', '-u', type=int, default=100)
    parser.add_argument('--proxies', '-p', type=int, default=5)
    parser.add_argument('--domains', type=int, default=1, help='Number of domains')
    parser.add_argument('--attackers', type=int, default=1, help='Number of attackers')
    parser.add_argument('--threshold', '-t', type=int, default=10)
    parser.add_argument('--duration', '-d', type=float, default=60.0)
    parser.add_argument('--seed', '-s', type=int, default=42)
    parser.add_argument('--config', '-c', type=str, help='JSON config file')
    parser.add_argument('--output', '-o', type=str, help='Output JSON file')
    args = parser.parse_args()
    
    config = {
        'num_users': args.users,
        'num_proxies': args.proxies,
        'num_domains': args.domains,
        'num_attackers': args.attackers,
        'ban_threshold': args.threshold,
        'duration': args.duration,
        'seed': args.seed,
    }
    
    if args.config:
        with open(args.config, 'r') as f:
            config.update(json.load(f))
    
    if args.algorithm == 'pdd':
        results = run_pdd(config)
    else:
        sys.exit(f"Unknown algorithm: {args.algorithm}")
    
    # Output results
    output = json.dumps(results, indent=2)
    if args.output:
        with open(args.output, 'w') as f:
            f.write(output)
    else:
        print(output)


if __name__ == '__main__':
    main()
