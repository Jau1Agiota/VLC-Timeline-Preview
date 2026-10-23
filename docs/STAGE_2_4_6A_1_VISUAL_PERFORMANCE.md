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
request IDs for bookkeeping and cannot block subsequent seeks. A clock query
can sample a later seek; ID matching does not make that clock the image PTS. Capture/stream costs are measured
explicitly; diagnostic mode is not sold as uncontaminated visual performance.

One active seek plus one latest pending value bounds decoder requests. Mouse
positions are deduplicated, launches are spaced by at least 16 ms (first launch
immediate), event draining yields after 64 events, and pending values coalesce.
Completed in-flight candidates may be presented as explicitly labeled older
intermediates while the latest target stays pending. Presentation IDs must increase;
an old completion cannot overwrite a newer image. Only the newest pending target
is decoded next. This is latest-pending-wins with monotonic presentation, not
strict suppression of every frame behind the cursor. That distinction removes
the deterministic continuous-motion starvation of the old strict gate. No older
image is labeled as the latest target; raw samples record latest_at_handoff and
intermediate_images. Required latest-target replay checks still fail if only an
older intermediate was shown. One-in-flight serialization may still delay the
latest target on expensive media; it is not a claim of MPC-QT parity.

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
  active seeks, with ordered intermediate presentation and separate target/image labels. Also WGL/Win32 is
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
regressions pass. Native Windows build/replay outcomes are recorded below.
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

## Actual final CI results (2026-10-23)

[Run 39974939100](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39974939100)
completed successfully: official-repository fixture preparation, native Windows
x64 strict compilation of both programs, policy/geometry/Python tests, all four
same-file replay cases, ZIP readback/SHA-256 verification, and provisional
burned-image analysis. Source: `f5760207c98691d2631c34f1240ed9d3af7f10b0`.
No VLC build was invoked. Local GCC 6.4.0 compile/link and ten Python tests also
passed, plus ShellCheck 0.10.0, actionlint 1.7.7 (including inline ShellCheck) and
42 existing CI guard tests (one intentional stage-specific skip). The optional
local OCR regression passed with actual FFmpeg/Tesseract; native unit tests may
skip it if those tools are absent, independently of the successful Ubuntu OCR job.

The input was a **12-second synthetic SDR 1920×1080 HEVC Main 10 MKV**, decoded on
CPU and rendered with libmpv's **software** API on GitHub-hosted Windows 2025.
It is not the user's movie or an RTX/OpenGL test. Both sources used the same
compiler, SDK, file and input schedule. Six paced targets, three identical-target
requests and two 12-request bursts gave 33 inputs/case. Eleven paced/final targets
were required; all were presented while still current. There were no replay or
process-exit failures. These are smoke-test sample sizes, not a device benchmark.

| Case | First hover (n=1) | Warm handoff median / p95 (n=5) | Rapid final handoff median / p95 (n=2) |
|---|---:|---:|---:|
| A.1 visual | 25.384 ms | 20.099 / 26.391 ms | 42.008 / 53.468 ms |
| A.1 timing | 27.889 ms | 20.262 / 27.086 ms | 32.308 / 35.928 ms |
| A.1 capture | 29.848 ms | 21.221 / 27.128 ms | 39.378 / 51.251 ms |
| Unchanged A, capture | 26.191 ms | 20.417 / 25.953 ms | 40.449 / 53.959 ms |

**No meaningful warm-seek speedup is demonstrated here.** Rapid differences at
n=2 are not reliable evidence of superiority. These handoffs are GDI calls, not
physical scanout/perceived responsiveness. Order/cache/scheduling effects remain.
Opening/loader timings are retained separately in each raw benchmark.json.

A.1 visual showed 18 images, including seven explicitly older intermediate
presentations; timing showed 17/six; capture showed 18/seven. The old strict gate
showed 11/no intermediates. Only the newest pending seek was retained. Newer
completed images were protected by the tested monotonically increasing ID guard.
This demonstrates the functional starvation correction under the replay; it does
not establish subjective smoothness on the user's movie. Every case still passed
the required latest-target checks, so old intermediates were not counted as
successful final-target responses.

For visual warm samples, the seek-to-restart pipeline median/p95 was
19.894/26.213 ms; total render-call wall time 1.223/1.820 ms; readback, BMP and
stream-log costs were zero. Capture BMP cost was 1.527/1.761 ms and stream output
0.098/0.103 ms. These overlapping counters must not be summed. The pipeline
rather than GUI rendering dominates this **software synthetic** case; it does
not establish decoder-only CPU time or the RTX bottleneck. No GL readback was
exercised on this runner. Real-movie GPU/core/queue bottlenecks remain unconfirmed.

Process CPU, session GUI pump/paint totals/maxima, resource snapshots, metadata,
all request records and diagnostic images are preserved in the raw evidence.
GPU utilization is **unmeasured here**, not zero. The normal-mode memory field
is a shutdown snapshot after decoder teardown, not active/peak usage. Use the
external sampler/Windows process monitoring for actual-device resource analysis.
GitHub truncates large Checks notices at 4096 characters, so some session fields
are absent there: full artifact JSON is authoritative. Cloud artifact retrieval
returned HTTP 403; no missing resource values were invented from that limitation.

Provisional OCR: new capture 18/18 source-plausible images, old capture 11/11.
Both had **1,400 ms median / 7,800 ms p95 absolute error** against requested time.
The long GOP intentionally exposes coarse keyframe selection; normal/timing
produce no BMPs and therefore no direct image-accuracy score. Human review and
normal-mode physical recordings remain pending. No accuracy gate is declared
passed, and neither a command reply nor a sampled time-pos is treated as PTS.

Full raw results/images: [artifact 12511251280](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39974939100/artifacts/12511251280).
Provisional OCR: [artifact 12510842022](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39974939100/artifacts/12510842022).
Fixture/manifest: artifact `12510078643` on that run. CI's DejaVu test-font
notices are in `prototypes/stage_2_4_6A_1/FONT_NOTICES.txt`; future fixture exports
include that notice and omit the copied font file. Existing artifacts remain
preserved. The application ZIP never includes font software. This notice/export-filter-only
follow-up was statically checked; the executed application/replay/accuracy source
remains the pinned commit identified above. Extracted complete Checks
subobjects, with truncation explicitly marked, are committed in
`prototypes/stage_2_4_6A_1/evidence/ci-39974939100.json`.

## Download and next decision

[Download the Windows x64 package](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39974939100/artifacts/12510462696)
(GitHub login; 14-day retention). Extract the Actions download and then its inner
`libmpv-stage246A1-win64-f5760207c986.zip` into a new folder.
Inner ZIP SHA-256:
`4656cbe15fddfb44751d292c692831eeede4d7a31fc7ab838509bc556ccef518`.
The manifest identifies built source `f5760207c98691d2631c34f1240ed9d3af7f10b0`;
later documentation/evidence and fixture-export notice updates do not change that executable. Reuse the
existing verified runtime and launch visual mode using the command above.
The EXE, own source/build/test procedure, runtime lock, setup and comparison
scripts are provided; the third-party DLL remains a separate verified setup.

Recommendation: test the same HEVC movie in normal visual mode, then collect
normal/timing/capture and optional old-binary comparisons. If latency remains
250–527 ms, inspect pending queue versus seek/restart versus cumulative GL work,
readback, swap and GUI work before changing decoding or declaring success.
**No claim of MPC-QT parity, real-movie speed improvement, RTX validation or VLC
integration is made.** Stop for the user's physical Windows results and approval.
