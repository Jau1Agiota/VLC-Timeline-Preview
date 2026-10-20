# Proposed thumbnail architecture — VLC 3.0.24

Design only; no implementation or performance claims. References below are to release commit `6de05adcbaf2e8b85fe86aad4169393098628119`.

## Existing Qt timeline and playback boundary

`modules/gui/qt/util/input_slider.hpp/.cpp` implements `SeekSlider : QSlider`. It already tracks mouse movement, hover entry/exit, dragging, buffering, chapter markers, and a `TimeTooltip` widget. `setPosition(float, int64_t, int)` receives normalized playback position and duration in seconds; it disables interaction for an invalid position or non-seekable input. `inputLength` is seconds, while chapter seekpoints use microseconds (`CLOCK_FREQ`); LibVLC times use milliseconds. Explicit checked conversions are essential.

`mouseMoveEvent` updates the slider only while sliding, then computes the existing time/chapter tooltip. `getValueFromXPos` / `getValuePercentageFromXPos` encode handle-aware coordinate mapping. Reuse and test that mapping, clamp the normalized fraction, reject zero duration and narrow widgets, and preserve existing orientation behavior. Do not implement a different hover fraction that disagrees with click seeking. Vertical timelines already have TODOs; keep a text-only fallback until explicitly tested.

`mousePressEvent` preserves left/middle clicks and chapter jumps; release handles a final pending seek. `startSeekTimer` throttles drag seeks to 150 ms. `wheelEvent` also performs intentional seeking. Leave these handlers' playback behavior intact. Hover request scheduling must have its own timer, never reuse `seekLimitTimer` or emit `sliderMoved`/`sliderDragged`.

`modules/gui/qt/components/controller.cpp` connects `InputManager::positionUpdated` to `SeekSlider::setPosition`, `sliderDragged` to `InputManager::sliderUpdate`, buffering updates to the slider, and `inputCanSeek` to `setSeekable`. `InputManager::sliderUpdate` in `input_manager.cpp` calls `var_SetFloat(p_input, "position", new_pos)`. This is the active playback mutation boundary and must never be invoked by a preview request. `InputManager` also reports input replacement, state and duration; preview availability should follow current media identity and capabilities, not assume that a visible enabled slider alone proves a second decoder can open the media.

`modules/gui/qt/util/timetooltip.hpp/.cpp` is a native QWidget tooltip painted with QPainter. It already uses transparent mouse handling, chapter/time text, and screen clamping, with Qt5/Qt6 conditionals. A small image area in this widget or a similarly owned sibling widget is preferable to introducing a web/QML subsystem. Keep the existing tooltip as fallback on misses/errors. Recompute screen bounds and DPI positioning for the larger image; test fullscreen, multiple displays, scaling, window hide and destruction. Use QImage for thread transport; construct QPixmap and touch widgets only on the GUI thread. The default contrib Qt is 5.6.3: avoid unguarded newer Qt APIs.

New Qt files would require the source/header/MOC registration described in `modules/gui/qt/Makefile.am`. Such build-system and UI edits belong in Stage 2, not this documentation stage.

## Independent extraction options

| Option | Benefits | Risks / decision |
| --- | --- | --- |
| Dedicated LibVLC instance and media player, memory video callbacks | Reuses VLC demux/access/codec ecosystem; separates playback state | Cold startup, callbacks and seek readiness are asynchronous; extra decoder/network resources; Qt plugin linkage and re-entrant initialization need a prototype |
| Private FFmpeg demux/decoder context | Explicit packet, seek and timestamp control; no active-player state | Additional integration and licensing surface; duplicates VLC access/demux policy; cannot assume VLC-supported inputs are FFmpeg-supported |
| Separate extraction helper process | Can kill an unresponsive decoder without freezing Qt; isolates crashes | Packaging/IPC/startup overhead; executable discovery and Windows process cleanup; requires explicit build integration |
| Active-player snapshot or temporary seek | Simple access to current frame only | Reject: cannot decode arbitrary hover time without changing active playback |
| Private VLC input/vout internals | Native internal abstractions | Greater core coupling and lifecycle risk; defer until a prototype demonstrates a need |

