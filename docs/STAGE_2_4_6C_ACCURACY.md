# Stage 2.4.6C accuracy investigation

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
playback engine is added. Measured results and remaining failures are recorded in
[STAGE_2_4_6C_RESULTS.md](STAGE_2_4_6C_RESULTS.md).

## Mapping reproduction and interpretation

A real Qt Windows-base QProxyStyle model with VLC's 18px metric, width1280,
duration7200000ms and mouse x640 reproduces style centers8..1270. The old
style-center request is3605705ms, floored to3605500ms; the actual click value5000
produces3600000ms. This is a **5500ms difference with no decoding involved**.
The regression runs actual Qt geometry methods. It is a host Qt model, not a
measurement of the user's Windows monitor/style. The new exact path reads the
same value as the click rather than subtracting a guessed offset.

A retained image can also legitimately carry an older request from farther right
while the pointer moves left; its label alone is not proof of a future decoded
PTS. Both that scheduling effect and real keyframe error must be distinguished
from geometry. No constant timestamp correction is introduced.

The slider has 10000 value steps, finite physical pixels and a whole-second normal
tooltip. Millisecond diagnostics do not imply the mouse can select every millisecond
of a long film. Exact mode matches the click's float fraction; the precision target
is decoded image error relative to THAT selected destination. Chapter-zone clicks,
custom input-fast-seek preferences, edit lists and unusual time origins may follow
different semantics and remain separate test cases.

## Windows setup and backend/mode selection

Exact mode remains experimental: nonzero-origin media have measured multi-second
errors, and synthetic HEVC is slower. See the results report before comparison.
The default fast mode remains the Stage B performance control.

Extract the NEW portable ZIP into a new folder, never over an earlier VLC build.
The artifact wraps an inner application ZIP plus SHA256SUMS/PACKAGE_MANIFEST.json.
Verify the inner ZIP against that manifest before extraction. The libmpv DLL is
NOT bundled: corresponding-source/FFmpeg redistribution obligations remain
unresolved. The same identified/checksum-pinned shinchiro20261021 SDK from StageB
is downloaded for private use only. Main VLC and the separate official3.0.24
LibVLC helper runtime remain intact. See thumbnail-mpv/THIRD_PARTY.md.

From the extracted VLC root (replace the media example with your actual file):

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Setup-Runtime.ps1 -Destination C:\VLC-246C-Runtime1
# Accuracy comparison:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode exact -Runtime C:\VLC-246C-Runtime1 -Media 'C:\Videos\movie.mkv'
# Stage B fast algorithm/mapping control:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode fast -Runtime C:\VLC-246C-Runtime1 -Media 'C:\Videos\movie.mkv'
# Existing LibVLC helper fallback (SeekMode is ignored):
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libvlc -Media 'C:\Videos\movie.mkv'
```

Run one comparison at a time for performance, using the same playback state,
media, display, video-output settings and hover points. Close only the experimental
window between modes; never terminate all VLC processes globally. --no-one-instance
prevents forwarding to an older VLC. Runtime folders must be new. Setup copies
both workers next to the verified DLL; Start checks the selected worker and DLL
hashes before launch. Mode environment variables affect only that child.

Defaults remain LibVLC backend and (when libmpv is selected) fast seeking. Direct
configuration is VLC_THUMBNAIL_BACKEND=libmpv, VLC_MPV_SEEK=exact and VLC_MPV_HELPER
pointing to mpv-thumbnail-accurate-worker.exe. Fast uses mpv-thumbnail-worker.exe.
Do not mix a helper executable with another mode's selector. Unknown mode or
missing/failed helper is rejected without touching foreground playback.

## Device checklist (not performed in this cloud task)

1. Open the SAME RTX3060/1080p10-bit HEVC movie used for Stage B. Confirm ordinary
   playback/audio, no unexpected helper windows, and no blank images after first
   success. Test first hover, distant points, rapid reversal and stationary hover.
2. Revisit about46:23 and1:10:52. Pause before clicking the same ordinary timeline
   point so subsequent playback time does not confound the image comparison.
   Compare actual scenes after the seek settles; inspect both caption and content.
3. With synthetic fixtures, inspect visible PTS and alternating scene colors on
   both sides of cuts. A matching clock value alone proves nothing. Capture VLC
   snapshots after paused clicks for human side-by-side review. The automated
   LibVLC helper is a start-time proxy, NOT an automated test of a Qt click.
4. Test beginning/end, VFR, long GOP and positive start offsets. Inspect the actual
   duration reported by each engine. Record failures rather than applying offsets
   based only on one movie's captions.
5. Test pause/resume, drag/click/keyboard/chapter actions, switch media while busy,
   leave/re-enter immediately, fullscreen toolbar/auto-hide and multiple monitors.
6. Close VLC during a seek; check the exact child PID exits. Test missing runtime
   and terminate only the experimental preview child to exercise failure recovery.
   Select fast or LibVLC for controlled rollback.
7. Watch CPU/GPU/memory for both VLC and its own preview child during100+ reversals.
   Record first, warm median/p95 and final-target responsiveness separately. Smooth
   retention of an old image is not proof that the newest target arrived promptly.

Add -Diagnostics to the launch command for a new %TEMP%\vlc246C-*.log (disabled
normally). Normal mode uses --verbose=0; diagnostics use verbosity2. Logs include
style geometry time, click_ms, requested_ms, running/wanted generation, process-
local request ID, strategy, candidate_state=provisional and unknown actual-image
PTS/error. Exact hover captions can show milliseconds. Never interpret unknown
PTS/error as zero, or the requested image label as a decoded frame timestamp.

## Reproduction

Worker only (no VLC rebuild):
`CC=x86_64-w64-mingw32-gcc bash prototypes/stage_2_4_6C/build.sh VERIFIED_SDK NEW_OUTPUT`.
This requires the unchanged dependencies.json SDK SHA and strict warnings. The
full pipeline uses the existing pinned VideoLAN Docker image/GCC6.4.0/Qt5.6.3,
matching prebuilt contrib, isolated official VLC helper runtime and unchanged disk
cleanup/free-space/contrib verification guards.

Manual full pipeline:

```bash
gh workflow run windows-diagnostic-pilot.yml --repo Jau1Agiota/VLC-Timeline-Preview \
  --ref experiment/stage-2.4.6C-accurate-seeking -f stage=2.4.6C \
  -f benchmark_only=false -f native_only=false
