# Stage 2.4.6C accuracy investigation (work in progress)

Base: PR #16 head 8161fa1a0e88efc0a2911255164ca22503c3d8f6.
Stage B user-confirmed smooth/no blanks on RTX 3060/1080p Main10 HEVC is preserved
as control. All previous source worktrees and packages remain untouched.

Before changes: hover uses style handle centers and integer floor milliseconds,
then Controller::bucket floors to 250ms. Click uses getValueFromXPos (QStyle
sliderValueFromPosition with handleLength()/2 and width()-handleLength(), range
0..10000), then float(value)/maximum to InputManager::sliderUpdate and VLC's
position variable. Tooltip uses that click fraction times whole-second inputLength,
formatted to seconds. These are different geometries and precision paths. A pixel
on a long movie can span several seconds. The example labels alone do not prove
an engine offset, and a retained intermediate image may have an older request time.

The exact experiment uses the SAME click value/float fraction against current
millisecond duration, with no 250ms bucket. It changes preview requests only;
normal click/drag/keyboard/chapter and core playback code are untouched.
Fast remains the original B mapping/bucketing and original B worker binary.
Chapter-marker clicks are a separate destination path and are not promised to
match ordinary position previews.

Pinned mpv source e471496c56f0f49c133bd88b50aed62b845f1909 verifies:
- player/playloop.c seek(): KEYFRAME disables hr-seek; EXACT enables it; the
  seek event is emitted after reset. render reset forgets queued frames.
- player/video.c advances to STATUS_READY and waits for the first rendered frame
  before PLAYBACK_RESTART. Waiting to render only AFTER restart would deadlock.
- video/out/vo_libmpv.c NEXT_FRAME_INFO.target_time is presentation scheduling
  time, not the video's image PTS. client/render public APIs do not expose a
  reliable per-surface media PTS. video-frame-info has approximate metadata but
  no PTS; estimated timecode/time-pos are NOT a frame-image timestamp.

Exact worker: persistent paused SW player, hr-seek=yes, absolute+exact. It clears
candidate readiness at SEEK and only counts a new non-redraw/non-repeat image
rendered after that event; completion still also requires reply and restart.
This is a causal fence, NOT proof of image timestamp. The unmodified B worker source is
compiled separately and packaged as fast control. Neither seeks the foreground VLC instance.

Every exact frame carries request sequence, requested_ms, seek_strategy=exact,
actual_image_timestamp_ms=null, candidate_state=provisional and
frame_timestamp_verified=false. GUI verifies these against its running request;
controller generation/ID correlates the process-local sequence. No clock property
is misrepresented as PTS. Cache identity includes exact mode; pending frames keep
their OWN request label. Cancellation fences prevent canceled exact images from
resurfacing on immediate re-entry. Unknown PTS/delta remain unknown; no frame is
marked confirmed solely from seek completion. Debug logs include precise click,
request, generation and unknown PTS/error. Debug caption can show hover ms without
changing normal popup geometry or fullscreen flags.

VLC sliderUpdate sets position; src/input/input.c uses precise DEMUX_SET_POSITION
unless input-fast-seek is enabled. Exact mpv is a practical precision alignment,
not proof that two demuxers/codecs pick the identical frame at every cut. Source
start offsets, VFR and near-end behavior require image-content tests. No manual
constant offset is added. Normal decoding uses presentation order, not packet DTS.

Hybrid is not enabled: two seeks/updates may decode twice and visibly change an
image. Measure fast/exact first rather than inventing a refinement schedule.
No low-resolution stage, indexing, disk thumbnails in normal operation, or new
playback engine is added. Actual performance/accuracy results and device setup
will be recorded after the clean Windows build and native experiments.
