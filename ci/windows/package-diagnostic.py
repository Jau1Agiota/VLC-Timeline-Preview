#!/usr/bin/env python3
"""GPL-2.0-or-later. CI-1 packaging, verification, bounded smoke tests and telemetry.
No credentials or GitHub resource APIs. All outputs belong to the new pilot run.
"""
import argparse
import hashlib
import json
import os
from pathlib import Path, PurePosixPath
import signal
import resource
import stat
import struct
import sys
import subprocess
import time
import tempfile
import zipfile

STAGE = os.environ.get("VLC_CI_STAGE", "2.4.3")
SOURCE = {"2.4.3": "2f7cb820e893c8dc53e40cddbab9c57a2fe1a0fc",
          "2.4.4": "e879ce656c218eddec13926f03b5f0615d14dc61",
          "2.4.5": "d10df7f0a54326f122f54b2a2961a3e1b0ee1f66"}[STAGE]
IMAGE = "registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7"
CONTRIB = "910cc9cada6080dd777fbafb2ebdd0c24f702c30"
CONTRIB_SHA = "49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c"
SDK_SHA = "1ed59c09152e78aff84663fa76efab26057716576db74332858810e5ef54ae1f"
BINARIES = ("vlc.exe", "libvlc.dll", "libvlccore.dll", "plugins/gui/libqt_plugin.dll",
            "thumbnail-preview/thumbnail-helper.exe", "thumbnail-preview/libvlc.dll",
            "thumbnail-preview/libvlccore.dll")
MARKER = b"timeline-diagnostic slider="
PERF_MARKER = "request_to_present_ms=".encode("utf-16le")
README = (("Stage 2.4.4 fullscreen preview correction; Windows visual verification pending.\r\n"
           if STAGE == "2.4.4" else "Stage 2.4.5 performance experiment; Windows acceptance pending.\r\n" if STAGE == "2.4.5" else "Stage 2.4.3 CI-1 diagnostic pilot; no fullscreen fix.\r\n") +
          "Set VLC_TIMELINE_HOVER_DEBUG=1 before launching vlc.exe with --verbose=2.\r\n"
          "Keep thumbnail-preview/ intact: it is a separate official VLC runtime.\r\n"
          "Native Windows GUI/audio/fullscreen and frame PTS accuracy are unverified.\r\n"
          "Build provenance: BUILD_MANIFEST.json. Do not overwrite previous packages.\r\n").encode()


def require(condition, message):
    if not condition:
        raise ValueError(message)


def digest(path):
    h = hashlib.sha256()
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            h.update(chunk)
    return h.hexdigest()


def json_bytes(data):
    return (json.dumps(data, indent=2, sort_keys=True) + "\n").encode()


def write_new(path, data):
    with Path(path).open("xb") as stream:
        stream.write(data)


