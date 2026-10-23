# Stage 2.4.6A.1 — standalone visual-performance experiment

Extract into a **new folder**. This program does not load or control VLC.
The existing SHA-256-verified Stage 2.4.6A libmpv runtime is unchanged/compatible.
Do not replace earlier EXEs, runtime folders or VLC files.

From this extracted folder in PowerShell (replace media/runtime paths):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Preview.ps1 -Media 'C:\Videos\movie.mkv' -Runtime 'C:\VLC-246A-Runtime' -Results 'C:\VLC-246A1-Visual1'
```

If you do not already have the verified runtime, install official 7-Zip, then:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Get-Runtime.ps1 -Destination 'C:\VLC-246A1-Runtime'
```

Use that new runtime path when launching. Setup downloads only the identified,
checksum-pinned SDK; no global codec installation. The ZIP excludes libmpv pending
its complete corresponding-source/license audit. See THIRD_PARTY.md. A suitable
OpenGL driver and Vulkan loader (normally installed by the NVIDIA driver) are
required. No random DLL mirrors. The assembled private folder runs portably.

The 800-pixel window has a blue hover track/marker, seeking feedback and an orange
marker for the **last presented image request**. The 320×180 image stays visible
while a different target is pending. Its request label does not change until a
candidate is presented. Both labels are requests, not verified frame timestamps;
keyframe output can be seconds early/late. Hover the actual track. The console
also accepts milliseconds/newline, or `quit`. Close the window to flush results.

Modes (`-Mode`):

- `visual` (default): OpenGL rendering, no GPU readback or BMP saving; no per-frame
  stdout/journal writes. Bounded numeric samples are flushed on normal exit.
- `timing`: streams diagnostics and samples memory/CPU/properties, without GPU
  readback/BMPs. Properties never gate subsequent seeks.
- `capture`: additionally reads back GL pixels and saves at most 128 BMPs. This
  deliberately perturbs timing; use only for image-content/accuracy investigation.

`-Hardware no` defaults to CPU decoding; `-Hardware auto-copy` requests GPU decode.
Check `decoder_backend` in events.jsonl rather than assuming the option worked.
`-Seek exact` is an explicit accuracy/latency comparison; default remains
keyframes. `-Renderer sw` is only a software/headless comparison, not MPC-QT's GL.
All modes fully decode source video and render one aspect-preserving 320×180
image; no low-resolution stage, indexing, custom cache or background preprocessing.

## Same-movie, same-workload comparison

Requires Python 3 with `py` on PATH. Close other benchmark windows; keep playback
conditions the same for each run. New output root only:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Compare-Modes.ps1 -Media 'C:\Videos\movie.mkv' -Runtime 'C:\VLC-246A-Runtime' -OutputRoot 'C:\VLC-246A1-Compare1'
```

Optionally add `-LegacyExe 'C:\OldStage246A\mpv-preview.exe'` for the unchanged
Stage 2.4.6A executable, using the identical input sequence. No old files change.
This harness replays 12 widely separated paced targets, three identical requests,
then six 12-request rapid bursts. Missing paced/latest targets are failures,
not silently excluded latency samples. Intermediate rapid targets may coalesce.
Pacing defaults to one second; for overload investigation run `replay.py` directly
with `--interval 0.25`. Repeat runs, including reversed mode order via individual
replay commands, before comparing medians/p95; first hover is only n=1/run.

```powershell
py .\replay.py --exe .\mpv-preview.exe --runtime C:\VLC-246A-Runtime --media C:\Videos\movie.mkv --output C:\VLC-246A1-Replay2 --mode visual --renderer gl --samples 30
```

JSON includes accepted-input→handoff, pending-queue, command reply, restart,
render-call CPU wall time, readback/flip, swap, BMP and stream-log cost. Final
journal samples include superseded work. `restart_ms - queue_ms` estimates the
seek-to-restart pipeline (I/O/demux/decode/synchronization), **not decoder-only
CPU time**. `ui_message_age_ms` estimates queued Windows mouse-event age;
stdin replay does not measure physical mouse/scanout latency. Render-call time
is not GPU execution time. Memory in visual mode is a shutdown snapshot, not a
peak; session CPU includes all threads and idle/teardown time. Timing/capture
per-frame CPU has Windows timer granularity. Counters/history are bounded (last
4096 completed/discarded samples); abrupt termination may lose buffered history.

Compare-Modes optionally collects `nvidia-smi` whole-GPU/decoder utilization at
1 Hz into gpu.csv. This includes **all** GPU workloads, not just this process.
Read gpu-status.json/gpu-stderr.txt; unsupported counters/N/A are unmeasured,
never zero. It stops only the sampler it started. For perceived latency, film the
screen/cursor with an external high-frame-rate camera; handoff is not scanout.

## Timecoded HEVC fixture and accuracy

Use an existing trusted FFmpeg with libx265/drawtext (no codec pack download):

```powershell
py .\generate_hevc_fixture.py C:\VLC-246A1-Fixture --font C:\Windows\Fonts\consola.ttf --duration 180
```

Creates full-resolution 1080p HEVC10 MP4/MKV with visible source PTS and a manifest.
This synthetic SDR fixture is not a substitute for your long-form HEVC movie.
Run capture mode/replay on it and read actual BMP text; normal-mode screen
recording provides the separate visual check without adding readback to the
program. The copied `score_burned_pts.py` can provisionally OCR benchmark.json/BMPs
on a machine with FFmpeg/Tesseract. Human review remains required. `time-pos` and
command completion never prove frame PTS; frame_timestamp_verified stays false.

## Build and tests

From the repository, using a verified SDK in a new output folder:

```sh
CC=x86_64-w64-mingw32-gcc bash prototypes/stage_2_4_6A_1/build.sh VERIFIED_SDK NEW_OUTPUT
cc -std=c11 -Wall -Wextra -Werror prototypes/stage_2_4_6A_1/test_policy.c -lm -o /tmp/visual-policy
/tmp/visual-policy
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s prototypes/stage_2_4_6A_1 -v
```

MSYS2 UCRT64 uses `CC=gcc PYTHON=python`. Native Windows and old GCC 6.4.0 linking
are verified separately in the report. Source and runtime remain GPL-related;
headers' ISC license does not relicense the DLL. See the repository report
`docs/STAGE_2_4_6A_1_VISUAL_PERFORMANCE.md` for actual outcomes and limitations.
No physical Windows/GPU performance acceptance is implied by a successful build.