Recommended first experiment: a dedicated LibVLC extractor, preferably hosted in a small private helper process if shutdown/timeout isolation cannot be bounded in-process. First verify that Qt-plugin linkage and second-instance initialization are safe in this custom VLC build; this has not been proven. Do not directly add a public-libvlc dependency to the Qt module without reviewing its existing libvlccore dependency and plugin initialization path. The helper can avoid that coupling. Do not require an arbitrary system FFmpeg executable for the shipped feature.

A private player must own a distinct media descriptor and never receive the active player's input/player pointer. Pass a validated source identity from the interface controller. Never copy all active input options: authentication, cookies, device access and stream-output options need explicit policy. Avoid passing credentials in command lines, cache keys, logs or IPC diagnostics.

For the LibVLC prototype, register memory output callbacks before play, suppress audio and subtitle output on the private media/player, disable hardware decoding only for that private decoder where supported, and negotiate a bounded pixel buffer. Validate the relevant options against the actual release; an ignored option is not proof of isolation. A private player must not create a visible native video window or acquire an audio device. Do not change global settings or the foreground player's hardware mode.

`include/vlc/libvlc_media_player.h` documents `libvlc_video_set_callbacks` and format callbacks. It explicitly warns that memory rendering can disable hardware decoding or require GPU-to-CPU copies, and incur CPU conversion/scaling. Honor pitch, line counts, alignment, negotiated dimensions and format; never assume tightly packed RV32 bytes or alpha semantics. Handle source dimension changes and callback overlap without use-after-free. Copy into an owned image before releasing callback buffers.

Wait for parsing/playback and seekability readiness before seeking. `libvlc_media_player_set_time` is asynchronous and does not return an extracted frame or guarantee exact frame accuracy. Reject pre-seek/stale output; validate timing near the target, decode forward from a keyframe and allow a documented tolerance. Display callbacks lack a frame PTS argument, so correlating them with `get_time` is approximate. If accurate timestamps cannot be established, expose previews as approximate or use a backend with explicit decoded PTS rather than claiming frame accuracy. Pausing before the target frame may prevent delivery; model decode, capture, then quiesce explicitly and test it.

`libvlc_video_take_snapshot` captures the current video output; it is not an arbitrary-time thumbnail API. Calling it on the foreground player cannot satisfy isolated hover extraction. VLC 4-only thumbnail APIs must not be assumed available in this VLC 3.x branch.

## Request scheduling and threading

A GUI-owned PreviewController sends immutable requests `(mediaGeneration, requestId, sourceKey, targetMs, outputSize)` to one worker. Start with local regular files, finite positive duration and confirmed seekability. An initial 100–200 ms debounce is a tunable proposal, not a measured optimum. Keep at most one extraction in flight and one pending latest request; coalesce repeated requests for the same time bucket. Prioritize current hover; no whole-video scan on open.

Results carry generation, request ID, actual/estimated timestamp and image. Accept only the current media generation and requested bucket; discard older results even if cancellation raced. Cancellation should be a generation/token invalidation plus cooperative worker checks, not unsafe QThread termination. Decoder calls may block; a timeout must invalidate results promptly but does not magically stop a blocked decoder. Use the helper-process design if bounded teardown cannot be achieved with the selected backend.

No decoder work, blocking wait, file/network IO or player shutdown runs on the Qt GUI thread. LibVLC callbacks execute on VLC threads: enqueue notifications and return quickly. Never invoke stop/release directly from a callback that may be joined by those calls. The worker serializes its player control operations and releases it only after callbacks have drained.