def atomic_json(path, data):
    """Keep the previous record intact until a complete replacement is ready."""
    path = Path(path)
    body = json_bytes(data)
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(dir=path.parent, prefix=".ci1-", delete=False) as stream:
            temporary = Path(stream.name)
            stream.write(body)
            stream.flush()
            os.fsync(stream.fileno())
        os.replace(temporary, path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def safe_name(name):
    p = PurePosixPath(name)
    require(bool(name) and not p.is_absolute() and ".." not in p.parts and
            "\\" not in name and ":" not in name and str(p) == name,
            "unsafe/noncanonical archive path")
    return name


def pe64(path):
    with Path(path).open("rb") as stream:
        require(stream.read(2) == b"MZ", "missing DOS signature")
        stream.seek(0x3C)
        offset_data = stream.read(4)
        require(len(offset_data) == 4, "truncated DOS header")
        offset = struct.unpack("<I", offset_data)[0]
        require(64 <= offset <= 1024 * 1024, "invalid PE header offset")
        stream.seek(offset)
        header = stream.read(26)
    require(len(header) == 26 and header[:4] == b"PE\0\0" and
            struct.unpack_from("<H", header, 4)[0] == 0x8664 and
            struct.unpack_from("<H", header, 24)[0] == 0x20B, "expected PE32+ x64 binary")


def has_marker(path, marker=MARKER):
    tail = b""
    with Path(path).open("rb") as stream:
        for chunk in iter(lambda: stream.read(1024 * 1024), b""):
            data = tail + chunk
            if marker in data:
                return True
            tail = data[-len(marker):]
    return False


def package(staging, output, metadata_path):
    staging, output = Path(staging).resolve(), Path(output).resolve()
    require(staging.is_dir() and not output.exists(), "staging must exist and output must be new")
    require(output != staging and staging not in output.parents, "output cannot be inside staging")
    metadata = json.loads(Path(metadata_path).read_text())
    require(metadata["source_commit"] == SOURCE and metadata["image"] == IMAGE and
            metadata["contrib_commit"] == CONTRIB and metadata["contrib_sha256"] == CONTRIB_SHA and
            metadata["sdk_sha256"] == SDK_SHA, "unexpected build provenance")
    for key in ("run_id", "run_attempt"):
        require(str(metadata[key]).isdigit(), "invalid run identity")
    files = {}
    omitted = {"plugins/plugins.dat", "STAGE_2_4_2_SHA256SUMS"}
    for path in sorted(staging.rglob("*")):
        require(not path.is_symlink(), "symlinks are not permitted in delivery staging")
        if path.is_dir():
            continue
        require(path.is_file(), "nonregular staging input")
        name = safe_name(path.relative_to(staging).as_posix())
        require(not any(p in {".git", "win64", "wine-prefix", "downloads", "fixtures", "guard"}
                        for p in PurePosixPath(name).parts), "build material in delivery staging")
        if name not in omitted:
            files[name] = digest(path)
    require("BUILD_MANIFEST.json" not in files and "CI_1_README.txt" not in files, "reserved metadata path")
    for name in BINARIES:
        require(name in files, "missing required binary: " + name)
        pe64(staging / name)
    for name in ("COPYING.txt", "thumbnail-preview/COPYING.txt"):
        require(name in files, "missing license: " + name)
    require(any(n.startswith("thumbnail-preview/plugins/") and n.endswith(".dll") for n in files),
            "missing private runtime plugins")
    require(has_marker(staging / "plugins/gui/libqt_plugin.dll"), "missing Stage 2.4.3 instrumentation")
    if STAGE == "2.4.5":
        require(has_marker(staging / "plugins/gui/libqt_plugin.dll", PERF_MARKER),
                "missing Stage 2.4.5 Qt timing instrumentation")
        require(has_marker(staging / "thumbnail-preview/thumbnail-helper.exe", b"libvlc_init_ms"),
                "missing Stage 2.4.5 helper timing instrumentation")
    files["CI_1_README.txt"] = hashlib.sha256(README).hexdigest()
    manifest = dict(metadata, files_sha256=files, native_windows_gui_tested=False,
                    frame_pts_accuracy_verified=False, main_plugin_cache_omitted=True,
                    toolchain={"gcc": "6.4.0", "binutils": "2.27", "qt": "5.6.3"})
    body = json_bytes(manifest)
    name = f'VLC-3.0.24-stage{STAGE.replace(".", "")}-ci1-win64-{SOURCE[:12]}-{metadata["run_id"]}-{metadata["run_attempt"]}'
    output.mkdir()
    archive = output / (name + ".zip")
    with zipfile.ZipFile(archive, "x", zipfile.ZIP_DEFLATED, compresslevel=6, allowZip64=True) as z:
        for filename in sorted(files):
            if filename == "CI_1_README.txt":
                z.writestr(name + "/" + filename, README)
            else:
                z.write(staging / filename, name + "/" + filename)
        z.writestr(name + "/BUILD_MANIFEST.json", body)
    external = {"source_commit": SOURCE, "driver_commit": metadata["driver_commit"],
                "zip": archive.name, "root": name, "size_bytes": archive.stat().st_size,
                "sha256": digest(archive), "embedded_manifest_sha256": hashlib.sha256(body).hexdigest()}
    write_new(output / "PACKAGE_MANIFEST.json", json_bytes(external))
    write_new(output / "SHA256SUMS", (external["sha256"] + "  " + archive.name + "\n" +
              digest(output / "PACKAGE_MANIFEST.json") + "  PACKAGE_MANIFEST.json\n").encode())
    verify(output)
    print(json.dumps(external))


def verify(directory, expected_manifest=None):
    directory = Path(directory)
    manifest_path = directory / "PACKAGE_MANIFEST.json"
    if expected_manifest:
        require(manifest_path.read_bytes() == Path(expected_manifest).read_bytes(),
                "downloaded manifest differs from producer")
    record = json.loads(manifest_path.read_text())
    require(record["source_commit"] == SOURCE, "unexpected source")
    filename, root = safe_name(record["zip"]), safe_name(record["root"])
    require("/" not in filename and "/" not in root and filename == root + ".zip", "invalid package name")
    checks = {}
    for line in (directory / "SHA256SUMS").read_text().splitlines():
        sha, name = line.split("  ", 1)
        require(name in {filename, "PACKAGE_MANIFEST.json"} and name not in checks,
                "unexpected or duplicate checksum target")
        checks[name] = sha
    require(set(checks) == {filename, "PACKAGE_MANIFEST.json"}, "incomplete checksum file")
    for name, expected in checks.items():
        require(digest(directory / name) == expected, "SHA256 mismatch: " + name)
    archive = directory / filename
    require(archive.stat().st_size == record["size_bytes"] and checks[filename] == record["sha256"],
            "archive differs from manifest")
    with zipfile.ZipFile(archive) as z:
        names = z.namelist()
        require(len(names) == len(set(names)), "duplicate ZIP member")
        for info in z.infolist():
            safe_name(info.filename)
            require(info.filename.startswith(root + "/") and not stat.S_ISLNK(info.external_attr >> 16),
                    "invalid ZIP member")
        body = z.read(root + "/BUILD_MANIFEST.json")
        require(hashlib.sha256(body).hexdigest() == record["embedded_manifest_sha256"], "manifest hash mismatch")
        embedded = json.loads(body)
        require(embedded["source_commit"] == SOURCE and embedded["image"] == IMAGE and
                embedded["contrib_commit"] == CONTRIB and embedded["contrib_sha256"] == CONTRIB_SHA and
                embedded["sdk_sha256"] == SDK_SHA and embedded["driver_commit"] == record["driver_commit"],
                "embedded provenance mismatch")
        expected = {root + "/" + name for name in embedded["files_sha256"]}
        require(set(names) == expected | {root + "/BUILD_MANIFEST.json"}, "unexpected/missing ZIP contents")
        for name, expected_sha in embedded["files_sha256"].items():
            safe_name(name)
            h = hashlib.sha256()
            with z.open(root + "/" + name) as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    h.update(chunk)  # Reading every member also checks ZIP CRC.
            require(h.hexdigest() == expected_sha, "ZIP member hash mismatch: " + name)
    print("ZIP CRC, member hashes, provenance and external SHA256SUMS verified")


def read_number(path):
    try:
        return int(Path(path).read_text().strip())
    except (OSError, ValueError):
        return None


def snapshot(output, disks, phase=None, status=None):
    root = Path(output).parent.parent
    if phase is None:
        try:
            phase = (root / "current-phase").read_text().strip()
        except OSError:
            phase = "unknown"
    record = {"utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
              "monotonic_seconds": time.monotonic(), "phase": phase, "status": status, "disk": {}}
    for disk in disks:
        fs = os.statvfs(disk)
        record["disk"][disk] = {"available_bytes": fs.f_bavail * fs.f_frsize,
                                "total_bytes": fs.f_blocks * fs.f_frsize}
    for filename in ("memory.current", "memory.peak", "memory.max"):
        record["cgroup_" + filename.replace(".", "_")] = read_number("/sys/fs/cgroup/" + filename)
    for filename in ("memory.events", "cpu.stat"):
        try:
            values = dict(line.split() for line in Path("/sys/fs/cgroup/" + filename).read_text().splitlines())
            record["cgroup_" + filename.replace(".", "_")] = {k: int(v) for k, v in values.items()}
        except (OSError, ValueError):
            record["cgroup_" + filename.replace(".", "_")] = None
    mem = dict((l.split(":")[0], int(l.split()[1]) * 1024) for l in Path("/proc/meminfo").read_text().splitlines()
               if l.startswith(("MemTotal:", "MemAvailable:")))
    record["kernel_memory"] = mem  # VM-wide, not process RSS; labelled separately.
    with Path(output).open("a") as stream:
        stream.write(json.dumps(record) + "\n")


def process_identity(pid):
    try:
        fields = Path(f"/proc/{pid}/stat").read_text().rsplit(") ", 1)[1].split()
        return fields[19] if fields[0] != "Z" else None
    except (OSError, IndexError):
        return None


def monitor(output, stop, disks):
    atomic_json(str(output) + ".owner.json", {"pid": os.getpid(), "start": process_identity(os.getpid())})
    while True:
        snapshot(output, disks)
        if Path(stop).exists():
            break
        time.sleep(5)
    write_new(str(output) + ".done", b"complete\n")


def summarize(directory):
    directory = Path(directory)
    result = {"note": "Cgroup peaks cover cgroup lifetime; host kernel memory is VM-wide. Missing final samples after hard termination are not zero usage."}
    for scope in ("host", "container", "events"):
        path = directory / (scope + ".jsonl")
        rows = []
        if path.exists():
            for line in path.read_text().splitlines():
                try:
                    rows.append(json.loads(line))
                except ValueError:
                    pass  # A hard kill can truncate the last sample.
        if not rows:
            result[scope] = {"measurements_available": False}
            continue
        peaks = [r["cgroup_memory_peak"] for r in rows if r.get("cgroup_memory_peak") is not None]
        result[scope] = {"samples": len(rows), "measured_span_seconds": rows[-1]["monotonic_seconds"] - rows[0]["monotonic_seconds"],
                         "cgroup_peak_bytes": max(peaks) if peaks else None,
                         "minimum_disk_available_bytes": min(v["available_bytes"] for r in rows for v in r["disk"].values()),
                         "monitor_finished": Path(str(path) + ".done").exists(), "last_sample": rows[-1]}
    for name in ("run-status.json", "container-run-status.json", "docker-state.json"):
        try:
            result[name] = json.loads((directory / name).read_text())
        except (OSError, ValueError):
            result[name] = None
    # Include every teardown invocation; a later success must not erase an error.
    result["teardown"] = []
    try:
        for line in (directory / "teardown.jsonl").read_text().splitlines():
            try:
                result["teardown"].append(json.loads(line))
            except ValueError:
                pass
    except OSError:
        pass
    atomic_json(directory / "summary.json", result)


def reporting_owned(work, scope):
    work = Path(work)
    require(not work.is_symlink() and work.is_dir() and work.resolve() == work,
            "unowned/noncanonical reporting directory")
    marker = work / ".ci1-work"
    require(marker.is_file() and not marker.is_symlink(), "missing ownership marker")
    if scope == "host":
        expected = {"GITHUB_ACTIONS": "true", "RUNNER_ENVIRONMENT": "github-hosted",
                    "RUNNER_OS": "Linux", "RUNNER_ARCH": "X64", "ImageOS": "ubuntu24"}
        require(all(os.environ.get(k) == v for k, v in expected.items()), "not a hosted runner")
        require(work.parent == Path(os.environ["RUNNER_TEMP"]).resolve() and
                work.name == "vlc-ci1-" + os.environ["GITHUB_RUN_ID"] + "-" + os.environ["GITHUB_RUN_ATTEMPT"],
                "wrong run ownership")
    else:
        require(scope == "container" and os.environ.get("CI_INSIDE") == "1" and work == Path("/out"),
                "not an owned container output")
    container_id = work / "container.id"
    require(not container_id.is_symlink() and (not container_id.exists() or container_id.is_file()),
            "unsafe container identity file")
    metrics = work / "metrics"
    require(metrics.is_dir() and not metrics.is_symlink(), "unsafe/missing metrics directory")
    require(all(p.is_file() and not p.is_symlink() for p in metrics.iterdir()), "unsafe metrics file")


def stop_monitor(metrics, scope, supplied_pid, report, grace=6, escalation=2):
    output = metrics / (scope + ".jsonl")
    owner_path = Path(str(output) + ".owner.json")
    done = Path(str(output) + ".done")
    owner = json.loads(owner_path.read_text()) if owner_path.exists() else None
    require(owner is None or (isinstance(owner, dict) and "pid" in owner and "start" in owner and
            (owner["start"] is None or isinstance(owner["start"], str))),
            "invalid monitor ownership record; no signal sent")
    pid = owner["pid"] if owner else supplied_pid
    if pid is None:
        return  # Initialization failed before starting a monitor.
    require(isinstance(pid, int) and pid > 1 and pid != os.getpid(), "invalid monitor PID")
    start = owner["start"] if owner else process_identity(pid)

    def alive():
        return start is not None and process_identity(pid) == start

    if not alive():
        if not done.exists():
            report("monitor exited without completion evidence")
        return
    # Never signal a reused PID or a process unrelated to this exact monitor.
    try:
        command = Path(f"/proc/{pid}/cmdline").read_bytes().split(b"\0")
    except FileNotFoundError:
        if not alive():
            if not done.exists():
                report("monitor exited without completion evidence")
            return
        raise
    expected = [str(Path(__file__).resolve()).encode(), b"monitor", str(output).encode()]
    require(any(command[i:i + 3] == expected for i in range(len(command) - 2)),
            "monitor identity mismatch; no signal sent")
    try:
        (metrics / (scope + ".stop")).touch()
    except OSError as error:
        report("monitor stop marker: " + str(error))

    def await_exit(seconds):
        deadline = time.monotonic() + seconds
        while alive() and time.monotonic() < deadline:
            time.sleep(0.1)
        return not alive()

    if not await_exit(grace):
        report("monitor shutdown timeout; bounded signal escalation required")
        for sig in (signal.SIGTERM, signal.SIGKILL):
            if not alive():
                break
            try:
                os.kill(pid, sig)
            except ProcessLookupError:
                break
            if await_exit(escalation):
                break
    if alive():
        report("monitor remains alive after bounded shutdown; summary is incomplete")
    elif not done.exists():
        report("monitor stopped without completion evidence")


def finalize(work, scope, status=None, phase="before-driver", monitor_pid=None):
    work = Path(work)
    reporting_owned(work, scope)  # No reporting, Docker or signaling before validation.
    metrics = work / "metrics"
    errors = []
    started = time.monotonic()

    def report(message):
        errors.append(message)
        print("CI-1 teardown: " + message, file=sys.stderr, flush=True)
        # Surface bounded teardown evidence through Checks when log downloads are unavailable.
        if os.environ.get("GITHUB_ACTIONS") == "true" or os.environ.get("CI_INSIDE") == "1":
            escaped = message.replace("%", "%25").replace("\r", "%0D").replace("\n", "%0A")
            print("::warning title=CI teardown::" + escaped, flush=True)

    def attempt(label, operation):
        try:
            return operation()
        except (OSError, ValueError, subprocess.SubprocessError) as error:
            report(label + ": " + str(error))
            return None

    def command(argv, seconds, env=None):
        result = subprocess.run(argv, stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                timeout=seconds, check=False, env=env)
        require(result.returncode == 0, "command failed (exit " + str(result.returncode) + "): " +
                result.stderr.decode(errors="replace")[:500])
        return result.stdout

    def inspect(container):
        state = json.loads(command(["docker", "inspect", "--format", "{{json .State}}", container], 10))
        require(isinstance(state, dict) and isinstance(state.get("Running"), bool), "invalid Docker state")
        atomic_json(metrics / "docker-state.json", state)
        return state

    status_file = metrics / ("run-status.json" if scope == "host" else "container-run-status.json")
    if not status_file.exists():
        attempt("build status", lambda: atomic_json(status_file,
                {"exit_code": status, "phase": phase}))
    if status is not None:
        attempt("exit sample", lambda: snapshot(metrics / "events.jsonl", [str(work)], phase, status))
    if scope == "host" and (work / "container.id").exists():
        def read_container():
            container = (work / "container.id").read_text().strip()
            require(len(container) == 64 and all(c in "0123456789abcdef" for c in container),
                    "invalid container ID; no Docker operation attempted")
            return container
        container = attempt("container identity", read_container)
        if container is not None:
            state = attempt("Docker inspection (previous evidence retained on failure)", lambda: inspect(container))
            if state is None or state["Running"]:
                attempt("Docker stop", lambda: command(["docker", "stop", "--time", "15", container], 25))
                attempt("Docker final inspection (previous evidence retained on failure)", lambda: inspect(container))
    elif scope == "container" and (work / "wine-prefix").is_dir():
        def stop_wine():
            prefix = work / "wine-prefix"
            require(not prefix.is_symlink() and prefix.resolve() == prefix, "unsafe Wine prefix; no shutdown attempted")
            environment = dict(os.environ, WINEPREFIX=str(prefix))
            result = subprocess.run(["wineserver", "-k"], stdout=subprocess.PIPE, stderr=subprocess.PIPE,
                                    timeout=5, check=False, env=environment)
            # Wine 8 returns silent status 1 when the private server already exited.
            # A bounded -w must still prove shutdown; never accept diagnostics/errors.
            require(result.returncode == 0 or (result.returncode == 1 and not result.stdout and not result.stderr),
                    "private Wine kill failed (exit " + str(result.returncode) + "): " +
                    result.stderr.decode(errors="replace")[:500])
            command(["wineserver", "-w"], 5, environment)
        attempt("private Wine shutdown", stop_wine)
    attempt("monitor shutdown", lambda: stop_monitor(metrics, scope, monitor_pid, report))
    attempt("final sample", lambda: snapshot(metrics / (scope + ".jsonl"), [str(work)], "workflow-finish"))
    record = {"scope": scope, "build_exit_code": status, "phase": phase, "errors": errors,
              "wall_seconds": time.monotonic() - started}
    def record_teardown():
        with (metrics / "teardown.jsonl").open("a") as stream:
            stream.write(json.dumps(record) + "\n")
    attempt("teardown evidence", record_teardown)
    attempt("final summary", lambda: summarize(metrics))
    historical_errors = False
    try:
        for line in (metrics / "teardown.jsonl").read_text().splitlines():
            try:
                historical_errors |= bool(json.loads(line).get("errors"))
            except ValueError:
                historical_errors = True
    except OSError:
        historical_errors = True
    # The EXIT handler preserves the build status. The separate workflow step
    # must still fail if an earlier teardown invocation failed.
    return 1 if errors or historical_errors else 0


def smoke(runtime, fixtures, output):
    runtime, fixtures, output = Path(runtime), Path(fixtures), Path(output)
    output.mkdir()
    win = lambda p: "Z:" + str(p.resolve()).replace("/", "\\")
    results = []
    cases = [(f"ordinary.{ext}", ms, True) for ext in ("mp4", "mkv") for ms in (1000, 14750)]
    cases += [("corrupt.mp4", 1000, False), ("audio_only.mp4", 1000, False), ("missing.mp4", 1000, False)]
    # The minimum legal deadline exercises a short budget. Fast decodes may finish
    # inside it; record that honestly instead of claiming the watchdog fired.
    cases += [("ordinary.mp4", 1000, False)]
    for index, (name, ms, positive) in enumerate(cases):
        bmp = output / f"candidate-{index}.bmp"
        timeout_case = index == len(cases) - 1
        deadline = "100" if timeout_case else "5000"
        command = ["wine", str(runtime / "thumbnail-helper.exe"), win(fixtures / name), str(ms), win(bmp), deadline]
        started = time.monotonic()
        process = subprocess.Popen(command, cwd=runtime, stdout=subprocess.PIPE, stderr=subprocess.PIPE, start_new_session=True)
        try:
            stdout, stderr = process.communicate(timeout=15)
        except subprocess.TimeoutExpired:
            os.killpg(process.pid, signal.SIGKILL)
            stdout, stderr = process.communicate()
            (output / f"case-{index}.stdout").write_bytes(stdout)
            (output / f"case-{index}.stderr").write_bytes(stderr)
            results.append({"file": name, "requested_ms": ms, "exit": process.returncode,
                            "wall_seconds": time.monotonic() - started, "external_timeout": True,
                            "frame_timestamp_verified": False, "checks_passed": False})
            (output / "results.json").write_bytes(json_bytes(results))
            raise ValueError("helper exceeded external 15-second bound")
        (output / f"case-{index}.stdout").write_bytes(stdout)
        (output / f"case-{index}.stderr").write_bytes(stderr)
        results.append({"file": name, "requested_ms": ms, "exit": process.returncode,
                        "wall_seconds": time.monotonic() - started, "timeout_case": timeout_case,
                        "watchdog_triggered": timeout_case and process.returncode == 124,
                        "frame_timestamp_verified": False, "checks_passed": False})
        (output / "results.json").write_bytes(json_bytes(results))
        if positive or (timeout_case and process.returncode == 0):
            record = json.loads(stdout)
            require(process.returncode == 0 and record["requested_ms"] == ms and
                    record["status"] in ("candidate_captured", "fallback_candidate_captured") and
                    record["frame_timestamp_verified"] is False, "helper candidate protocol failure")
            require(54 < bmp.stat().st_size <= 320 * 180 * 4 + 54, "unbounded/invalid helper BMP")
            with bmp.open("rb") as stream:
                data = stream.read(54)
            require(data[:2] == b"BM", "invalid BMP signature")
            width, height = struct.unpack_from("<ii", data, 18)
            require(0 < width <= 320 and 0 < abs(height) <= 180 and width == record["width"] and
                    abs(height) == record["height"], "helper dimension mismatch")
        else:
            expected = {2} if name == "missing.mp4" else {3, 124}
            require(process.returncode == 124 if timeout_case else process.returncode in expected,
                    "unexpected negative/short-deadline helper result (including crash)")
        results[-1]["checks_passed"] = True
        (output / "results.json").write_bytes(json_bytes(results))


def timed(output, command):
    require(bool(command), "missing timed command")
    started = time.monotonic()
    status = None
    try:
        status = subprocess.call(command)
        return status
    finally:
        usage = resource.getrusage(resource.RUSAGE_CHILDREN)
        write_new(output, json_bytes({"command": command, "exit_code": status,
                  "wall_seconds": time.monotonic() - started,
                  "child_user_seconds": usage.ru_utime, "child_system_seconds": usage.ru_stime,
                  "largest_child_max_rss_bytes": usage.ru_maxrss * 1024,
                  "note": "Linux largest-child RSS, not concurrent-process total; use cgroup telemetry for aggregate."}))


def inputs(args):
    source = Path(args.source)
    tracked = ("modules/gui/qt/util/input_slider.cpp", "modules/gui/qt/util/input_slider.hpp",
               "modules/gui/qt/util/previewdiagnostics.hpp",
               "modules/gui/qt/util/thumbnailpreview.cpp", "modules/gui/qt/util/thumbnailpreview.hpp",
               "modules/gui/qt/util/timetooltip.cpp", "modules/gui/qt/util/timetooltip.hpp",
               "prototypes/stage_2_3/thumbnail_helper.c")
    metadata = {"source_commit": SOURCE, "driver_commit": args.driver, "image": args.image,
                "contrib_commit": args.contrib, "contrib_sha256": args.contrib_sha, "sdk_sha256": args.sdk_sha,
                "sdk_url": "https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.7z",
                "run_id": os.environ["GITHUB_RUN_ID"], "run_attempt": os.environ["GITHUB_RUN_ATTEMPT"],
                "source_sha256": {p: digest(source / p) for p in tracked}}
    write_new(args.output, json_bytes(metadata))


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    commands = parser.add_subparsers(dest="command", required=True)
    p = commands.add_parser("package")
    p.add_argument("staging"); p.add_argument("output"); p.add_argument("--metadata", required=True)
    p = commands.add_parser("verify")
    p.add_argument("directory"); p.add_argument("--expected-manifest")
    for name in ("monitor", "snapshot"):
        p = commands.add_parser(name)
        p.add_argument("output"); p.add_argument("--disk", nargs="+", required=True)
        if name == "monitor": p.add_argument("--stop", required=True)
        else:
            p.add_argument("--phase"); p.add_argument("--status", type=int)
    p = commands.add_parser("finalize"); p.add_argument("work")
    p.add_argument("--scope", choices=("host", "container"), required=True)
    p.add_argument("--status", type=int); p.add_argument("--phase", default="before-driver")
    p.add_argument("--monitor-pid", type=int)
    p = commands.add_parser("summarize"); p.add_argument("directory")
    p = commands.add_parser("smoke")
    for name in ("runtime", "fixtures", "output"): p.add_argument("--" + name, required=True)
    p = commands.add_parser("timed"); p.add_argument("output"); p.add_argument("argv", nargs=argparse.REMAINDER)
    p = commands.add_parser("inputs"); p.add_argument("output")
    for name in ("source", "driver", "image", "contrib", "contrib-sha", "sdk-sha"):
        p.add_argument("--" + name, required=True)
    args = parser.parse_args()
    if args.command == "package": package(args.staging, args.output, args.metadata)
    elif args.command == "verify": verify(args.directory, args.expected_manifest)
    elif args.command == "monitor": monitor(args.output, args.stop, args.disk)
    elif args.command == "snapshot": snapshot(args.output, args.disk, args.phase, args.status)
    elif args.command == "summarize": summarize(args.directory)
    elif args.command == "finalize":
        raise SystemExit(finalize(args.work, args.scope, args.status, args.phase, args.monitor_pid))
    elif args.command == "smoke": smoke(args.runtime, args.fixtures, args.output)
    elif args.command == "timed":
        raise SystemExit(timed(args.output, args.argv[1:] if args.argv[:1] == ["--"] else args.argv))
    elif args.command == "inputs": inputs(args)


if __name__ == "__main__":
    main()