```

Fixture generation uses the original Stage3 generator plus
prototypes/stage_2_4_6C/generate_extra.py (FFmpeg and a licensed font required).
These scripts index ONLY synthetic test clips to establish truth; no background
indexing is added to the product. hevc is30s1080p24fps/Main10/10sGOP, scene_cuts
alternates white/blue every second, and offset remuxes ordinary video with+5s PTS
while keeping its burn-in unchanged. Font binaries are not exported.

The native benchmark compares separately built original fast/exact workers,
alternating mode order per media, seven distant/edge requests and20 rapid intents
at5ms cadence with one active/one latest pending. Diagnostic BMPs are saved only
by the test harness. Rendering/pipe receipt are distinct from Qt paint or physical
scanout. All exact runtime image timestamps remain unknown; OCR is independent,
provisional and requires human review. Actual run IDs, checksums, raw evidence,
measured results and remaining limitations are in
[STAGE_2_4_6C_RESULTS.md](STAGE_2_4_6C_RESULTS.md).

## Verified local checks

- GCC6.4.0 MinGW-w64: exact worker compiles and links against the pinned libmpv
  import library with `-Wall -Wextra -Werror`. This is not a Windows execution test.
- Qt controller harness: existing lifecycle/cache/debounce/stale-generation tests
  and new exact-mode request/provenance/cancel-and-reenter cases pass. It runs host
  Qt with a fake decoder, not the Windows application or real media.
- Actual Qt slider-geometry reproduction and pure timestamp-mapping checks pass.
- CI safety/packaging regressions pass for StageC and the StageB control:43 tests
  each, one explicitly skipped. Disk threshold and cleanup allowlist are unchanged.
- Two Python OCR/scene-BMP parser tests pass. These check evidence handling, not
  the correctness of real decoded frames.
- Bash syntax, ShellCheck0.10.0, actionlint1.7.7 and git diff whitespace checks pass.
- Extra fixtures were generated locally and ffprobe confirms Main10 HEVC and the
  positive5s stream offset. Local fixture generation is not native accuracy proof.

Application source is pinned to febfd332f4ffbc94981f0b1cac298dc941de057b for the
clean package; later branch commits improve tests, measurement and documentation
without changing the application or worker. Build/benchmark execution is recorded
separately from these local checks. No physical Windows GUI/RTX3060 testing or
PotPlayer comparison has been performed here.
