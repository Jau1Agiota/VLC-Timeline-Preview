# Stage 2.4.6A: isolated libmpv keyframe preview experiment

## Status

Experimental branch starts at the preserved Stage 2.4.5 feature head
`342e4028a87deddb99f71bf03bc7a50524be0bed`. Nothing in VLC production code,
the Stage 2.4.5 LibVLC engine, previous ZIPs or untracked build directories is
modified. The standalone Win32 program compiles and links with the existing
MinGW-w64 GCC 6.4.0 compiler. Native Windows software-renderer measurements and packaging passed; target-PC
OpenGL/GPU and human visual acceptance remain pending.

Do not integrate yet. The user's MPC-QT observations are the motivation, not
measurements of this independent experiment.

## Verified reference and architecture

MPC-QT commit `d2d0d15de6e9113df19a4d6e539897de2227a16d` was inspected:

- `src/widgets/videopreview.cpp`: separate persistent `MpvObject`, paused,
  `vo=libmpv`, audio off, subtitles hidden, `hr-seek=no`; `show()` requests a seek.
- `src/mpvwidget.cpp`, `MpvObject::seek`: constructs absolute/keyframes flags.
- Its OpenGL widget renders video; this program independently uses the public
  libmpv OpenGL render API, without copying MPC-QT implementation code.

The standalone program has one media/player for its entire lifetime. All
source frames are decoded normally; only rendering is scaled to a 320×180
aspect-preserving canvas. There is no cache, thumbnail index, prefetch,
low-resolution decode stage, Qt dependency, VLC player handle, audio or subtitle
output. Manual hover over the strip requests a timestamp; stdin accepts arbitrary
millisecond requests. Requests span the file without recreating the player.

One request is active and one latest value replaces any pending request. The
active seek is allowed to finish, but is rendered into a private buffer/back
buffer and cannot be presented if its ID is obsolete. The latest request then
starts. Every request is bounded by a 5-second deadline; hangs terminate this
standalone process, never VLC. Initialization is bounded by 15 seconds, teardown
by 5 seconds. Up to 10,000 input requests and 128 saved BMPs bound diagnostics.
A watchdog is independent of the event/render pump. Native callback handlers
only signal the window; they never call rendering recursively.

Client calls during rendering use the public asynchronous command/property API
and nonblocking `mpv_wait_event(0)`. No synchronous property lookup runs while
video rendering is active. The WGL context remains current on its owning GUI
thread through render-context destruction. The public SDK's threading rules,
render/frame-info structs, asynchronous command IDs and lifecycle were inspected.
The software renderer is an explicit alternative for headless CI; upstream warns
it is slow, CPU based and may mishandle HDR. It must not be labelled MPC-QT's
OpenGL path or used to assert GPU performance.

## Frame evidence and measurements

The program records separately: process/open initialization; file load to first
render; initial presentation handoff; command reply; seek/restart events; new
non-redraw/non-repeat render frame; presentation handoff; per-request process
CPU time; working set/private memory; runtime decoding backend. A successful
command reply alone is never a candidate. Presentation requires command reply,
seek, playback restart and a new render frame, plus the latest ID at handoff.

OpenGL handoff ends at SwapBuffers; software handoff ends at
StretchDIBits/GdiFlush. Neither proves physical monitor scanout. The public
render frame-info provides scheduling metadata, **not media PTS**. The sampled
`time-pos` property after handoff is useful diagnostic evidence while paused,
but is explicitly not independent proof of the saved image timestamp.
`frame_timestamp_verified` and `physical_display_verified` remain false.

Use burned-in PTS fixtures and read the BMP itself to measure actual image error.
Keep `requested_ms`, `playback_position_ms` and independently observed burned PTS
as separate columns. Keyframe seeks can be seconds early on long GOPs, or choose
nearby keyframes; fast output must not be described as exact. An `exact` mode is
provided solely as an explicit accuracy/latency comparison.

Benchmark sampling uses 30 requests cycling between 3% and 95% of the file,
then six bursts of 12 requests at 5 ms intervals. Only the last target in each
burst is required to produce a visible result. Raw JSONL and BMPs are retained.
Median and nearest-rank p95, sample counts, CPU identity and actual renderer are
reported; measured native Windows results follow below. Compare Stage 2.4.5 only
on identical files/hashes/timestamps and disclose its accurate-seek difference.
A real comparison on the user's movie is pending access to that local file.

## Dependency provenance and licensing

