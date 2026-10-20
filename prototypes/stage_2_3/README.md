# Stage 2.3 standalone extraction prototype

Research code only. No timeline/tooltip integration or VLC production files change. See [the results report](../../docs/STAGE_2_3_EXTRACTION_RESULTS.md) before interpreting an exit code of zero.

`thumbnail_helper.c` creates a private LibVLC 3.0.24 instance/player, requests `:start-time` and copies the **first displayed candidate** through memory-video callbacks. It saves an uncompressed, bounded 32-bit BMP. There is no fixed sleep and no claim that the candidate belongs to the requested timestamp. The source-visible timecode experiment is the only accuracy evidence. This intentionally simple candidate rule is what Stage 2.3 tests, not a proven association algorithm.

## Incremental build in the existing WSL2 environment

Use your already validated baseline checkout/build and official VideoLAN Windows x64 container. Preserve local changes and existing binaries; no full VLC rebuild is necessary. The exact new prototype branch is `prototype/stage-2.3-libvlc-extraction`. From a clean source checkout, fetch/switch to it; tracked VLC source remains identical to the baseline.

```sh
git status --short
git fetch origin prototype/stage-2.3-libvlc-extraction
git switch prototype/stage-2.3-libvlc-extraction
```

In WSL, set these two paths to the **existing** build on your machine; `win64` is the standard release wrapper's build directory:

```sh
VLC_SRC="$HOME/src/VLC-Timeline-Preview"
VLC_BUILD="$VLC_SRC/win64"
PORTABLE="$VLC_BUILD/vlc-3.0.24"
# Confirm the actual portable path; use your validated package directory if different.
test -f "$PORTABLE/vlc.exe"
test -f "$PORTABLE/libvlc.dll"
test -d "$PORTABLE/plugins"
test -f "$VLC_BUILD/lib/.libs/libvlc.dll.a"
# All output paths below must be new. Do not replace the validated package.
mkdir "$HOME/vlc-stage23"
docker run --rm -v "$VLC_SRC:/vlc:ro" -v "$HOME/vlc-stage23:/stage23" -w /vlc \
  registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7 \
  bash -lc 'bash prototypes/stage_2_3/build.sh /vlc/win64 /stage23/build'
cp -a "$PORTABLE" "$HOME/vlc-stage23/portable"
cp "$HOME/vlc-stage23/build/thumbnail-helper.exe" "$HOME/vlc-stage23/portable/"
```

If your build is not `$VLC_SRC/win64`, mount its existing directory read-only at `/baseline-build` and pass `/baseline-build` to `build.sh`. The script compiles just one source file against the repository headers and your baseline's `libvlc.dll.a`; it runs no configure, contrib, make or VLC application target. Build outputs include executable hash and PE import inspection. Compiler mismatch or a missing import library is an explicit failure, not a reason to rebuild everything. Inspect dependencies and copy only required matching runtime DLLs from the validated environment if necessary; do not download a codec/DLL pack.

Copy the isolated `portable` directory to Windows, e.g. `C:\VLC-Stage23\portable`. Include its **matching** libvlc/libvlccore DLLs, plugin tree and existing runtime dependencies. Never put the helper beside the foreground application's installed binaries. The helper rejects a runtime whose version is not 3.0.24. It clears its own `VLC_PLUGIN_PATH` before initialization; this is not a sandbox and does not eliminate Windows DLL-search risks. Use a trusted copied package and working directory.

## Generate visible-PTS fixtures

Use an already installed FFmpeg with libx264, AAC and drawtext, Python 3 and a readable monospace TTF font. Nothing in these scripts downloads media, codecs or tools. In WSL (adjust font path if necessary):

```sh
python3 "$VLC_SRC/prototypes/stage_2_3/generate_fixtures.py" \
  "$HOME/vlc-stage23/fixtures" \
  --font /usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s "$VLC_SRC/prototypes/stage_2_3" -p 'test_*.py' -v
```

Default fixtures are 30 seconds, 640×360, with large source **PTS** text burned into every selected frame and a 660 Hz audio tone. Variants: ordinary H.264 without B-frames (GOP 30), B-frames (GOP 60), long GOP (300 frames), and VFR selecting original 30 fps timestamps at uneven intervals. MP4 is encoded once then remuxed to MKV. The generator verifies B-frame presence and variable presentation gaps and records hashes/FFmpeg version/frame timestamps in `manifest.json` and `*.frames.json`. Remuxing can introduce an audio-related container offset; the **burned text**, not container clock metadata, is the frame truth. Audio-only MP4 and deliberately corrupt MP4 are included as failure fixtures.

Output directories must be new; a failed run may leave partial fixtures. Preserve its log and choose a new output directory after diagnosis. Do not use a partial run without its completed manifest. Shorter `--duration` is available for generator smoke tests; the supplied matrix intentionally requires default 30-second fixtures.

Copy fixtures and `run_matrix.ps1` to Windows. Do not run the Windows tests on WSL `wine` and describe them as native Windows behavior.

## Windows candidate collection and measurement

First, in a trusted PowerShell working directory with the copied portable package:

```powershell
& 'C:\VLC-Stage23\portable\thumbnail-helper.exe' `
  'C:\VLC-Stage23\fixtures\ordinary.mp4' 14750 `
  'C:\VLC-Stage23\single-14750.bmp' 10000
$LASTEXITCODE
```

