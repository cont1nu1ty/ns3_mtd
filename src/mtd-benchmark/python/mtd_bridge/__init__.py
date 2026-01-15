"""MTD-Python-Bridge (Cppyy): Python algorithms driving ns-3 MTD-Benchmark.

This package is staged into `build/bindings/python` for source-tree development.

Hard constraints (enforced in code):
- No per-packet cross-language hooks: only coarse-grained events/actions.
- Minimum control-loop tick interval.
"""

from .constraints import MIN_TICK_INTERVAL_MS, MAX_EVENTS_PER_TICK
from .runner import BridgeRunner
from .types import Algorithm, BridgeContext
from .api import MtdApi
from .log_export import (
    find_latest_run,
    load_events_jsonl,
    load_attack_events,
    load_score_events,
    load_timeline,
    export_to_csv,
    export_to_json,
    get_run_metadata,
)
from .examples import (
    SimpleThresholdAlgorithm,
    AdaptiveShuffleAlgorithm,
    LoadBalancingAlgorithm,
)

__all__ = [
    # Core
    "Algorithm",
    "BridgeContext",
    "BridgeRunner",
    "MtdApi",
    # Constraints
    "MIN_TICK_INTERVAL_MS",
    "MAX_EVENTS_PER_TICK",
    # Log export
    "find_latest_run",
    "load_events_jsonl",
    "load_attack_events",
    "load_score_events",
    "load_timeline",
    "export_to_csv",
    "export_to_json",
    "get_run_metadata",
    # Example algorithms
    "SimpleThresholdAlgorithm",
    "AdaptiveShuffleAlgorithm",
    "LoadBalancingAlgorithm",
]
