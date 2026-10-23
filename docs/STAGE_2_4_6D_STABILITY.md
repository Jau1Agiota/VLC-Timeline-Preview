# Stage 2.4.6D playback/popup stability

Base: `33c9670e8adb968e1cd158432dd85be8b809917b`, PR17 Stage C.
Branch: `fix/stage-2.4.6D-playback-popup-stability`. No baseline branch, package,
untracked win64 directory or earlier worktree is overwritten. PR16/17 stay unmerged.

The user confirms Stage C exact seeking is accurate and smooth on their RTX3060
with real 1080p Main10 HEVC. That physical finding is the baseline for this stage;
Stage C synthetic measurements are not substituted for this device observation.
No worker, seek/mapping, generation, debounce, cache or playback algorithm changes
are made here. Fast and LibVLC selectors remain intact.

## Freeze investigation: cause not established

The supplied 14.45s recording shows the main picture staying fixed while the
interface and preview images update. That supports the reported symptom and argues
against a complete GUI event-loop deadlock at that time. The user reports audio
continues. Neither recording provides decoder/driver logs or proves a decoder,
vout, driver or compositor cause. The short 6.75s recording shows foreground image
changes and popup/timestamp transitions. The user reports popup movement in BOTH
windowed and fullscreen modes; the supplied captures show windowed VLC.
Recordings/private movie pixels are not committed or redistributed.

Audit: preview Controller uses QProcess pipes, copied QImage pixels and a private
child Job Object. Only the child PID is assigned, terminated or given below-normal
priority. Its decoder is software (`hwdec=no`) and its mpv handle is process-local.
No foreground LibVLC/player/vout handle or decoder surface is passed to it. Hover
never calls sliderUpdate, var_Set(position/time), pause or vout reset. Foreground
click/drag/keyboard seeking remains the original InputManager/input path. The one
bounded wait is Controller destruction, not hover/extraction. CPU/memory pressure,
Qt/native popup composition, foreground hardware decoding, output synchronization
and an upstream VLC3.0.24 defect remain hypotheses, not demonstrated causes.

Existing source paths:
- `input_manager.cpp`: UpdatePosition/UpdateStatus/UpdateVout and lifecycle events.
- `src/input/decoder.c`: DecoderUpdateStatVideo aggregates decoded, displayed and
  lost counters; decoder and vout ownership are separate from the preview process.
- `src/video_output/video_output.c`: ThreadDisplayRenderPicture/ThreadDisplayPicture
  prepare/render/display and vout counters. A display-counter increment is NOT
  proof that a new frame reached the monitor.
- `modules/video_output/win32/direct3d11.c` and `direct3d9.c`: foreground Windows
  presentation; `modules/codec/avcodec/video.c` and `d3d11va.c`: decoding/hardware paths.
  No changes are made to these core/decoder/renderer paths.

There is no reproducible RTX3060/Windows GUI environment or original movie in the
cloud. Reproduction in stock VLC3.0.24/Stage B is pending the device matrix below.
**No freeze fix is claimed. No forced seek, playback reset, output restart or
hardware-acceleration change is installed.**

## Opt-in foreground diagnostics

`VLC_PLAYBACK_STABILITY_DEBUG=1` adds one GUI-thread heartbeat per second in
InputManager, disabled by default. It reads the held current input and copies
existing statistics only if a nonblocking stats trylock succeeds. It never waits
for a stats lock or queries/controls vout from the timer. Missing/disabled/locked
statistics invalidate delta comparisons. Pause, input release/media generation,
backward time and counter resets establish a new baseline.

Each line has wall time, input identity/generation, state, input clock, GUI
heartbeat interval, GUI gap flag, counters and deltas. Existing UpdateVout logs
already-held vout IDs without new vout lookups. Hover diagnostics also identify
child/GUI PIDs. Enabled diagnostics can add logging overhead; ordinary builds
have no heartbeat timer or new log writes.

Observations are evidence, not diagnoses:
- `audio-progress-video-static`: audio progresses while decoded/display counters
  plateau; investigate video decode/delivery, ES/demux state and timing.
- `decode-progress-no-display`: video decode progresses without display counters;
  investigate drops, vout scheduling/presentation and output logs.
- `display-counter-progress` with visibly frozen content: counters alone cannot
  detect a stale surface, static scene, driver/compositor or capture problem.
- `gui_gap=1`: heartbeat interval exceeded2.5s; investigate GUI/logging starvation.
  It does not locate the blocking operation. A fully hung UI cannot emit heartbeats.

Use VLC verbosity2 logs alongside these observations to identify actual decoder,
DXVA and vout modules/errors. Statistics are periodically aggregated and can lag;
a single plateau is not a confirmed freeze, and no counter is frame PTS.

## Popup correction: demonstrated geometry handoff defect

