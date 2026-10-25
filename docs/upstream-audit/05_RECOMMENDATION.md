# 05 — Recommendation and smallest next research stage

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

## Executive conclusion

**Inferred:** upstream contribution appears technically feasible, but submitting
our current fork unchanged is not the recommended path. Keep it as user-experience
and regression evidence. Pursue a **VLC-native preview provider aligned with the
existing core preview work**, with a small Qt 6/QML presentation adapter. First
measure existing native thumbnail requests; investigate a persistent secondary
native pipeline only if those measurements show initialization/reuse is the real
bottleneck. Acceptance and equivalent responsiveness are **Unverified**.

## Most important discoveries

- **Confirmed:** upstream `master` is 4.0.0-dev with Qt 6/QML, not the VLC 3 QWidget
  timeline we modified. [modules/gui/qt/player/qml/SliderBar.qml](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/player/qml/SliderBar.qml#L74) currently
  wires hover to time/chapter text and separates press/drag seeking. Our 3.0.24
  files are not a patch-ready integration point.
- **Confirmed:** [#29393](https://code.videolan.org/videolan/vlc/-/issues/29393)
  already frames previews as a core/multiple-source feature. An open macOS MR and
  closed Qt proposal exist. New work should complement that direction, not assume
  this is an unclaimed feature request.
- **Confirmed:** native async requests, precise/fast seek options, cancellation,
  a frame-associated date and optional process isolation already exist in
  [include/vlc_preparser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc_preparser.h#L125). External worker reuse is **not decoder reuse**:
  [src/preparser/internal.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/preparser/internal.c#L506) creates/closes an input per thumbnail.
- **Confirmed:** native tests exist, but the documented `test_src_input_thumbnail`
  is stale; current target is `test_src_preparser_thumbnail`. Its frame-date
  assertion is disabled. Hardware-enable arguments also do not establish actual
  acceleration when the thumbnail decoder's device callback returns NULL.
- **Confirmed:** our exact libmpv worker has no verified image PTS. Stage F evidence
  retains the known positive-origin error; a successful image/seek acknowledgement
  is not universal frame accuracy. No native-equivalence measurements were made.
- **Confirmed:** current VideoLAN AI policy forbids agent submissions even with
  approval, requires human understanding and coding-assistance disclosure in the
  MR, and disallows AI co-author credit. Future contact must be human-led.

## What to reuse and what to replace

| Reuse as requirements/tests — **Inferred** | Replace or omit — **Inferred** |
|---|---|
| Passive hover distinct from foreground seeking | VLC 3 `SeekSlider`/QWidget integration, old float mapping |
| Bounded asynchronous scheduling, one active/latest pending work | libmpv DLL/SDK dependency and custom helper protocol |
| Media-generation invalidation and lifecycle cancellation | Win32-only Job Object/watchdog implementation in an otherwise portable feature |
| Retained image's own time association | Unknown-PTS success semantics as an accuracy guarantee |
| Bounded cache and explicit unsupported/error fallback | Fixed-drive/MP4-MKV-only/24-hour prototype policy, GUI-thread file polling |
| Live anchoring, atomic size/position, fullscreen/focus/DPI test cases | HWND topmost calls and global-coordinate rules transplanted to Qt Quick/Wayland |
| Own synthetic burned-in fixtures, near-end/VFR/long-GOP/offset tests after license review | Private films/recordings, standalone launchers, custom release packaging and playback-freeze diagnostics |
| Source-origin/latency distinctions and existing regression evidence | Mandatory orange-glow/Verdana styling in the initial functional patch |

**Inferred:** use the native in-memory picture result, not per-hover JPEG/BMP files.
A stable backend owns request/context lifetime and media identity; convert/scale
on a worker and queue retained image data to the GUI. Never call widget methods
from core callbacks. Prepare context before Submit; cancellation may synchronously
invoke it. Release requests and held pictures exactly once, and join backend
teardown away from an interactive UI path where necessary. Reuse existing async
Qt image-provider patterns, e.g. [modules/gui/qt/util/vlcaccess_image_provider.hpp](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/util/vlcaccess_image_provider.hpp#L65),
rather than introduce a second renderer. Provider/texture caching must be bounded
as well as CPU image caching.

**Inferred:** define cache identity for media epoch, chosen track, requested time/
quality, output size and applicable transforms. Cache-hit/intermediate/retained
presentation policy must be explicit; “latest wins” currently means monotonic
presentation, not strict newest-request-only. Preserve existing QML chapter
attraction, Shift behavior, keyboard and accessibility; a hover fraction is not
a promise that chapter-snapping clicks select that exact frame.

**Inferred:** native provided thumbnails/tiles can avoid expensive generation;
on-demand local extraction is a possible fallback, not a mandate for automatic
whole-video indexing. Persistent decode is an optional core optimization proposal,
not an established API. No production implementation is authorized by this audit.

## Main blockers and uncertainties

**Unverified:** native cold/warm/rapid random-hover latency on the user's long-form
10-bit HEVC media; normalized native picture dates vs burned-in frame PTS; precise
seek behavior at discontinuities/ends; effective hardware paths; resource contention
with normal playback; agreed provider API and frontend ownership. Do not infer
performance from a method named “thumbnail,” a warm process or the presence of a
hardware option. Core/Qt platform and current build-system compatibility need a
new environment; the Stage F image is for VLC 3.

**Confirmed / unresolved:** libmpv's exact distribution is not LGPL-only by default
and its complete corresponding-source obligations remain unaudited. It is not
bundled by Stage F. Source contributions additionally require provenance and
GPL-frontend/LGPL-core boundary review; an independently implemented idea is not
a license to relicense another contributor's GPL implementation. CLA/DCO/sign-off
rules and inaccessible review discussions were not fully verified. See [03](03_LICENSE_AND_PROVENANCE.md)
and [04](04_VIDEOLAN_CONTRIBUTION_PATH.md).

## Small patch sequence — proposed, subject to human/maintainer agreement

1. **Inferred:** documentation/test corrections for the stale target and native
   picture-time/cancellation contracts, only after human verification of observed
   source issues. Do not bundle unrelated test fixes into the UI feature.
2. **Inferred:** core/provider contract tests and narrowly scoped native on-demand
   provider integration consistent with #29393; reuse existing preparser. If a
   persistent-session optimization is justified, make it a separate core series
   with flush/preroll/time-origin/lifecycle tests, not a hidden GUI decoder.
3. **Inferred:** a small Qt bridge exposing request/result/media generation with
   bounded scheduling/cache and queued image delivery; no foreground seek calls.
4. **Inferred:** QML thumbnail content above the existing timeline using current
   popup/theme/DPI facilities, timestamp fallback and honest retained-image state.
   Preserve controls and chapter behavior; keep cosmetic refinements separate.
5. **Inferred:** adversarial lifecycle and cross-platform interaction tests plus
   reviewer-requested docs. Human handles the official MR, CI and discussion.

## Smallest next experiment and stop condition

**Inferred:** Stage 2.4.7.1 should be an isolated native API benchmark, not another
branded VLC distribution or a timeline rewrite. On a newly approved, pinned upstream
build, keep one preparser and issue serial/widely separated requests using real
burned-in fixtures, with controlled rapid supersession/cancellation. Measure
actual picture-ready latency, requested vs returned vs burned-in time, cold/warm
median/p95, memory/CPU and ordinary playback isolation. Compare supported internal
and external operation and the existing Stage F control on the **same device and
media**. Test microsecond/millisecond conversions, negative/nonzero origins,
rotation/aspect, selected-track policy, VFR, long GOPs, audio-only/corrupt input
and near EOF. No fixed delay or playback clock proves image identity.

**Unverified:** whether B alone meets the responsiveness target. If it does,
prefer it over a new persistent decoder. If not, use measured initialization and
decode costs to motivate C with maintainers. If provided-preview/sidecar work
already solves a use case, integrate through that common contract rather than
compete with it.

**Confirmed scope:** five reports only; no app build, runtime benchmark, production
edit, merge, release or VideoLAN contact. Stop for the developer's review and
approval of any subsequent experiment.
