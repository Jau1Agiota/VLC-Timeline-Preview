# 02 — Native VLC feasibility, APIs and experiments

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

## Current upstream is a different target

**Confirmed:** [meson.build](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/meson.build#L1) identifies `4.0.0-dev`, C17/C++17 and Meson
≥1.10.0. [modules/gui/qt/meson.build](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/meson.build#L5) requires Qt 6 ≥6.2 with QML/Quick.
Stage F's GCC 6.4.0/Qt 5.6.3 build is not a validated build environment for this
source. The authoritative default branch is `master`, not our VLC 3.0.24 branch.

**Confirmed:** the active timeline is [modules/gui/qt/player/qml/SliderBar.qml](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/player/qml/SliderBar.qml#L74).
Hover currently supplies a timestamp/chapter string to `PointingToolTipAttached`;
press/drag use a separate FSM and set `Player.position`, with chapter attraction
unless precise interaction is requested. Keyboard/accessibility actions exist.
[modules/gui/qt/widgets/qml/PointingTooltip.qml](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/widgets/qml/PointingTooltip.qml#L25) and
[modules/gui/qt/util/pointingtooltipattached.hpp](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/util/pointingtooltipattached.hpp#L27) provide the existing popup
and shared attached instance. These inspected paths do not implement hover-frame
extraction/display. This scoped source result is not a claim that all VLC products
or unmerged branches lack previews.

**Confirmed:** proposals exist:

- [#1251](https://code.videolan.org/videolan/vlc/-/issues/1251): open seek-bar thumbnail request.
- [#29393](https://code.videolan.org/videolan/vlc/-/issues/29393): open WIP covering demux/adaptive previews, sidecars, generated previews and media-library sources. Its proposed `vlc_previews`/input controls were not found in the inspected `include/`, `src/`, demux and Qt trees; proposal names are not verified APIs.
- [!7493](https://code.videolan.org/videolan/vlc/-/merge_requests/7493): open macOS slider preview MR, head `6f53c171803cad33e6e987782832d21c07965bfd`. Diff contains caching/hover UI and native preparser calls using older request-ID APIs; not a merged Qt solution or a current-master compatibility proof.
- [!7822](https://code.videolan.org/videolan/vlc/-/merge_requests/7822): closed, unmerged, despite its documentation-style title contains Qt extraction/UI/build code. #29393 records that this earlier Qt-driven generation architecture did not fit the broader design. Review notes returned HTTP 401; do not invent the MR's precise closure rationale.
- [medialibrary #493](https://code.videolan.org/videolan/medialibrary/-/issues/493): meeting notes discuss generation cost, tile metadata, sidecars, mobile resource use and avoiding automatic whole-library generation. Its historical 10–45-second movie-generation statement is not our measurement or a single-hover latency benchmark.

## Verified native interfaces at this revision

All rows are **Confirmed**; these are not VLC 3 APIs.

| Interface | Contract / source |
|---|---|
| `vlc_preparser_New` | Configure `VLC_PREPARSER_TYPE_THUMBNAIL`, thread limit, timeout, optional external process: [include/vlc_preparser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc_preparser.h#L283). Zero thread count defaults to one; timeout zero means unlimited. |
| `vlc_preparser_req_NewThumbnail` | Creates an idle caller-owned request holding an `input_item_t`: [include/vlc_preparser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc_preparser.h#L375). Args select time/position, PRECISE/FAST and `hw_dec` default false. |
| `vlc_preparser_Submit` | Exactly one completion after successful submission; callback can run before Submit returns: [include/vlc_preparser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc_preparser.h#L430). |
| `vlc_thumbnailer_cbs.on_ended` | Status plus `picture_t*`; hold the picture past callback scope, null on failure/cancel/timeout: [include/vlc_preparser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc_preparser.h#L125). |
| `vlc_preparser_Cancel` / `vlc_preparser_req_Release` | Cancel can call back synchronously for queued work; release alone does not cancel. Context must outlive termination: [include/vlc_preparser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc_preparser.h#L454). |
| `vlc_preparser_req_NewThumbnailToFiles` | Output encoding/scaling/cropping and file writing; not necessary for a transient hover image. |
| Public LibVLC 4 parser | `libvlc_parser_new`, `libvlc_parser_task_new_thumbnail`, `libvlc_parser_submit`, `libvlc_parser_cancel_request`, task release: [include/vlc/libvlc_parser.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc/libvlc_parser.h#L385). Useful for a separate experiment; internal Qt integration should normally use native core interfaces. |
| Picture provenance | Native `picture_t.date` exists. [lib/picture.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/lib/picture.c#L58) copies it into public pictures; [include/vlc/libvlc_picture.h](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/include/vlc/libvlc_picture.h#L121) exposes `libvlc_picture_get_time` in **microseconds**, not the old millisecond assumption. |

**Confirmed:** [src/preparser/internal.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/preparser/internal.c#L506) creates a fresh independent
`INPUT_TYPE_THUMBNAILING` input for each request, queues its seek before starting,
waits for a frame/error/timeout, then stops and closes the input. It does not seek
the foreground player. [src/input/es_out.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/input/es_out.c#L2750) normally excludes audio and
subtitles for thumbnailing, and [src/input/decoder.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/input/decoder.c#L2129) uses thumbnail
callbacks rather than an ordinary visible vout. No foreground silence/window
claim has been runtime-tested here; unusual options/modules still need validation.

**Confirmed:** native isolation already has an out-of-process option.
[src/preparser/external.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/preparser/external.c#L1138) maintains process pools, using the existing
`vlc-preparser` executable and core IPC. [!6854](https://code.videolan.org/videolan/vlc/-/merge_requests/6854)
is merged. [modules/misc/medialibrary/Thumbnailer.cpp](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/misc/medialibrary/Thumbnailer.cpp#L36) uses it where available.
[configure.ac](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/configure.ac#L601) requires process spawning and shared linking for the
Autotools path; [src/meson.build](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/meson.build#L331) contains desktop platform definitions.
Unsupported builds need an explicitly chosen in-process fallback, not forced
external operation. Public LibVLC's wrapper currently sets `external_process=false`
([lib/parser.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/lib/parser.c#L423)), so the public parser is not proof of process isolation.

**Confirmed:** process reuse does **not** reuse an open demuxer/decoder across
thumbnail requests: the child runs the same per-request internal implementation.
External completion can update the shared item's metadata
([src/preparser/external.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/preparser/external.c#L185)). **Inferred:** snapshot/copy the media item and
its permitted options when needed to avoid unintended metadata coupling. Separate
input ownership proves no shared seek handle, not zero CPU/disk impact on playback.

**Confirmed:** MainCtx's existing network preparser is configured only for PARSE,
with `external_process=false` and no timeout, not for thumbnail requests
([mainctx.cpp](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/gui/qt/maininterface/mainctx.cpp#L279)).
A preview service needs an appropriately configured preparser; an existing pointer
is not proof it supports thumbnail operations. Its worker count limits request
concurrency, not necessarily all codec threads or full-resolution decode memory.

## Accuracy and hardware are unresolved gates

**Confirmed:** PRECISE is documented “precise, but potentially slow”; FAST is
“fast, but potentially imprecise.” Time uses `vlc_tick_t`; prefer integer time
requests for experiments. Position is downcast to float inside `ThumbnailerRun`.
`picture_t.date` and external serialization of `date`
([modules/misc/preparser_serializer/json/tojson.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/modules/misc/preparser_serializer/json/tojson.c#L1054)) provide stronger
frame-associated evidence than libmpv's unknown image PTS, but not proof of a
normalized media time for every demuxer, edit list, offset or clock domain.

**Confirmed:** [test/src/preparser/thumbnail.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/test/src/preparser/thumbnail.c#L80) disables its expected picture
`date` assertion under `#if 0`, with a clock-related TODO. Thumbnail delivery is
one-shot (`b_first`), [src/input/decoder.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/input/decoder.c#L1595). **Unverified:** target error
budget, origin normalization, preroll/long-GOP/near-end selection and visual
agreement with clicking. A timestamp field alone does not establish equivalence.

**Confirmed:** args expose `hw_dec`, but the thumbnail decoder's `get_device`
callback returns NULL “on purpose” to avoid DLL/picture allocation overhead
([src/input/decoder.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/input/decoder.c#L1595)). **Unverified:** effective acceleration by codec,
platform and module. Setting the flag is not evidence of RTX/VAAPI/VideoToolbox
acceleration or improved latency; software should be the initial control.

## A/B/C comparison

| Approach | Feasibility and complexity | Risks / maintenance / unknowns |
|---|---|---|
| A — retain libmpv subprocess | **Confirmed:** current Windows prototype and prior tests exist. **Inferred:** mechanically feasible, but high upstream maintenance cost. | Second media engine, separate dependencies/security updates, GPL/static dependency source audit, cross-platform process containment and duplicated format/track/colour behavior. No verified maintainer prohibition, but substantial review concerns. Does not solve unknown frame PTS. |
| B — existing native thumbnail requests | **Confirmed:** async API, independent input, cancellation, picture ownership/date and native process backend exist. **Inferred:** lowest-complexity native baseline; medium Qt adapter/testing work. | Per-request input/decoder initialization; full-size decoded picture before scaling; possible foreground resource contention. Hover latency and accuracy unmeasured. Native ML cover generation is not an equivalent hover benchmark. |
| C — VLC-native provider/session or hybrid | **Inferred:** use supplied stream/sidecar/ML previews when available, on-demand B as a possible provider, and investigate a persistent private input only if cold-open costs demand it. High complexity for a reusable decoding session. | No verified ready-made persistent thumbnail-session API. One-shot callbacks/lifecycle, seek flush/preroll, track changes, recovery, resource limits and clock normalization require core design/tests. Do not replace this with an ordinary player's timestamp-less memory callback and declare accuracy solved. |

**Inferred:** recommend the native direction aligned with #29393, with B as the
first measurable extraction building block. Do not submit a new Qt-only decoding
service or libmpv backend unchanged. Maintainers must decide how on-demand fallback
fits the broader preview-source contract; proposed WIP APIs are not callable today.

## Tests and smallest next experiment — proposed, not executed

**Confirmed:** the standalone build documentation still demonstrates
`test_src_input_thumbnail` ([doc/standalone/buildsystem.md](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/doc/standalone/buildsystem.md#L228)). Current build
registrations are `test_src_preparser_thumbnail` and
`test_src_preparser_thumbnail_to_files` ([test/src/meson.build](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/test/src/meson.build#L112),
[test/Makefile.am](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/test/Makefile.am#L257)). Test source covers mock video/audio-only, fast/precise,
position/time, delayed video, timeout and cancellation. It is not a real-media
latency benchmark. External-path coverage needs inspection: the thumbnail test's
external block is guarded by **`!defined(HAVE_VLC_PROCESS_SPAWN)`** at lines 257–261,
whereas the comparison test skips when spawning is unavailable. This is a confirmed
source/coverage concern, not a reproduced failing test or an upstream bug report.

**Inferred:** after separate approval, make a small standalone native request
harness on this pinned master, without timeline UI or production modifications.
Keep one preparser object; issue bounded serial PRECISE/FAST requests at widely
separated times on the same burned-in MP4/MKV/HEVC/VFR/long-GOP/offset/cut fixtures.
Record cold open vs repeated request, image-ready vs command time, cancellation
completion, raw picture date, calibrated media time, burned-in PTS error, peak
memory/CPU and foreground A/V continuity. Test queued/running cancellation,
callback-before-Submit-return, media switch, corrupt/audio-only input and shutdown.
Convert/scale off the GUI thread; measure its cost separately. Exercise internal
and supported external backend explicitly. No numeric performance prediction is
established. Compare Stage F on identical media/device as a control, not synthetic
numbers from another machine. No full-video index or libmpv rewrite is needed.

For a future **already configured** current-master Meson build with tests enabled
(`-Dtests=enabled`) and mock/rawvideo modules, the following are planning commands,
not commands executed in this audit:

```sh
meson test -C build-meson --list
meson test -C build-meson test_src_preparser_thumbnail --print-errorlogs
meson test -C build-meson test_src_preparser_thumbnail_to_files --print-errorlogs
```

**Unverified:** native cold/warm median/p95, reusable decoder benefit, cross-platform
HW paths, normalized image date and maintainer acceptance of any provider API.
