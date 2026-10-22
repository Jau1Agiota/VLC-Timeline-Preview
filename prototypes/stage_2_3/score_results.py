#!/usr/bin/env python3
"""GPL-2.0-or-later. Score manually observed, visibly burned image timestamps."""
import argparse
import csv
import json
import math
from pathlib import Path


def score(rows):
    counts = dict(passed=0, inaccurate=0, extraction_failed=0, unreviewed=0)
    errors = []
    for row in rows:
        if row["exit_code"] != "0" or row.get("external_timeout", "False").lower() == "true":
            counts["extraction_failed"] += 1
            continue
        if not row["observed_ms"] or row["observation_method"] != "visible_pts":
            counts["unreviewed"] += 1
            continue
        observed = float(row["observed_ms"])
        target = float(row["requested_ms"])
        if not math.isfinite(observed) or not math.isfinite(target) or min(observed, target) < 0:
            raise ValueError("Timestamps must be finite and nonnegative")
        # Ensure each manually-reviewed image still exists. Reviewer remains
        # responsible for actually reading its PTS; this is not OCR validation.
        error = abs(observed - target)
        errors.append(error)
        counts["passed" if error <= 250 else "inaccurate"] += 1
    return {**counts, "reviewed_abs_error_ms": errors,
            "maximum_abs_error_ms": max(errors) if errors else None,
            "accuracy_gate_passed": bool(rows) and counts["passed"] == len(rows)}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("csv", type=Path)
    args = parser.parse_args()
    with args.csv.open(encoding="utf-8-sig", newline="") as stream:
        rows = list(csv.DictReader(stream))
    for row in rows:
        if row["exit_code"] == "0" and not (args.csv.parent / row["image"]).is_file():
            parser.error(f"Missing candidate image: {row['image']}")
    result = score(rows)
    print(json.dumps(result, indent=2))
    raise SystemExit(0 if result["accuracy_gate_passed"] else 1)


if __name__ == "__main__":
    main()
