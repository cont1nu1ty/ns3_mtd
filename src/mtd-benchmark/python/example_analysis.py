#!/usr/bin/env python3
"""Example: Post-simulation log analysis.

This demonstrates how to load and analyze log data from a simulation run.
"""

import sys
from pathlib import Path

# Add current directory to path (mtd_bridge is in the same directory)
sys.path.insert(0, str(Path(__file__).parent))

try:
    import pandas as pd
    from mtd_bridge.log_export import (
        find_latest_run,
        load_events_jsonl,
        load_attack_events,
        load_score_events,
        load_timeline,
        get_run_metadata,
        export_to_csv,
    )
except ImportError as e:
    print(f"Error importing: {e}")
    print("Install required packages:")
    print("  pip install pandas")
    sys.exit(1)


def main():
    """Analyze latest simulation run."""
    # Find latest run
    run_dir = find_latest_run("logs")
    if run_dir is None:
        print("No log directory found. Run a simulation first.")
        return

    print(f"Analyzing run: {run_dir}")

    # Load metadata
    metadata = get_run_metadata(run_dir)
    print(f"\n=== Run Metadata ===")
    print(f"Start time: {metadata.get('startTime', 'N/A')}")
    print(f"Total events: {metadata.get('stats', {}).get('totalEvents', 'N/A')}")

    # Load events
    print("\n=== Loading Events ===")
    events_df = load_events_jsonl(run_dir)
    print(f"Total events: {len(events_df)}")
    if not events_df.empty:
        print(f"Event types: {events_df['type'].value_counts().to_dict()}")

    # Load attack events
    print("\n=== Attack Events ===")
    attacks_df = load_attack_events(run_dir)
    print(f"Attack records: {len(attacks_df)}")
    if not attacks_df.empty:
        print("\nAttack Summary:")
        print(attacks_df[["type", "ratePps", "packetSize", "durationActual", "packetsSent"]].to_string())

    # Load score events
    print("\n=== Score Events ===")
    scores_df = load_score_events(run_dir)
    print(f"Score updates: {len(scores_df)}")
    if not scores_df.empty and "score" in scores_df.columns:
        print(f"Average score: {scores_df['score'].astype(float).mean():.3f}")
        print(f"Max score: {scores_df['score'].astype(float).max():.3f}")

    # Load timeline
    print("\n=== Timeline ===")
    timeline_df = load_timeline(run_dir)
    print(f"Timeline events: {len(timeline_df)}")
    if not timeline_df.empty:
        print("\nFirst 10 events:")
        print(timeline_df.head(10).to_string())

    # Export to CSV for further analysis
    output_dir = Path("analysis_output")
    output_dir.mkdir(exist_ok=True)

    if not events_df.empty:
        export_to_csv(events_df, output_dir / "events.csv")
        print(f"\nExported events to {output_dir / 'events.csv'}")

    if not attacks_df.empty:
        export_to_csv(attacks_df, output_dir / "attacks.csv")
        print(f"Exported attacks to {output_dir / 'attacks.csv'}")

    if not scores_df.empty:
        export_to_csv(scores_df, output_dir / "scores.csv")
        print(f"Exported scores to {output_dir / 'scores.csv'}")


if __name__ == "__main__":
    main()
