# Stage 2.4.6A.1: visual-performance correction

## Status and preservation

Experimental branch: `experiment/stage-2.4.6A.1-visual-performance`, based on
Stage 2.4.6A commit `7c64b3478e888c79313fc5cae8106629e48b8bbb`.
All new prototype code lives in `prototypes/stage_2_4_6A_1/`; Stage 2.4.6A's source,
VLC production code, LibVLC helper and existing builds/ZIPs are untouched.
This is an experimental correction, **not a claim of MPC-QT responsiveness**.
The user's 1080p HEVC10 movie/RTX 3060 is not accessible in this environment.

## Findings: confirmed versus hypotheses

User observations: real-movie handoffs about 250–527 ms, sluggish rapid hovering,
poor timeline feedback, no demonstrated hardware-decoding improvement. Those are
user-reported observations, not a raw dataset from which we can compute p95.
The previous 30-second 640×360 synthetic software benchmarks were not a valid
prediction of that movie's perceived responsiveness.

Confirmed by the old source (`prototypes/stage_2_4_6/preview.c`):

- `render_update()` unconditionally glReadPixels/CPU-flipped every successful GL
  render, including work that would later be discarded. This can synchronize
  CPU/GPU; its actual share of the user's delay was **not measured**.
- `record_frame()` saved BMPs and synchronously flushed per-frame stdout/journal
  on the GUI/render thread. The UI could not process mouse events during that work.
- After handoff, time-pos/hwdec-current replies gated releasing the active request.
  These diagnostics were not necessary for showing the image or starting a seek.
- One seek in flight plus strict stale rejection can leave the previous image
  visible throughout continuous movement if decode/render is slower than input.
  Every pixel/y change could enqueue a fresh request; the strip gave no explanation.
- A fixed 20 ms fallback timer, queueing, driver calls, seek/restart synchronization
  and full-resolution HEVC decoding are other possible contributors. No single
  one is established as the dominant RTX bottleneck from source inspection alone.

The new build measures these stages separately rather than assuming readback or
GPU decoding is the answer. CPU wall time of a GL call is not GPU utilization;
seek-to-restart includes I/O/demux/decode/synchronization, not just decoder cost.

## Implemented correction

A persistent private paused libmpv player remains isolated from VLC. Normal
`visual` mode renders directly to OpenGL and swaps without readback/BMP capture,
per-frame stdout, per-frame file writes or property-reply waits. Small QPC samples
are buffered in a fixed 4096-entry ring and written on clean shutdown. Startup
metadata/error output remains. A watchdog bounds initialization, requests and
teardown; abnormal termination can lose the buffered samples.

`timing` streams measurements and collects extra CPU/memory/property diagnostics;
`capture` adds readback and bounded BMP writing. Property replies are matched to
request IDs and cannot block subsequent seeks. Capture/stream costs are measured
explicitly; diagnostic mode is not sold as uncontaminated visual performance.

One active seek plus one latest pending value bounds decoder requests. Mouse
positions are deduplicated, launches are spaced by at least 16 ms (first launch
immediate), event draining yields after 64 events, and pending values coalesce.
Superseded candidates are recorded but never replace the image. The conservative
one-in-flight/event/frame gate is retained; it may still limit continuous-hover
updates on expensive media. No arbitrary older frame is labeled as the newest
request to create the appearance of speed. Real-movie starvation remains an
acceptance risk if removing diagnostic overhead is insufficient.

The 800-pixel dark seekbar has a blue target/progress marker, timestamp, seeking
feedback and orange last-image-request marker. The 320×180 preview stays visible
while another target is pending; its label changes only on presentation. Frame
PTS remains unverified. Timeline mapping uses actual [16,784] track geometry and
clamps media bounds. Source video is fully decoded, aspect preserved; no custom
cache, indexing, low-resolution stage, playback UI or VLC integration was added.

## MPC-QT comparison and licensing

Reviewed pinned MPC-QT commit `d2d0d15de6e9113df19a4d6e539897de2227a16d`:

- VideoPreview constructs a separate paused MpvObject, audio/audio-display off,
  sub-visibility off, vo=libmpv, hr-seek=no. show() requests absolute/keyframes.
- MpvObject sends commands by queued connection to MpvController on a QThread;
  controller command() uses public mpv commands on that worker.
