# Stage 2.4.6A: isolated libmpv keyframe preview experiment

## Status

Experimental branch starts at the preserved Stage 2.4.5 feature head
`342e4028a87deddb99f71bf03bc7a50524be0bed`. Nothing in VLC production code,
the Stage 2.4.5 LibVLC engine, previous ZIPs or untracked build directories is
modified. The standalone Win32 program compiles and links with the existing
MinGW-w64 GCC 6.4.0 compiler. Native Windows/GPU results remain pending.

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
`video-pts` property after handoff is useful diagnostic evidence while paused,
but is explicitly not independent proof of the saved image timestamp.
`frame_timestamp_verified` and `physical_display_verified` remain false.

Use burned-in PTS fixtures and read the BMP itself to measure actual image error.
Keep `requested_ms`, `video_pts_property_ms` and independently observed burned PTS
as separate columns. Keyframe seeks can be seconds early on long GOPs, or choose
nearby keyframes; fast output must not be described as exact. An `exact` mode is
provided solely as an explicit accuracy/latency comparison.

Benchmark sampling uses 30 requests cycling between 3% and 95% of the file,
then six bursts of 12 requests at 5 ms intervals. Only the last target in each
burst is required to produce a visible result. Raw JSONL and BMPs are retained.
Median and nearest-rank p95, sample counts, CPU identity and actual renderer are
reported; no timing results exist yet at this revision. Compare Stage 2.4.5 only
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