Lifecycle: input changed/stopped, slider hidden/destroyed, application closing or source invalidation increments generation, stops new work and hides the popup. Ordinary pause should still permit independent previews. Leaving hover cancels pending display work; retaining a small decoder/cache briefly is optional and bounded. Shutdown detaches events safely, stops the private player from its owning context, drains callbacks and frees buffers/media/instance in order. Widget lifetime must be guarded (e.g. QPointer plus queued receiver ownership); no raw GUI pointer may outlive its widget.

## Cache and resource policy

Use a byte-bounded LRU memory cache. A proposed starting limit is 32 MiB with 240×135 thumbnails; account for actual QImage stride and overhead rather than assuming compressed image size. Use time buckets (initially 1–5 seconds, adjusted to duration and timeline resolution), plus source identity, size/mtime for local files, video track, orientation, dimensions and extraction settings. Request IDs are not cache keys. Preserve aspect ratio, sample aspect ratio, rotation and color conversion policy; handle HDR as an explicit limitation before promising accurate SDR colors.

Do not key solely on path, playlist index or display name. Invalidate on replacement/size/mtime changes and on track/settings changes. File metadata does not guarantee content identity; a content fingerprint can be added if required. No persistent disk cache initially; later opt-in persistence needs an explicit quota, eviction, privacy policy and invalidation scheme. Avoid storing authenticated URLs or sensitive frames by default. Negative-cache failures briefly to avoid hammering unsupported files, with bounded retries and invalidation on a new source generation.

Limit decoder concurrency to one, cap resolution and in-flight memory, bound open/seek/decode time, and avoid speculative extraction under resource pressure. Separate state prevents deliberate playback seeks but cannot guarantee zero CPU, disk or GPU contention. Measure foreground audio underruns, frame drops and event-loop latency before enabling by default.

## Expected limitations and fallback

- Network inputs: a second reader can double traffic, prompt authentication, consume connection limits or trigger server-side state. Start disabled for network URLs; later require an explicit policy and safe credential handling. Range/seek support and finite duration must be verified, not inferred from scheme.
- Live, non-seekable, device, pipe, encrypted or DRM sources: no arbitrary-time preview. Show existing text only; do not reopen devices or seek the active input. Rolling DVR windows require a separate timestamp/window model.
- Hardware decoding: private CPU extraction may be expensive for 4K/8K, high bit depth or complex codecs; private GPU extraction can compete with foreground rendering. Default experimental extraction to software on the private decoder, then measure.
- Long videos and sparse keyframes: seeks may require substantial decode-forward work. Bucket adaptively, cancel obsolete requests, cap work and return a miss on timeout. Integer duration fields in the existing slider impose limits; do not amplify overflow by converting seconds to milliseconds in 32-bit arithmetic.
- Unsupported, malformed, missing or audio-only files: bounded failure and text fallback. Foreground playback support does not guarantee support in a different backend. Test plugin discovery, track selection, corrupt indexes, timestamp discontinuities and files that change while open.
- Optical-disc navigation, multi-angle content, adaptive streaming, external subtitles and transformed output: a raw extracted frame may differ from the foreground composited picture. Do not promise subtitle/OSD/filter parity; no foreground filter changes.

## Stage 2 acceptance plan

First produce and test the unmodified baseline. Then prototype extraction without UI on short/long seekable local files and verify time accuracy, no audible output and bounded cancellation. Add the smallest Qt integration only after backend lifetime and ABI concerns are resolved.

Regression checks must cover hover while playing/paused, drag/release/wheel and chapter clicks, rapid mouse motion, file replacement during extraction, stop/close/shutdown, multiple sliders/fullscreen, DPI/screens, codec failures and missing plugins. Confirm that hovering never writes active `position`/`time`, emits seek signals, changes pause/state or audio/video settings; intentional seeks retain their current semantics. Stress callback teardown and repeated open/close for leaks/deadlocks. Measure latency, cache hit rate, memory ceiling and foreground playback continuity. These are proposed tests, not tests executed in Stage 1.
