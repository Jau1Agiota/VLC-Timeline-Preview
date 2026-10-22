#!/usr/bin/env python3
"""GPL-2.0-or-later. Private helper timing; never a visual/PTS acceptance test."""
import argparse
import json
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time


def stats(values):
    values = sorted(values)
    return {"n": len(values), "median_ms": statistics.median(values),
            "p95_ms": values[math.ceil(.95 * len(values)) - 1]}


def benchmark(helper, fixtures, output, samples, wine=False, notice=False):
    helper, fixtures, output = Path(helper).resolve(), Path(fixtures).resolve(), Path(output)
    if output.exists():
        raise ValueError("output must be new")
    rows = []
    def win(path):
        return "Z:" + str(path).replace("/", "\\") if wine else str(path)
    with tempfile.TemporaryDirectory(prefix="vlc-perf-") as temporary:
        for extension in ("mp4", "mkv"):
            for index in range(samples):
                target = (1000, 4750, 8000)[index % 3]
                bmp = Path(temporary) / f"{extension}-{index}.bmp"
                command = (["wine"] if wine else []) + [str(helper), win(fixtures / ("ordinary." + extension)),
                           str(target), win(bmp), "5000"]
                started = time.perf_counter()
                process = subprocess.run(command, cwd=helper.parent, capture_output=True, timeout=20)
                wall = (time.perf_counter() - started) * 1000
                record = json.loads(process.stdout)
                if process.returncode or not bmp.is_file() or record.get("frame_timestamp_verified") is not False:
                    raise ValueError("helper failed or violated candidate protocol")
                rows.append({"mode": "one_shot", "file": "ordinary." + extension, "index": index,
                             "requested_ms": target, "process_wall_ms": wall, "helper": record})
                bmp.unlink()
    summary = {"one_shot": {"process_wall_ms": stats([r["process_wall_ms"] for r in rows])}}
    for name in ("capture_latency_ms", "total_latency_ms", "libvlc_init_ms", "media_player_create_ms",
                 "play_call_ms", "decode_wait_ms", "stop_release_ms", "bmp_output_ms"):
        summary["one_shot"][name] = stats([r["helper"][name] for r in rows])
    data = {"environment": {"platform": platform.platform(), "processor": platform.processor(),
                            "wine": wine, "logical_cpus": os.cpu_count()},
            "method": "New process/LibVLC per sample; OS caches not flushed. First sample separately identifiable; no visible UI measurement.",
            "frame_timestamp_verified": False, "rows": rows, "summary": summary}
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x") as stream:
        json.dump(data, stream, indent=2)
    print(json.dumps(summary))
    if notice:
        print("::notice title=Thumbnail timing::" + json.dumps(summary, separators=(",", ":")))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", required=True)
    parser.add_argument("--fixtures", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--samples", type=int, default=6)
    parser.add_argument("--wine", action="store_true")
    parser.add_argument("--github-notice", action="store_true")
    args = parser.parse_args()
    if not 3 <= args.samples <= 50:
        parser.error("samples must be between 3 and 50 per container")
    benchmark(args.helper, args.fixtures, args.output, args.samples, args.wine, args.github_notice)
