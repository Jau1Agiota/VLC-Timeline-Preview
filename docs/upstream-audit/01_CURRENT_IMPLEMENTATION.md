# 01 — Current implementation and upstream-sized scope

Audit date: 2026-10-25. Read-only research; no application builds, runtime
experiments, production edits or VideoLAN submissions. Baseline Stage F:
`31c583d5f83f18e06d9853d6e59adca26c1f68e9` (PR #20).
Upstream inspected directly from `https://code.videolan.org/videolan/vlc.git`:
`master` at `2556a9abdfc491d77d0cd96388f80153b4ae3910`.

**Confirmed** means directly supported by inspected source, public API metadata,
or explicitly identified existing evidence. **Inferred** means an architectural
judgment, not a maintainer decision. **Unverified** means an open question or
experiment not performed. Issue/MR states are observations on the audit date,
not permanent statements. Source links below are revision-pinned; issue links
remain live.

## Technical inventory

The actual-implementation column is **Confirmed** from the pinned Stage F source;
proposed dispositions are **Inferred**.

| Layer | Actual implementation | Upstream disposition — **Inferred** |
|---|---|---|
| Hover entry | [modules/gui/qt/util/input_slider.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/input_slider.cpp#L466) reads current input, duration, seekability, activity, mouse/buttons, video choices and media token. Hover does not call foreground seek controls. | Keep separation of observation from seeking; reimplement against current player/QML interfaces. |
| Mapping | [modules/gui/qt/util/seekhover.hpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/seekhover.hpp#L30) maps actual style handle endpoints; exact mode instead follows the existing click fraction, including its float conversion. Chapters remain a separate click path. | Preserve test intent, not VLC 3 widget-specific arithmetic. |
| Scheduling | [modules/gui/qt/util/thumbnailpreview.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/thumbnailpreview.cpp#L106) controls a private `QProcess` on the GUI thread; decoding is in another process. libmpv debounce 16 ms; other backend default 35 ms; deadline 6500 ms. | Thin UI adapter over a native backend; avoid a new external engine. |
| Backpressure | One running request plus one overwritten pending request; global active/extracting controller arbitration. | Reuse behavioral contract and tests. |
| Media eligibility | Fixed local drives on Windows; readable MP4/MKV; canonical path/size/mtime plus media token, backend/mode and request time form the hashed cache key. Times beyond 86,400,000 ms are refused. | Replace experiment limits with upstream capability/policy decisions. |
| Cache | GUI-thread shared LRU: 512 entries and 256 MiB accounted pixel ceiling, maximum 320×180 image. At full-size ARGB, 512 entries represent about 112.5 MiB pixels, excluding Qt/container/process overhead. | Retain bounded caching, reconsider budget and complete identity. |
| Fast worker | [prototypes/stage_2_4_6B/worker.c](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/prototypes/stage_2_4_6B/worker.c) retains one libmpv instance and uses keyframe seeking. | Reference/control only. |
| Exact worker | [prototypes/stage_2_4_6C/worker.c](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/prototypes/stage_2_4_6C/worker.c#L45) retains paused libmpv and SW render context; audio, subtitles, scripts/config and hardware decoding disabled. Uses `absolute+exact`. | Reference/control only; no direct core transplant. |
| Transport | Bounded JSON header plus exactly 230400 BGR0 bytes; sequence/request validation; GUI-owned QImage copy. No per-hover BMP files for libmpv. | Native `picture_t`/existing preparser IPC can avoid another custom protocol. |
| Popup | [modules/gui/qt/util/timetooltip.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/timetooltip.cpp#L65) anchors to the live timeline; one `setGeometry` transaction, separate fullscreen screen bounds and windowed owner bounds. | Reuse geometry invariants; current Qt Quick popup needs its own integration. |
| Fullscreen | [modules/gui/qt/util/timetooltip.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/timetooltip.cpp#L280) uses transient ownership and Win32 nonactivating topmost presentation. | Do not port HWND behavior into a cross-platform QML feature. |
| Stage E | [modules/gui/qt/util/timetooltip.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/timetooltip.cpp#L341) paints unchanged-size image, bottom fade/orange glow and centered time pill. Retained image keeps its own request label. | Retain honest association; styling is a separate design choice. |
| Stage F | [extras/package/portable/launcher.c](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/extras/package/portable/launcher.c), [extras/package/portable/Start-Portable.ps1](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/extras/package/portable/Start-Portable.ps1) establish private libmpv/exact environment and profile, fetch/cache verified runtime, stage helpers locally for removable application storage. | Packaging proof, outside an upstream feature patch. |

## Frame identity and persistence

**Confirmed:** exact requests preserve the click-mapped millisecond target; other
paths floor to a 250-ms bucket. Existing timing logs measure worker receipt or
controller presentation callbacks, not guaranteed physical-screen presentation.
Prior benchmark records are evidence with that scope, not new measurements here.

**Confirmed:** exact worker completion requires command acknowledgement, SEEK,
PLAYBACK_RESTART and a new non-redraw/non-repeat rendered frame
([prototypes/stage_2_4_6C/worker.c](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/prototypes/stage_2_4_6C/worker.c#L68)). These are causal fences, not image PTS.
The protocol explicitly sends `actual_image_timestamp_ms:null`,
`candidate_state:provisional`, `frame_timestamp_verified:false`. The tooltip's
minimal time pill is the displayed image's **requested** time, not a verified
sample timestamp. Exact seeking is not a universal accuracy guarantee.

**Confirmed:** presentation is not strictly “only newest wanted generation.”
[modules/gui/qt/util/thumbnailpreview.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/thumbnailpreview.cpp#L418) permits an in-flight intermediate
result if its generation advances the last displayed generation and media/lifecycle
still match. A newer cache presentation prevents subsequent older output. The
retained/intermediate image is labeled with its own request, never a new hover's
request. An upstream specification must explicitly choose this policy.

**Confirmed:** libmpv retains its decoder across requests. The 30-second GUI idle
timer is started in the **LibVLC** result path, not `mpvResult`; the LibVLC branch
also restarts after 64 requests. Do not describe these as libmpv limits.
The libmpv worker has a separate long idle watchdog and five-second active request
watchdog. Windows Job Objects contain processes and impose 1 GiB libmpv / 512 MiB
LibVLC memory limits. The destructor explicitly waits up to one second on the GUI thread
([modules/gui/qt/util/thumbnailpreview.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/thumbnailpreview.cpp#L148)).

## Development history relevant to review

**Confirmed**, from the feature files' Git history:

| Commit | Change |
|---|---|
| `f37cb04f9c` | Asynchronous isolated Qt integration, Stage 2.4.2 |
| `1ede9db591` / `e879ce656c` | Fullscreen diagnostics / separate placement and passive presentation |
| `12a2fb1f96` / `644865c3ee` | Scheduling/cache work / persistent LibVLC helper |
| `10c4aecd0e` / `5cfebdb971` | Persistent libmpv raw pipe / retained-image request association |
| `febfd332f4` | Exact seek worker and click-aligned requests, explicit provisional provenance |
| `52ad287063` | Atomic popup geometry and separate playback diagnostics |
| `0934ef24f0` / `e577355ed2` | Stage E visuals and tiny-image label guard |
| `15da8e5168` / `8f11eefc57` | Portable bootstrap / explicit OS hash-module import |

**Confirmed:** separate A/A.1 experiments informed the design; A.1 worktree head
`e9d79b96ae4af3691693c894d8b4e44b11a999d8` contains the standalone visual prototype.
It is historical reference, not the frontend shipped by Stage F.

## Quality review and minimum useful contribution

**Confirmed:** QObject/QProcess/timers/cache are GUI-thread-owned; the independent
worker owns decoding. Cancellation invalidates generations, clears presentation,
and kills private processes on media changes/teardown. It does not seek the active
player. The worker's Win32 watchdog, binary stdio, process accounting and BGR0
layout are Windows-specific. No current Linux/macOS worker portability is proven.

**Inferred:** filesystem stat/canonicalization in `identity()` on hover and the
100-ms health timer can still stall the GUI on slow fixed drives. Decoder/thread
count and total CPU are not bounded by the image-cache budget. A retained decoder
can consume substantial memory; the Job Object ceiling is not expected usage.

**Confirmed:** worker CLI/protocol carries file and timestamp, not the foreground
selected video-track ID. **Inferred:** alternate tracks, rotation, crop, colour/HDR
and playback filters need an explicit upstream policy and fuller cache identity;
raw-file preview need not match every foreground rendering transformation.

**Confirmed:** [test/modules/gui/qt/thumbnail/controller_test.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/test/modules/gui/qt/thumbnail/controller_test.cpp) covers
mapping, debounce, cache bounds, lifecycle/staleness, protocol failure, popup
geometry, labels and synthetic painting. [docs/evidence/stage_2_4_6F_ci.json](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/docs/evidence/stage_2_4_6F_ci.json)
records prior real Windows CI, including packaged launcher startup in a spaced
path and Qt dummy-output diagnostics. These are **existing evidence**, not tests
run during this audit, and do not establish physical GUI/USB/offline acceptance.
The existing positive-origin timestamp limitation remains; frozen-playback
investigation is not resumed.

**Inferred — minimum feature:** image plus well-defined time association; passive
hover with existing seek/keyboard/chapter/accessibility behavior intact; bounded
asynchronous requests; media-scoped cache; cancellation/lifetime correctness;
audio-only/unavailable-preview timestamp fallback; theme/DPI/fullscreen-safe
presentation. Preserve these acceptance tests while replacing platform adapters.

**Inferred — exclude from the initial upstream series:** standalone libmpv/LibVLC
workers and SDK downloaders, Stage F launcher, experimental process protocol,
private CI packaging, playback-freeze observer, verbose investigation tracing,
and mandatory orange-glow/Verdana styling. Keep synthetic fixtures and evidence
as references after provenance review. **Unverified:** performance equivalence
of any native replacement; see [02](02_NATIVE_VLC_FEASIBILITY.md).