Arguments: local fixed-drive MP4/MKV path, integer target milliseconds (0–86,400,000), **new** BMP output path, optional 100–120,000 ms engine/capture/cleanup deadline (default 10,000). UNC, network/removable drives, other extensions, invalid arguments and unsupported version are rejected. Output is never overwritten. Parent directory must already exist. Path validation occurs before the internal watchdog; the external runner bounds the entire process, including file checks and DLL load.

Exit 0 means **candidate captured and saved**, never “accuracy passed.” Successful stdout is a JSON record with requested time, callback capture latency, total operation latency, dimensions and `frame_timestamp_verified:false`. DLL startup, path validation and scheduling outside the engine timer are included only in the runner's wall latency. Exit 2: usage/input/runtime-version rejection; 3: engine/decoder/no-video failure; 4: output failure; 124: internal watchdog timeout. Loader failures or external termination can have other exit codes. Timeouts may produce no JSON and may leave a partial BMP: ignore every image whose process did not exit successfully.

Collect three repetitions at 0, 80, 1000, 14750, 28800, 29500 and 29950 ms for all eight video fixtures (168 processes). This includes near-start, middle, end and long-GOP positions:

```powershell
& 'C:\VLC-Stage23\run_matrix.ps1' `
  -Helper 'C:\VLC-Stage23\portable\thumbnail-helper.exe' `
  -Fixtures 'C:\VLC-Stage23\fixtures' `
  -Results 'C:\VLC-Stage23\results-run1'
```

The runner checks fixture hashes, drains stdout/stderr concurrently, records per-process wall/capture latency and exit status, and kills/reaps a process that exceeds 15 seconds. No extraction queue is created. Its `CreateNoWindow` suppresses the helper's console window, **not a video playback window**. Window/audio behavior still requires observation. PowerShell script execution must follow your normal local policy; no policy bypass is proposed.

For each successful BMP, read its visible `PTS HH:MM:SS.mmm` text. Enter `observed_ms` in `measurements.csv` and set `observation_method` to `visible_pts`. Example conversion only: `00:00:14.767` means 14767 ms. Do **not** populate observations with `get_time`, TimeChanged, ffprobe's nearest frame, or requested time. Inspect the actual helper-produced BMP; the fixture source alone does not prove its contents. Leave unreadable/uncertain images unreviewed and explain in notes. Retain original BMP/JSON/stderr and an unedited copy of the raw measurements.

```powershell
python 'C:\path\to\prototypes\stage_2_3\score_results.py' `
  'C:\VLC-Stage23\results-run1\measurements.csv'
```

The scorer enforces ≤250 ms absolute error, reports every failure/unreviewed row, rejects nonfinite input, and returns success only if all rows are visibly measured and within budget. It trusts the human observations; it is not OCR and does not independently prove them. `accuracy_gate_passed` addresses timestamp evidence only, **not** audio/window/cleanup acceptance. Report latency distribution and each fixture's error distribution from real results; do not infer them from the fixture-generation run.

## Behavior, error and cleanup tests (native Windows; unrun here)

- Listen with speakers/headphones while the helper processes the audible fixtures: no private tone should be heard. Check desktop, taskbar and foreground focus: no helper playback window or error dialog should appear. Fill `audible_output` and `visible_window` with `yes`/`no` after observation; never mark them automatically from flags or exit codes.
- Play separate foreground VLC video/audio while running the matrix. Listen for continuity, check foreground position progresses naturally, and inspect for resource contention. The helper has no foreground handles/IPC/control path, but shared CPU/disk can still affect playback.
- Test `audio_only.mp4` (expected no image/nonzero exit), `corrupt.mp4`, nonexistent input, non-seekable/unsupported inputs, unwritable output, existing output, target past EOF and invalid/negative timestamp. Preserve JSON/stderr/exit codes. No-video may be reported as end-without-video or hit a bounded timeout; do not assert exact code before measuring the packaged runtime.
- Run the helper with a 100 ms deadline on a fixture. A fast machine might finish within it; if not, expect 124. Confirm it exits and no VLC helper child remains. Run repeated success/failure cycles and external cancellation; inspect process/handle/memory behavior. The watchdog terminates **only this helper**, not foreground VLC. It bounds blocking stop at the cost of abrupt private-process teardown.
- Test UTF-8/Unicode, spaces, `%` and `#` in paths, renamed source, long paths and another cwd. Existing output must remain untouched. Long-path behavior depends on Windows configuration and the copied runtime; no guarantee is inferred from wide-character file APIs.

Callbacks allocate three aligned, bounded RV32 slots per negotiated output format, copy the first display candidate to an owned image, signal completion and return. They never stop/release VLC or write disk/IPC. Stop/release and format cleanup happen on the main control path; the watchdog covers hangs. Maximum negotiated frame is 320×180, but this does not cap full-resolution decoder memory. Cropping, SAR, HDR/color fidelity, dynamic format changes and audio-only EOF/stop races need actual device coverage. No cache, UI or persistent helper protocol is implemented in this stage.

## Decision after measurements

Do not proceed to timeline integration based on this source alone. First compile/run in the validated environment and attach complete actual measurements. If the first-display startup rule yields wrong-start/stale frames or errors over 250 ms, classify those as inaccurate output and **revise the prototype or investigate FFmpeg timestamp-aware decode-forward selection**. Never replace visible-image evidence with player-clock heuristics. Only proceed with LibVLC after accuracy, no-window/no-audio, bounded lifetime and practical latency are demonstrated.
