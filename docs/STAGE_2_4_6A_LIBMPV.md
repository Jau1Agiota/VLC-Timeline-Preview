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
Complete raw JSONL/BMP evidence: Actions artifact `12501611143` on that run.
Package artifact [12500414250](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39953050730/artifacts/12500414250)
contains `libmpv-stage246A-win64-c5cbbb9d8691.zip` and `SHA256SUMS`.
Inner ZIP SHA-256: `ac30aef36564b2003b53d419379d49883641820ec09940603469cd29e410071f`.
Artifacts are temporary (14 days), not permanent Release links. The ZIP contains
our executable/source only; run `Get-Runtime.ps1` to assemble the private runtime.

Recommendation: **user-PC OpenGL evaluation, not VLC integration yet**. The
persistent engine shows promising responsiveness; keyframe accuracy and combined
runtime redistribution remain material gates. Preserve Stage 2.4.5 as-is. Stop
before Stage 2.4.6B until Windows movie testing and user acceptance.

Local regression verification: strict GCC 6.4.0 compilation and final linking,
request-policy executable, five Python tests, Bash syntax, ShellCheck 0.10.0 and
actionlint 1.7.7 passed. The existing CI safety suite ran 42 tests successfully
(41 passed; one intentional stage-specific skip). A baseline diff confirmed no
changes to `modules/` or `prototypes/stage_2_3/thumbnail_helper.c`.

Latest executable/source delivery (reporting-only correction, preview engine
unchanged): source `0fb671f3f161a1ce3db280097eb714ccefb434ea`,
[package artifact 12501088618](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39954190209/artifacts/12501088618).
Inner ZIP: `libmpv-stage246A-win64-0fb671f3f161.zip`.
SHA-256: `a24b501d85b362dd462c06f5a5d41ff45ed40a738e415a40ef8bfac66dd41754`.
Its native Windows build, all four benchmarks/comparisons and independent readback
also passed. Raw evidence artifact: `12500719775`. Benchmark table above deliberately
identifies the earlier measured run; it does not combine samples across runs.


Follow-up run `39954190209` again passed its Windows benchmark/package job;
the Ubuntu analysis job exceeded its 15-minute limit without an accuracy
artifact. Cloud retrieval of job logs was blocked by HTTP 403, so the precise
internal command is not established. The workflow now reuses trusted
runner-provided FFmpeg/fonts when present, records their identity and bounds
APT update/install fallback to 120/180 seconds with one retry. Tesseract fallback
has the same bounds. These are dependency preparation changes, not decoder or
measurement changes. No all-green workflow or completed accuracy gate is claimed.


Run `39956694820` confirmed APT fallback exit 124 in fixture preparation (step
12:53:06–12:56:13 UTC); Windows/analysis were skipped because the prerequisite
failed. Exact download/service cause remains unavailable through cloud log
retrieval. It is not a decoder/compiler failure. Windows compilation/package
eligibility is now independent of fixture success: failed fixture preparation
explicitly skips download/benchmarks/accuracy, while strict compilation, unit
tests and package/readback checks still run. The overall workflow retains the
fixture failure; no skipped benchmark is reported as passed. This permits
current standalone binary delivery without weakening measurement prerequisites.

## Final engine delivery

Run [39957341975](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39957341975)
built engine source `5c30cce3b7d7ac1dc3aebc9f0f39171205496472`, including the
failed-presentation lock-release correction. Fixture generation and the Windows
job passed: strict compile/link, native policy/five Python tests, all four
benchmark cases and paired LibVLC comparisons, ZIP creation and independent
checksum/ZIP readback. This is the current engine package, superseding the older
links above without deleting them.

[Download artifact 12503386782](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39957341975/artifacts/12503386782)
contains `libmpv-stage246A-win64-5c30cce3b7d7.zip`.
Inner ZIP SHA-256: `b76c701a202104b794e442d0dd27919c20faf612f2f56d46d2e0e7ca0f148b94`.
Raw current-run evidence artifact: `12503750668`. Retention remains 14 days.
Final documentation/analysis corrections may follow this source commit; no
unbuilt change to the preview engine is implied by a later documentation SHA.

Extract the Actions download, verify/extract its inner ZIP, then change into
`libmpv-stage246A-win64`. Requires official 7-Zip. For downloaded unsigned scripts,
these launches affect execution policy only in each child PowerShell process:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Get-Runtime.ps1 -Destination "$env:USERPROFILE\VLC-246A-Runtime"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\Run-Preview.ps1 -Runtime "$env:USERPROFILE\VLC-246A-Runtime" -Media 'C:\Videos\movie.mkv' -Results "$env:USERPROFILE\VLC-246A-Test1"
```

Use new destination/results folders. This opens only the independent experimental
window, with default OpenGL rendering and CPU decoding. Hover its strip, enter
milliseconds in the console, or enter `quit`. Benchmark commands and the manual
acceptance checklist are above and in the package README. Do not replace any VLC
files or the Stage 2.4.5 runtime.


The final engine run's four mpv image analyses completed with the same
provisional median/p95 errors listed above (62 VFR, 101 ordinary, 79 long-GOP
keyframe and 36 exact images). Comparison-image conversion failed with FFmpeg
exit 234 before OCR; partial results/error details were preserved in accuracy
artifact `12504240084`. Inspection found the scorer incorrectly demanded a
320-pixel crop while the unchanged LibVLC helper permits narrower output. A real
313×180 BMP reproduced FFmpeg's invalid-crop failure locally. Cropping the actual
input width fixes that regression; six local Python tests pass, including real
FFmpeg/Tesseract processing of the narrow BMP and rejection of a blank image as
a timestamp. Each offline FFmpeg/Tesseract subprocess now has a 30-second limit.
This analysis-only correction does not change either decoder or the executable.
