#!/usr/bin/env python3
"""GPL-2.0-or-later. Private helper timing; never a visual/PTS acceptance test."""
import argparse
import json
import hashlib
import signal
import struct
import math
import os
from pathlib import Path
import platform
import statistics
import subprocess
import tempfile
import time


def stats(values):
    if not values:
        raise ValueError("empty sample")
    values = sorted(values)
    return {"n": len(values), "median_ms": statistics.median(values),
            "p95_ms": values[math.ceil(.95 * len(values)) - 1]}


def benchmark(helper, fixtures, output, samples, wine=False, notice=False, profiles=("baseline", "low-cache", "fast-seek", "hw-none", "hw-auto"),
              fixtures_names=("ordinary",)):
    helper, fixtures, output = Path(helper).resolve(), Path(fixtures).resolve(), Path(output)
    if output.exists():
        raise ValueError("output must be new")
    rows = []
    def win(path):
        return "Z:" + str(path).replace("/", "\\") if wine else str(path)
    with tempfile.TemporaryDirectory(prefix="vlc-perf-") as temporary:
        for profile in profiles:
            for extension in ("mp4", "mkv"):
                for fixture in fixtures_names:
                    for index in range(samples):
                        target = (1000, 4750, 8000)[index % 3]
                        bmp = Path(temporary) / f"{profile}-{fixture}-{extension}-{index}.bmp"
                        command = (["wine"] if wine else []) + [str(helper), win(fixtures / (fixture + "." + extension)),
                                   str(target), win(bmp), "5000", profile]
                        started = time.perf_counter()
                        child = subprocess.Popen(command, cwd=helper.parent, stdout=subprocess.PIPE,
                                                 stderr=subprocess.PIPE, start_new_session=wine)
                        try:
                            stdout, _ = child.communicate(timeout=20)
                        except subprocess.TimeoutExpired:
                            if wine:
                                os.killpg(child.pid, signal.SIGKILL)
                            else:
                                child.kill()
                            child.communicate()
                            raise ValueError("helper exceeded external 20-second bound")
                        wall = (time.perf_counter() - started) * 1000
                        record = json.loads(stdout)
                        if child.returncode or not bmp.is_file() or record.get("frame_timestamp_verified") is not False:
                            raise ValueError("helper failed or violated candidate protocol")
                        image = bmp.read_bytes()
                        if not 54 < len(image) <= 320*180*4+54 or image[:2] != b"BM":
                            raise ValueError("unbounded/invalid BMP")
                        width, height = struct.unpack_from("<ii",image,18)
                        if not (0 < width <= 320 and 0 < abs(height) <= 180 and
                                record.get("width") == width and record.get("height") == abs(height) and
                                record.get("requested_ms") == target and record.get("status") in
                                ("candidate_captured","fallback_candidate_captured")):
                            raise ValueError("helper request/image/protocol mismatch")
                        rows.append({"mode": "one_shot", "profile": profile, "file": fixture + "." + extension, "index": index,
                                     "requested_ms": target, "process_wall_ms": wall,
                                     "outside_main_ms": max(0, wall-record.get("entry_to_result_ms",wall)),
                                     "bmp_sha256": hashlib.sha256(image).hexdigest(), "helper": record})
                        bmp.unlink()
    summary = {}
    for profile in profiles:
        group = [r for r in rows if r["profile"] == profile]
        summary[profile] = {name: stats([r[name] for r in group]) for name in ("process_wall_ms","outside_main_ms")}
        for name in ("capture_latency_ms", "total_latency_ms", "libvlc_init_ms", "media_player_create_ms",
                     "play_call_ms", "decode_wait_ms", "stop_release_ms", "bmp_output_ms"):
            summary[profile][name] = stats([r["helper"][name] for r in group])
    identical = {}
    reference = {(r["file"],r["index"]):r["bmp_sha256"] for r in rows if r["profile"] == "baseline"}
    for profile in profiles:
        group = [r for r in rows if r["profile"] == profile]
        identical[profile] = {"n":len(group), "byte_identical_to_baseline":sum(
            r["bmp_sha256"] == reference.get((r["file"],r["index"])) for r in group)}
    cpu = platform.processor()
    if wine and Path("/proc/cpuinfo").exists():
        cpu = next((line.split(":",1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                    if line.startswith("model name")), cpu)
    data = {"environment": {"platform": platform.platform(), "processor": cpu,
                            "wine": wine, "logical_cpus": os.cpu_count()},
            "method": "New process/LibVLC per sample; OS caches not flushed. First sample separately identifiable; no visible UI measurement.",
            "frame_timestamp_verified": False, "rows": rows, "summary": summary, "candidate_comparison":identical}
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x") as stream:
        json.dump(data, stream, indent=2)
    print(json.dumps(summary))
    if notice:
        print("::notice title=Benchmark environment::" + json.dumps(data["environment"]))
        print("::notice title=Candidate comparison (not PTS verification)::" + json.dumps(identical))
        print("::notice title=Thumbnail timing::" + json.dumps(summary, separators=(",", ":")))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", required=True)
    parser.add_argument("--fixtures", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--samples", type=int, default=6)
    parser.add_argument("--all-fixtures", action="store_true")
    parser.add_argument("--wine", action="store_true")
    parser.add_argument("--github-notice", action="store_true")
    args = parser.parse_args()
    if not 3 <= args.samples <= 50:
        parser.error("samples must be between 3 and 50 per container")
    benchmark(args.helper, args.fixtures, args.output, args.samples, args.wine, args.github_notice, fixtures_names=("ordinary", "bframes", "long_gop", "vfr") if args.all_fixtures else ("ordinary",))