Selected provider: `shinchiro/mpv-winbuild-cmake`, release `20261021`, generic x64
asset `mpv-dev-x86_64-20261021-git-e471496c56.7z` (31,622,190 bytes).
Source commit: `e471496c56f0f49c133bd88b50aed62b845f1909`.
Archive SHA-256: `efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633`.
Extracted DLL SHA-256: `04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`.
Both downloaded bytes and DLL were verified locally against the release API
and extracted bytes. Lock file contains the exact HTTPS URL. Runtime version,
FFmpeg version and build configuration are emitted by the executable; a git
nightly must not be mislabeled as the latest stable v0.41.0 release.

`objdump` confirms `libmpv-2.dll` imports Windows system/UCRT DLLs, OpenGL and
`vulkan-1.dll`. Vulkan loader availability must be checked; do not fetch random
DLLs. A current RTX driver normally provides it. CPU decoding is the default
(`hwdec=no`) with OpenGL rendering; `auto-copy` requests hardware decode, whose
actual selection is recorded. NVIDIA decoding and target-PC OpenGL remain
unverified, not inferred from an option string.

MPC-QT's GPL reference code was not copied. Our original source is GPL-2.0+.
Public libmpv headers have ISC notices. mpv defaults to GPLv2+, with LGPL mode
possible only for a suitable build; FFmpeg/dependency licensing also depends on
build configuration. The SDK archive has no complete corresponding-source or
license bundle, so the downloadable experiment ZIP intentionally excludes its
DLL/import library. A private-use setup script fetches and verifies it directly
from the identified provider. This is a dependency setup step, not an all-in-one
runtime redistribution. The assembled folder can run portably after setup.
Before distributing any combined package, audit exact source/patch/configuration
and notices for mpv, FFmpeg and every linked dependency. No unsupported LGPL
claim or invented written source offer is made. See `THIRD_PARTY.md`.

## Build, delivery and user test

Reproducible commands and exact PowerShell launch/benchmark examples are in
`prototypes/stage_2_4_6/README.md`. Build only the standalone C file against the
verified SDK; never invoke a VLC target. Existing GCC 6.4.0 compile/link and native
host request-policy tests passed. The independent manual Actions workflow uses
MSYS2 UCRT64 GCC on Windows 2025, records compiler identity, tests the policy and
benchmarks software rendering on real Windows. It downloads unchanged Stage 2.3
fixtures generated in a separate Ubuntu job. It does not rebuild VLC, clean
existing disks, need Docker/privileged mode, publish Releases or use secrets.
The existing pilot workflow gains only an explicitly selected 2.4.6A dispatch
route, leaving previous stage paths and their default unchanged. This allows a
new reusable workflow on an unmerged experimental branch to be dispatched.

Native CI OpenGL cannot be assumed on a non-GPU runner. If the DLL cannot load,
record its exact dependencies/Windows error and stop rather than substituting
unidentified libraries. Delivery should include only our executable/source,
lock file, scripts, manifest and checksum, with independent artifact readback.
The user then obtains the verified private runtime before running the program.

On the user's PC:

1. Extract into a new folder and run the pinned private-runtime setup.
2. Launch default OpenGL/CPU decode. Record opening, first hover and warm samples.
3. Hover rapidly between distant timeline positions; verify old requests never
   replace the latest one, and normal VLC playback/audio remain unaffected.
4. Run `--renderer gl` benchmark on ordinary MP4/MKV and the real movie. Keep
   raw JSON and read burned-in PTS from matching BMP filenames.
5. Compare explicit `--hardware auto-copy` and `--seek exact` in separate new
   result folders, inspecting actual decoder selection, quality and seek error.
6. Test close during a seek, invalid/corrupt/audio-only files, end-of-media,
   aspect ratios and a long-GOP clip. Report actual failures/timeouts.
7. Keep Stage 2.4.5 untouched as rollback. Do not replace its helper or DLLs.

Recommendation at this revision: investigate further only; do not integrate
libmpv until native OpenGL responsiveness, acceptable keyframe error, lifecycle
behavior and a redistributable dependency/source package are demonstrated.

## Actual Windows pilot results (2026-10-23)

Run [39953050730](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39953050730),
source `c5cbbb9d869102e46ab87fe53dd9d8a6838af390`: Windows build, native request-policy
and Python tests, four libmpv benchmark cases, four unmodified Stage 2.4.5 helper
comparisons, packaging and independent artifact checksum/ZIP readback passed.
The subsequent Ubuntu OCR job produced all four mpv summaries but failed during
comparison-image processing; this run as a whole is **failed**, not an all-green
accuracy certification. A reporting correction preserves partial evidence and
makes individual analysis errors explicit. Earlier pilot artifacts are retained.

