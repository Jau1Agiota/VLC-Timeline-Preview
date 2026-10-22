#!/usr/bin/env python3
"""GPL-2.0-or-later. Private helper timing; never a visual/PTS acceptance test."""
import argparse
import json
import hashlib
import signal
import struct
import queue
import threading
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


def persistent_samples(helper, media, output, samples, profile, wine):
    """One outstanding request; bounded reader queue; no decoder/player reuse."""
    def win(path):
        return "Z:"+str(path).replace("/","\\") if wine else str(path)
    command=(["wine"] if wine else [])+[str(helper),"--worker",win(media),win(output),profile]
    started=time.perf_counter()
    child=subprocess.Popen(command,cwd=helper.parent,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                           stderr=subprocess.PIPE,start_new_session=wine)
    responses=queue.Queue(maxsize=2)
    def receive():
        while True:
            line=child.stdout.readline(4097)
            try: responses.put_nowait(line)
            except queue.Full:
                child.kill();return
            if not line:return
    def discard():
        while child.stderr.read(4096):pass
    reader=threading.Thread(target=receive,daemon=True);reader.start()
    errors=threading.Thread(target=discard,daemon=True);errors.start()
    rows=[]
    try:
        for index in range(samples):
            target=(1000,4750,8000)[index%3]
            if index:started=time.perf_counter()
            child.stdin.write(f"{index+1} {target} 5000\n".encode("ascii"));child.stdin.flush()
            line=responses.get(timeout=12)
            elapsed=(time.perf_counter()-started)*1000
            if not line or len(line)>4096:raise ValueError("worker EOF/output bound")
            record=json.loads(line)
            image=output.read_bytes()
            if (record.get("exit_code")!=0 or record.get("request_id")!=index+1 or
                record.get("requested_ms")!=target or record.get("frame_timestamp_verified") is not False or
                not 54<len(image)<=320*180*4+54 or image[:2]!=b"BM"):
                raise ValueError("worker protocol failure")
            rows.append({"mode":"worker_cold" if index==0 else "worker_warm", "profile":profile,
                         "file":media.name,"index":index,"requested_ms":target,
                         "process_wall_ms":elapsed,"helper":record,
                         "bmp_sha256":hashlib.sha256(image).hexdigest()})
            output.unlink()
        child.stdin.close();child.wait(timeout=7)
        if child.returncode:raise ValueError("worker failed during clean shutdown")
    finally:
        if child.poll() is None:
            if wine:os.killpg(child.pid,signal.SIGKILL)
            else:child.kill()
            child.wait(timeout=5)
        reader.join(timeout=2);errors.join(timeout=2)
        child.stdout.close();child.stderr.close()
        if not child.stdin.closed:child.stdin.close()
    return rows


def benchmark(helper, fixtures, output, samples, wine=False, notice=False, profiles=("baseline", "low-cache", "fast-seek", "hw-none", "hw-auto"),
              fixtures_names=("ordinary",), persistent=False):
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
    if persistent:
        with tempfile.TemporaryDirectory(prefix="vlc-worker-perf-") as temporary:
            for profile in profiles:
                for extension in ("mp4","mkv"):
                    for fixture in fixtures_names:
                        rows.extend(persistent_samples(helper,fixtures/(fixture+"."+extension),
                                    Path(temporary)/"frame.bmp",samples,profile,wine))
    summary = {}
    for profile in profiles:
        group = [r for r in rows if r["profile"] == profile and r["mode"] == "one_shot"]
        summary[profile] = {name: stats([r[name] for r in group]) for name in ("process_wall_ms","outside_main_ms")}
        for name in ("capture_latency_ms", "total_latency_ms", "libvlc_init_ms", "media_player_create_ms",
                     "play_call_ms", "decode_wait_ms", "stop_release_ms", "bmp_output_ms"):
            summary[profile][name] = stats([r["helper"][name] for r in group])
    identical = {}
    reference = {(r["file"],r["index"]):r["bmp_sha256"] for r in rows if r["profile"] == "baseline" and r["mode"] == "one_shot"}
    for profile in profiles:
        group = [r for r in rows if r["profile"] == profile and r["mode"] == "one_shot"]
        identical[profile] = {"n":len(group), "byte_identical_to_baseline":sum(
            r["bmp_sha256"] == reference.get((r["file"],r["index"])) for r in group)}
    workers={}
    for mode in ("worker_cold","worker_warm"):
        for profile in profiles:
            group=[r for r in rows if r["mode"]==mode and r["profile"]==profile]
            if not group:continue
            workers[profile+"/"+mode]={"request_wall_ms":stats([r["process_wall_ms"] for r in group]),
                "capture_ms":stats([r["helper"]["capture_latency_ms"] for r in group]),
                "helper_total_ms":stats([r["helper"]["total_latency_ms"] for r in group]),
                "init_ms":stats([r["helper"]["libvlc_init_ms"] for r in group]),
                "byte_identical_to_baseline":sum(r["bmp_sha256"]==reference.get((r["file"],r["index"])) for r in group)}
    cpu = platform.processor()
    if wine and Path("/proc/cpuinfo").exists():
        cpu = next((line.split(":",1)[1].strip() for line in Path("/proc/cpuinfo").read_text().splitlines()
                    if line.startswith("model name")), cpu)
    data = {"environment": {"platform": platform.platform(), "processor": cpu,
                            "wine": wine, "logical_cpus": os.cpu_count()},
            "method": "New process/LibVLC per sample; OS caches not flushed. First sample separately identifiable; no visible UI measurement.",
            "frame_timestamp_verified": False, "rows": rows, "summary": summary, "candidate_comparison":identical,"worker_summary":workers}
    output.parent.mkdir(parents=True, exist_ok=True)
    with output.open("x") as stream:
        json.dump(data, stream, indent=2)
    print(json.dumps(summary))
    if notice:
        print("::notice title=Persistent worker timing::" + json.dumps(workers))
        print("::notice title=Benchmark environment::" + json.dumps(data["environment"]))
        print("::notice title=Candidate comparison (not PTS verification)::" + json.dumps(identical))
        print("::notice title=Thumbnail timing::" + json.dumps(summary, separators=(",", ":")))


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--helper", required=True)
    parser.add_argument("--fixtures", required=True)
    parser.add_argument("--output", required=True)
    parser.add_argument("--samples", type=int, default=6)
    parser.add_argument("--persistent", action="store_true")
    parser.add_argument("--profiles", nargs="+", choices=("baseline","low-cache","fast-seek","hw-none","hw-auto"), default=("baseline","low-cache","fast-seek","hw-none","hw-auto"))
    parser.add_argument("--all-fixtures", action="store_true")
    parser.add_argument("--wine", action="store_true")
    parser.add_argument("--github-notice", action="store_true")
    args = parser.parse_args()
    if not 3 <= args.samples <= 50:
        parser.error("samples must be between 3 and 50 per container")
    benchmark(args.helper, args.fixtures, args.output, args.samples, args.wine, args.github_notice, profiles=args.profiles, persistent=args.persistent, fixtures_names=("ordinary", "bframes", "long_gop", "vfr") if args.all_fixtures else ("ordinary",))