- MpvGlWidget's render callback queues a widget update; paintGL() renders directly
  to a Qt FBO, and frameSwapped reports the swap. No mandatory glReadPixels/BMP
  capture exists on that ordinary preview path.
- Unlike MPC-QT's direct queued seek/update model, our frame gate serializes
  active seeks to retain conservative stale-result handling. Also WGL/Win32 is
  not Qt's FBO/compositor; mpv/FFmpeg builds, configuration and drivers are not
  established as identical. Matching the media engine alone does not prove parity.

No MPC-QT implementation code was copied; its GPLv2 license was reviewed.
Original/modified experiment source remains GPL-2.0-or-later. Public APIs are the
same verified pinned libmpv client/render/OpenGL API used in Stage 2.4.6A.
The SDK/runtime source/version/checksums are unchanged in dependencies.json;
Get-Runtime and DLL hash verification remain compatible with the existing runtime.
No new codec pack, dependency mirror or LGPL claim. The ZIP still excludes the
DLL/import library pending a complete corresponding-source/license audit of mpv,
FFmpeg and static dependencies. See THIRD_PARTY.md and the Stage 2.4.6A report.

## Validation and performance protocol

Local GCC 6.4.0 Windows x64 strict compile/link, policy/geometry tests and Python
regressions pass. Native Windows build/replay outcomes will be recorded below.
No real-movie, RTX GPU, physical display, listening or foreground-VLC coexistence
results have been performed in this cloud environment.

replay.py uses the same paced targets/bursts for visual, timing, capture and an
optional untouched Stage 2.4.6A executable (--legacy). Normal mode does not need
per-frame synchronous replies to drive the test; its journal is read after exit.
It reports missing paced/latest targets as failures and separates first-hover,
warm, identical-target and rapid-last samples. Median/nearest-rank p95 include
sample counts; first-hover n=1 per run is not a stable distribution. Fixed pacing
may overload expensive media; repeat at smaller intervals to expose that, not to
silently exclude failures. Opposite mode orders/repetitions control warm-up bias.

Metrics: queued mouse-message age estimate, pending queue, command reply,
seek/restart, render completion/call wall time, readback/flip, swap, BMP and streamed
log cost. Session CPU counts all threads; visual memory is a shutdown snapshot.
Optional nvidia-smi sampling is whole-GPU/decoder use (all processes), possibly
unsupported/N/A on WDDM. It is never substituted by render-call wall time.
`time-pos` is diagnostic only; actual accuracy requires burned-in image timestamps
and human review. Physical scanout/subjective responsiveness requires device tests.

A reproducible full-resolution synthetic HEVC10 generator is provided; default
180 seconds is still not a long feature film. CI may use a 12-second sample solely
for functional software smoke. No synthetic result establishes RTX/movie parity.

## Windows test procedure and acceptance

Extract the new package separately. Reuse the existing verified runtime. In its
new directory, replace the paths and use a new results directory:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Preview.ps1 -Media 'C:\Videos\same-movie.mkv' -Runtime 'C:\VLC-246A-Runtime' -Results 'C:\VLC-246A1-Visual1'
```

1. Open exactly the previously tested movie. Record opening/first-hover behavior.
2. Sweep repeatedly between distant positions and through neighboring positions.
   The image should remain visible; the blue target and orange image-request label
   must remain distinguishable during pending work. Note stalls/missing updates.
3. Close normally to flush samples. Repeat with timing/capture in separate folders.
4. Run Compare-Modes.ps1 (README commands), optionally supplying the old EXE.
   Preserve all raw JSON, stderr, capture BMPs and GPU files. Repeat/opposite order.
5. Compare CPU and actual GPU/decoder counters; verify selected backend from logs.
   Test no and auto-copy separately; no hardware acceleration assumption.
6. Use the generated timecoded fixture for image error, then verify physical normal
   mode by screen/cursor recording. Do not infer accuracy from a command reply.
7. Close during seeking, reopen, test invalid/audio-only media, long-GOP/endpoints,
   aspect ratios and ordinary MP4/MKV. Confirm no orphan program/audio and verify
   normal VLC playback concurrently. Preserve old builds as the comparison.

Acceptance is **pending user Windows testing**: smooth rapid hovering on the same
movie, satisfactory 320×180 quality, honest labels, bounded resources/cleanup and
measured first/warm/rapid latency. If pipeline/render/queue times remain large,
report which dominates. No success claim or VLC integration until approval.