TimeTooltip::adjustPosition moved the native popup BEFORE resizing it. A shown Qt
widget regression reproduces a move notification at the new image position with
old timestamp-only dimensions; it fails on Stage C and passes with one setGeometry
transaction. Painter/path state is prepared before that transaction. This removes
a confirmed intermediate inconsistent geometry, without changing final dimensions.

Async/cache arrivals also reused the vertical target from the last mouse event,
even when the actual slider/toolbar had moved. TimeTooltip now retains a weak
slider reference and samples its top in Qt global logical coordinates for every
layout. Owner bounds are refreshed at the same point. Cursor X, existing margin,
monitor clipping and Stage2.4.4 fullscreen above-toolbar policy remain unchanged.
No guessed vertical offset, native-pixel/DPI mixing, new window flags or focus
operation is introduced. Worker arrivals, cache hits and hover updates all use
that same placement function. Opt-in logs now include popup move/resize events.

The handoff defect is confirmed in a host Qt test; it is NOT proven to be the sole
cause of the user's brief Windows jump. Physical windowed/fullscreen/mixed-DPI
confirmation is still required. Image removal during drag/leave/ineligible states
continues to restore the timestamp tooltip as before, rather than retain stale UI.

## Tests and build

Local Qt regression demonstrates the handoff failure before correction. Afterward,
atomic geometry, live-anchor movement without a mouse event,100rapid reversals,
window bounds and fullscreen placement pass. Existing exact/fast protocol, cache,
cancellation, generation, controller cleanup and click mapping tests remain.
Progress-observer tests cover audio-only progress, decode-without-display, display
progress, pause/resume, unavailable statistics, resets/media generation and GUI gaps.
Actual clean Windows build/native test results and package checksum will be
recorded after CI; these host tests are not physical-device acceptance.

## Windows comparison and log collection

Extract the NEW package into a new directory. Do not overlay Stage B/C. The inner
ZIP SHA must match PACKAGE_MANIFEST.json/SHA256SUMS. No libmpv DLL is bundled;
Setup-Runtime still downloads the same checksum-pinned private runtime and copies
both unchanged workers. Keep thumbnail-preview/ intact for the official LibVLC fallback.

From the extracted application's root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Setup-Runtime.ps1 -Destination C:\VLC-246D-Runtime1
# Normal quiet exact mode:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode exact -Runtime C:\VLC-246D-Runtime1 -Media 'C:\Videos\movie.mkv'
# Reproduce with foreground AND hover diagnostics (new log per launch):
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode exact -Runtime C:\VLC-246D-Runtime1 -Media 'C:\Videos\movie.mkv' -PlaybackDiagnostics -Diagnostics
Get-ChildItem "$env:TEMP\vlc246D-*.log" | Sort-Object LastWriteTime -Descending | Select-Object -First 3
Select-String -Path "$env:TEMP\vlc246D-*.log" -Pattern 'playback-stability|timeline-diagnostic|timeline-preview|direct3d|d3d11va|dxva|avcodec'
# Controls: same media/settings; close only the experimental window between runs:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode fast -Runtime C:\VLC-246D-Runtime1 -Media 'C:\Videos\movie.mkv' -PlaybackDiagnostics
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libvlc -Media 'C:\Videos\movie.mkv' -PlaybackDiagnostics
```

1. Begin with default hardware/output settings, the SAME movie and scene as Stage C.
   First play without hovering, then hover repeatedly across distant positions;
   test stationary hover,100rapid reversals, edges, pause/resume and normal seeks.
2. Repeat windowed/fullscreen and switching modes; test fullscreen toolbar auto-hide,
   different monitor origins/scaling, dragging and immediate leave/reenter. Check
   popup vertical position, focus, image labels, accuracy and continuous playback.
3. If frozen, keep recording briefly and note log wall time, whether UI/clock/audio
   advance, and whether a seek changes the displayed picture. Do not reset the
   renderer automatically. Save the COMPLETE log, not only filtered heartbeat lines.
4. Compare stock official VLC3.0.24 and the preserved Stage B/C packages with the
   same media/settings. Stock VLC has ordinary verbose logs only. Repeat with no
   hovering to separate preview activity from foreground issues. An intermittent
   non-recurrence in a short run is not a fix or proof of isolation.
5. Only as controlled diagnostic variables, try disabling hardware decoding OR
   selecting another video output via VLC preferences; change one variable at a
   time, record actual modules from logs and restore defaults afterward. Do not
   ship those preferences as a workaround. Check A/V sync and worker PIDs on exit.
6. Switch files during extraction; close VLC while a request runs. Check only this
   launch's worker PID exits. Do not kill all VLC/mpv processes globally.

Logs may contain media paths and ordinary VLC metadata; review before sharing.
The launcher uses --no-one-instance and child-scoped environment. Omitting both
switches returns to quiet operation; no persistent preference change is required.
Rollback: close only Stage D, launch the preserved Stage C package with its own
runtime/launcher; Stage B fast and LibVLC are also still selectable in Stage D.
No merge or replacement of the validated baseline occurs before user acceptance.