Environment: Windows Server 2025 (10.0.26100), four logical AMD64 CPUs (Family 25,
Model 1, Stepping 1). Actual runtime: `mpv v0.41.0-1227-ge471496c5`, FFmpeg
`N-127893-g9a861e968`; software rendering, actual `hwdec-current=no`. Inputs were
synthetic 30-second, 640×360 timecoded fixtures, not the user's movie. No GPU,
Windows foreground-VLC coexistence, audio listening or physical-display test is
claimed. Measurements describe GDI presentation handoff, not monitor scanout.

| Case | Warm handoff median / p95 | Rapid latest handoff median / p95 | First-hover handoff |
|---|---:|---:|---:|
| Ordinary MP4, keyframes | 3.880 / 5.377 ms | 4.124 / 5.373 ms | 6.521 ms |
| Long-GOP MKV, keyframes | 5.032 / 5.950 ms | 6.578 / 9.865 ms | 10.635 ms |
| VFR MP4, keyframes | 5.387 / 8.082 ms | 7.007 / 10.193 ms | 10.880 ms |
| Long-GOP MKV, exact | 49.166 / 80.767 ms | 56.809 / 132.902 ms | 18.189 ms |

Warm n=29/case, rapid n=6 latest results/case, first-hover **n=1**, hence not a
stable first-hover percentile. Each case sent 30 sequential and 72 burst inputs;
obsolete work is coalesced rather than requiring 102 presentations. Saved-image
counts vary with scheduling. Accepted input to command reply is recorded
separately and is not used as displayed-frame latency. Process-start-to-ready
was 429.054 ms for the first case and 79.438–82.912 ms subsequently; initialization
inside the executable to first render was 55.666–58.664 ms. Do not hide DLL/OS
loader overhead by reporting only in-program initialization.

Maximum **sampled** private memory was 34.3–36.7 MiB and working set 41.5–43.7 MiB.
Aggregate process CPU / wall ratios were 1.28, 2.01, 2.24 and 2.58 equivalent cores
respectively, including loader/startup time; these short synthetic sessions are
not steady-state movie CPU estimates. Per-request CPU samples have Windows
15.625 ms granularity and zeros do not mean no CPU use.

Paired unmodified Stage 2.4.5 helper warm **parent response** median/p95 was
64.539/88.933 ms for ordinary MP4 and 128.912/208.563 ms for one long-GOP MKV
comparison. mpv parent response (including BMP/logging/property work) was
5.783/7.496 ms ordinary/keyframes and 51.110/82.767 ms long-GOP/exact. These are
same-machine/file/target measurements; keyframe vs accurate seeking is a material
accuracy difference. LibVLC's first request initializes its player whereas mpv is
preloaded, so first-request values are not a fair like-for-like initialization
comparison. All four paired cases completed; complete rows are in the evidence
artifact. Checks expose only a subset of comparison notices.

Provisional OCR of actual mpv BMP content (source-frame plausibility checked):

| Case | Plausible images | Absolute image error median / p95 |
|---|---:|---:|
| Ordinary/keyframes | 95 | 500 / 1,000 ms |
| Long-GOP/keyframes | 75 | 5,517.85 / 9,513.65 ms |
| VFR/keyframes | 48 | 2,800 / 6,200 ms |
| Long-GOP/exact | 36 | 14.10 / 24.60 ms |

This is automated burned-text OCR, **human spot review pending**, and includes
intermediate images actually handed off during bursts. It is independent of
`time-pos`, but is not a completed accuracy gate. Fast keyframe output can be
nearly ten seconds away from the request on this long-GOP fixture. Exact seeking is slower but
promising; acceptable movie behavior must be decided on the user's PC. No frame
PTS is fabricated from a command acknowledgement or player clock.

Raw benchmark summaries: `prototypes/stage_2_4_6/evidence/windows-run-39953050730.json`.
Complete raw JSONL/BMP evidence: artifact `12501611143` on that run. Source/sample
sets are identified deliberately; results are not combined across runs.
Run [39957341975](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39957341975)
independently passed all four Windows benchmarks and paired comparisons after
the presentation-failure cleanup correction. Its four mpv OCR summaries repeated
the same median/p95 errors; raw evidence artifact is `12503750668`.

## Latest delivery and verification

[Download Windows x64 artifact 12503437384](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39958215438/artifacts/12503437384).
Source: `b2d83ffaf56e953bcd9d981f21d60b6a8b58d8c9`.
Inner ZIP: `libmpv-stage246A-win64-b2d83ffaf56e.zip`.
SHA-256: `f45149fff53fd7bce62aee46a3af416f75c7640dfcad85d4824dd98f1dcd8be0`.
Includes our EXE/source, setup scripts, manifest and checksum, **not libmpv**.
Artifacts expire after 14 days; no GitHub Release was created. All earlier builds
and artifacts remain preserved. Later documentation commits do not change this
executable; its C source is identical to the fully benchmarked `5c30cce3b7d7`.

