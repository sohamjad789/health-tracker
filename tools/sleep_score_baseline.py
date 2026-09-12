#!/usr/bin/env python3
"""Create a transparent reference sleep score from PhysioNet PSG labels.

This is a baseline for the health-tracker project, not a clinical score and not
a sleep-stage classifier. It uses lab-scored labels to establish the scoring
formula before live motion and PPG data are available.
"""

from __future__ import annotations

import argparse
import json
from pathlib import Path

EPOCH_SECONDS = 30
SLEEP_STAGES = {1, 2, 3, 5}  # N1, N2, N3, REM in the PhysioNet dataset


def read_labels(path: Path) -> list[tuple[float, int]]:
    labels: list[tuple[float, int]] = []
    for line_number, line in enumerate(path.read_text().splitlines(), start=1):
        fields = line.split()
        if len(fields) != 2:
            raise ValueError(f"{path}:{line_number}: expected timestamp and stage")
        labels.append((float(fields[0]), int(fields[1])))
    if not labels:
        raise ValueError(f"{path} contains no labels")
    return labels


def contiguous_wake_minutes(stages: list[int]) -> float:
    first_sleep = next((i for i, stage in enumerate(stages) if stage in SLEEP_STAGES), None)
    last_sleep = next((i for i in range(len(stages) - 1, -1, -1) if stages[i] in SLEEP_STAGES), None)
    if first_sleep is None or last_sleep is None:
        return 0.0
    return sum(stage == 0 for stage in stages[first_sleep:last_sleep + 1]) * EPOCH_SECONDS / 60


def sleep_latency_minutes(stages: list[int]) -> float | None:
    first_sleep = next((i for i, stage in enumerate(stages) if stage in SLEEP_STAGES), None)
    return None if first_sleep is None else first_sleep * EPOCH_SECONDS / 60


def score_labels(labels: list[tuple[float, int]]) -> dict[str, float | int | None]:
    # -1 is unscored in this dataset and must not count as wake or time in bed.
    stages = [stage for _, stage in labels if stage != -1]
    if not stages:
        raise ValueError("no scored epochs available")

    sleep_epochs = sum(stage in SLEEP_STAGES for stage in stages)
    total_minutes = len(stages) * EPOCH_SECONDS / 60
    sleep_minutes = sleep_epochs * EPOCH_SECONDS / 60
    efficiency_percent = 100 * sleep_epochs / len(stages)
    latency_minutes = sleep_latency_minutes(stages)
    waso_minutes = contiguous_wake_minutes(stages)

    # 0-100 wellness score with deliberate, inspectable weights.
    duration_points = min(sleep_minutes / 480, 1.0) * 40
    efficiency_points = (efficiency_percent / 100) * 30
    continuity_points = max(0.0, 1 - waso_minutes / 60) * 20
    latency_points = 0.0 if latency_minutes is None else max(0.0, 1 - latency_minutes / 60) * 10

    return {
        "sleep_score": round(duration_points + efficiency_points + continuity_points + latency_points),
        "sleep_minutes": round(sleep_minutes, 1),
        "scored_minutes": round(total_minutes, 1),
        "sleep_efficiency_percent": round(efficiency_percent, 1),
        "sleep_latency_minutes": latency_minutes,
        "wake_after_sleep_onset_minutes": round(waso_minutes, 1),
        "duration_points_out_of_40": round(duration_points, 1),
        "efficiency_points_out_of_30": round(efficiency_points, 1),
        "continuity_points_out_of_20": round(continuity_points, 1),
        "latency_points_out_of_10": round(latency_points, 1),
    }


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("labels", type=Path, help="PhysioNet *_labeled_sleep.txt file")
    args = parser.parse_args()
    print(json.dumps(score_labels(read_labels(args.labels)), indent=2))


if __name__ == "__main__":
    main()
