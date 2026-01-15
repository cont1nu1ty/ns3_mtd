"""Log export utilities for MTD-Benchmark.

Provides functions to read and export log data from the logs/ directory
for post-simulation analysis.
"""

from __future__ import annotations

import json
import glob
from pathlib import Path
from typing import Optional, Iterator
import pandas as pd


def find_latest_run(log_dir: str = "logs") -> Optional[Path]:
    """Find the latest run directory.

    Args:
        log_dir: Base log directory

    Returns:
        Path to latest run directory or None
    """
    log_path = Path(log_dir)
    if not log_path.exists():
        return None

    # Find all timestamped directories
    runs = sorted(log_path.glob("*/"), reverse=True)
    return runs[0] if runs else None


def load_events_jsonl(run_dir: Path) -> pd.DataFrame:
    """Load events.jsonl as pandas DataFrame.

    Args:
        run_dir: Path to run directory

    Returns:
        DataFrame with columns: timestamp, type, typeId, sourceNodeId, metadata
    """
    jsonl_path = run_dir / "events.jsonl"
    if not jsonl_path.exists():
        return pd.DataFrame()

    events = []
    with open(jsonl_path) as f:
        for line in f:
            if line.strip():
                events.append(json.loads(line))

    if not events:
        return pd.DataFrame()

    df = pd.DataFrame(events)
    
    # Expand metadata column
    if "metadata" in df.columns:
        metadata_df = pd.json_normalize(df["metadata"])
        df = pd.concat([df.drop("metadata", axis=1), metadata_df], axis=1)

    return df


def load_attack_events(run_dir: Path) -> pd.DataFrame:
    """Load attack events from attack/ directory.

    Args:
        run_dir: Path to run directory

    Returns:
        DataFrame with attack events
    """
    attack_dir = run_dir / "attack"
    if not attack_dir.exists():
        return pd.DataFrame()

    events = []
    for log_file in attack_dir.glob("*.log"):
        with open(log_file) as f:
            for line in f:
                if "groundTruth=" in line:
                    # Extract JSON from log line
                    start = line.find("groundTruth=") + len("groundTruth=")
                    json_str = line[start:].strip()
                    try:
                        gt = json.loads(json_str)
                        events.append(gt)
                    except json.JSONDecodeError:
                        pass

    return pd.DataFrame(events) if events else pd.DataFrame()


def load_score_events(run_dir: Path) -> pd.DataFrame:
    """Load score update events from score/ directory.

    Args:
        run_dir: Path to run directory

    Returns:
        DataFrame with score events
    """
    score_file = run_dir / "score" / "score_updated.log"
    if not score_file.exists():
        return pd.DataFrame()

    events = []
    with open(score_file) as f:
        for line in f:
            # Parse log format: [timestamp] SCORE_UPDATED node=X {key=val, ...}
            if "SCORE_UPDATED" not in line:
                continue

            # Extract timestamp
            if "[" in line and "]" in line:
                ts_start = line.find("[") + 1
                ts_end = line.find("]")
                timestamp = float(line[ts_start:ts_end].replace("ms", ""))

            # Extract metadata
            if "{" in line and "}" in line:
                meta_start = line.find("{") + 1
                meta_end = line.find("}")
                meta_str = line[meta_start:meta_end]
                metadata = {}
                for pair in meta_str.split(","):
                    if "=" in pair:
                        k, v = pair.split("=", 1)
                        metadata[k.strip()] = v.strip()

            events.append({
                "timestamp": timestamp,
                **metadata,
            })

    return pd.DataFrame(events) if events else pd.DataFrame()


def load_timeline(run_dir: Path) -> pd.DataFrame:
    """Load timeline_all.log as DataFrame.

    Args:
        run_dir: Path to run directory

    Returns:
        DataFrame with timeline events
    """
    timeline_file = run_dir / "timeline_all.log"
    if not timeline_file.exists():
        return pd.DataFrame()

    events = []
    with open(timeline_file) as f:
        for line in f:
            parts = line.strip().split(" ", 1)
            if len(parts) == 2:
                events.append({
                    "timestamp": float(parts[0]),
                    "event": parts[1],
                })

    return pd.DataFrame(events) if events else pd.DataFrame()


def export_to_csv(df: pd.DataFrame, output_path: str) -> None:
    """Export DataFrame to CSV.

    Args:
        df: DataFrame to export
        output_path: Output file path
    """
    df.to_csv(output_path, index=False)


def export_to_json(df: pd.DataFrame, output_path: str) -> None:
    """Export DataFrame to JSON.

    Args:
        df: DataFrame to export
        output_path: Output file path
    """
    df.to_json(output_path, orient="records", indent=2)


def get_run_metadata(run_dir: Path) -> dict:
    """Load system_meta.json and run_stats.json.

    Args:
        run_dir: Path to run directory

    Returns:
        Combined metadata dict
    """
    metadata = {}

    meta_file = run_dir / "system_meta.json"
    if meta_file.exists():
        with open(meta_file) as f:
            metadata.update(json.load(f))

    stats_file = run_dir / "run_stats.json"
    if stats_file.exists():
        with open(stats_file) as f:
            metadata["stats"] = json.load(f)

    return metadata


__all__ = [
    "find_latest_run",
    "load_events_jsonl",
    "load_attack_events",
    "load_score_events",
    "load_timeline",
    "export_to_csv",
    "export_to_json",
    "get_run_metadata",
]