The Windows job on [39958215438](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39958215438)
passed strict compile/link, native request-policy/Python suite, packaging and
independent checksum/ZIP readback. **The overall workflow failed**: Ubuntu fixture
preparation's APT fallback exited 124; benchmarks/accuracy were explicitly skipped
on this final packaging run. These skipped tests are not reported as passed.
The earlier successful benchmark runs provide the measured results above.

Local verification passed: GCC 6.4.0 compilation/final linking, request-policy
executable, six Python tests (including real FFmpeg/Tesseract narrow-image
processing), Bash syntax, ShellCheck 0.10.0, actionlint 1.7.7 and diff checks.
The existing CI safety suite ran 42 tests (41 passed; one intentional stage
skip). The optional offline OCR regression skips on machines lacking its tools.
A baseline diff confirmed no changes to `modules/` or the Stage 2.3 helper.

Fixture preparation reuses trusted runner-provided FFmpeg/fonts when present
and records their identity. APT fallback has one retry and explicit 120/180-second
update/install limits. Windows compilation/package delivery runs independently
of fixture success, while benchmark/accuracy prerequisites remain mandatory.
Earlier Ubuntu jobs also hit their 15-minute limits; cloud job-log retrieval
returned HTTP 403, so the exact stalled package/URL is unverified. No dependency
mirror or certificate-validation workaround was introduced.

A separate analysis defect was reproduced and corrected: run `39957341975`'s
LibVLC comparison-BMP conversion failed with FFmpeg exit 234 before OCR. The
scorer demanded a 320-pixel crop although the unchanged helper permits narrower
images. A real 313×180 BMP reproduced the invalid-crop error; using actual image
width fixes the regression. Partial error/image results remain in accuracy
artifact `12504240084`. Each offline FFmpeg/Tesseract subprocess now has a
30-second limit. Full comparison-image OCR with this correction remains pending;
no decoder or preview-executable change was required.

## Windows PC acceptance

Extract the Actions download, verify/extract its inner ZIP, then change into
`libmpv-stage246A-win64`. Requires official 7-Zip. These commands affect execution
policy only in each child PowerShell process, for the supplied unsigned scripts:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Get-Runtime.ps1 -Destination "$env:USERPROFILE\VLC-246A-Runtime"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Preview.ps1 -Runtime "$env:USERPROFILE\VLC-246A-Runtime" -Media 'C:\Videos\movie.mkv' -Results "$env:USERPROFILE\VLC-246A-Test1"
```

Use new destination/results folders. This opens only the experimental window,
with default OpenGL rendering and CPU decoding. Hover its strip, enter
milliseconds in the console, or enter `quit`. Benchmark commands and the manual
checklist are above and in the package README. Do not replace VLC files or the
Stage 2.4.5 runtime.

For independent image re-analysis without rebuilding either player, check out
this branch and use a machine with GitHub CLI, FFmpeg and Tesseract:

```sh
gh run download 39957341975 -R Jau1Agiota/VLC-Timeline-Preview -n libmpv-stage246A-evidence-39957341975-1 -D evidence-246A
gh run download 39957341975 -R Jau1Agiota/VLC-Timeline-Preview -n stage246-fixtures-39957341975-1 -D fixtures-246A
python3 prototypes/stage_2_4_6/score_burned_pts.py evidence-246A fixtures-246A accuracy-246A-new
```

The output directory must be new. Read actual BMP text for human review; OCR
plausibility never sets `frame_timestamp_verified=true`. MKV remuxing can offset
container timestamps relative to burned original PTS; raw absolute errors include
that offset, with no hidden normalization.

Prototype bounds: fixed-drive local MP4/MKV only; console requests at most 24
hours, 10,000 accepted requests and 128 saved images/process; 15-second
initialization and 5-second request/teardown deadlines. Real movies, identical
consecutive targets, unusual aspect ratios/HDR, hardware decode, corrupt/audio-only
inputs, GUI close/driver errors and foreground-VLC coexistence require manual
acceptance tests. A watchdog exit is a failure, never an accurate frame.

Recommendation: **user-PC OpenGL evaluation, not VLC integration yet**. The
persistent engine is promising, but keyframe image error exceeds the previous
250 ms target on these clips. Exact seeking is a useful measured comparison.
Target-PC responsiveness/quality, human accuracy review and combined-runtime
licensing remain gates. Stop before Stage 2.4.6B until user acceptance.
