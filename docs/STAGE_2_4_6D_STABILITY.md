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

A specific upstream presentation risk is visible in source, but not confirmed in
this incident: direct3d11.c:1320 calls Present1, and DEVICE_REMOVED/DEVICE_RESET log
`SwapChain Present failed` beside a `TODO device lost` rather than recreating the
output there. Its optional D3D11.4 render-fence path also has an infinite wait;
the prior pinned build config enables HAVE_D3D11_4_H, but the recording does not
establish which runtime device/context path was active. Direct3D9 logs
`Failed IDirect3DDevice9_Present`. These signatures should be checked in the FULL
log. No driver failure is inferred from the screen recording alone.

video_output.c:1180 increments its displayed counter AFTER the output callback
returns, even if that callback logged a Present error. Therefore rising displayed
counters cannot exonerate the renderer/driver or verify changing pixels. Paired
counter progress and native output logs are needed before choosing a correction.

There is no reproducible RTX3060/Windows GUI environment or original movie in the
cloud. Reproduction in stock VLC3.0.24/Stage B is pending the device matrix below.
**No freeze fix is claimed. No forced seek, playback reset, output restart or
hardware-acceleration change is installed.**

## Opt-in foreground diagnostics

`VLC_PLAYBACK_STABILITY_DEBUG=1` adds one GUI-thread heartbeat per second in
InputManager, disabled by default. It reads the held current input and copies
existing statistics only if nonblocking item and stats trylocks succeed in the existing lock order. It never waits
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
Qt's official [v5.6.3 Windows platform implementation](https://github.com/qt/qtbase/blob/v5.6.3/src/plugins/platforms/windows/qwindowswindow.cpp#L1492)
routes a geometry change through MoveWindow with position and dimensions together.
That supports this minimal transaction change; it is not a substitute for device
verification of native composition.

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

## Changed files and functions

| File | Targeted change |
|---|---|
| `modules/gui/qt/util/timetooltip.cpp/.hpp` | adjustPosition uses live timeline/owner geometry and one setGeometry; setPlacementContext holds a weak slider reference; event observes move/resize only when tracing. |
| `modules/gui/qt/util/input_slider.cpp` | updateTooltipContext passes the actual slider; mouse mapping/click/drag logic is unchanged. |
| `modules/gui/qt/input_manager.cpp/.hpp` | constructor creates an opt-in timer; TracePlayback reads snapshots; delInput resets sample generation; UpdateVout logs its existing held list. |
| `modules/gui/qt/util/playbackdiagnostics.hpp` | Progress::observe/reset classify counter observations without controlling playback. |
| `modules/gui/qt/util/thumbnailpreview.cpp` | started callback logs child/GUI PIDs only. |
| `modules/gui/qt/Makefile.am` | distribute the new header; no new dependency. |
| `prototypes/stage_2_4_6D/Start-Preview.ps1` | child-scoped PlaybackDiagnostics switch and a new per-launch log prefix. |
| CI workflow/build/packaging guards | separately pinned Stage D source/package, diagnostic marker/launcher checks and native Qt on/off smoke. |

## Tests and build

Local Qt regression demonstrates the handoff failure before correction. Afterward,
atomic geometry, live-anchor movement without a mouse event,100rapid reversals,
window bounds and fullscreen placement pass. Existing exact/fast protocol, cache,
cancellation, generation, controller cleanup and click mapping tests remain.
Progress-observer tests cover audio-only progress, decode-without-display, display
progress, pause/resume, unavailable statistics, resets/media generation and GUI gaps.
Local GCC6.4.0 strict syntax checks pass for InputManager, TimeTooltip and
Controller using matching Qt5.6.3 headers; Qt5.6.3 moc processes the changed headers.
The slider strict standalone check encounters an existing StageC enum-conversion
warning under additional -Werror; full CI uses the unchanged VLC warning flags.
Completed clean Windows build/native results and the package checksum are recorded
below; these host tests are not physical-device acceptance.

## Windows comparison and log collection

Extract the NEW package into a new directory. Do not overlay Stage B/C. The inner
ZIP SHA must match PACKAGE_MANIFEST.json/SHA256SUMS. No libmpv DLL is bundled;
Setup-Runtime still downloads the same checksum-pinned private runtime and copies
both unchanged workers. Keep thumbnail-preview/ intact for the official LibVLC fallback.

After extracting the GitHub artifact wrapper into a NEW download folder, verify
and extract the INNER ZIP from that folder (the final SHA is recorded below):

```powershell
$m=Get-Content .\PACKAGE_MANIFEST.json -Raw | ConvertFrom-Json
$zip=Join-Path (Get-Location).Path $m.zip
if((Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.sha256){throw 'ZIP SHA-256 mismatch'}
$install='C:\VLC-246D-Install1'
if(Test-Path -LiteralPath $install){throw 'Choose a NEW installation folder'}
Expand-Archive -LiteralPath $zip -DestinationPath $install
Set-Location (Join-Path $install $m.root)
```

From the extracted application's root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Setup-Runtime.ps1 -Destination C:\VLC-246D-Runtime1
# Normal quiet exact mode:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode exact -Runtime C:\VLC-246D-Runtime1 -Media 'C:\Videos\movie.mkv'
# Reproduce with foreground AND hover diagnostics (new log per launch):
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode exact -Runtime C:\VLC-246D-Runtime1 -Media 'C:\Videos\movie.mkv' -PlaybackDiagnostics -Diagnostics
$latest=Get-ChildItem "$env:TEMP\vlc246D-*.log" | Sort-Object LastWriteTime -Descending | Select-Object -First 1
$latest.FullName
Select-String -LiteralPath $latest.FullName -Pattern 'playback-stability|timeline-diagnostic|timeline-preview|direct3d|d3d11va|dxva|avcodec'
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

## Completed build and verification

[Windows CI run40036973045](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40036973045)
completed successfully. The clean cross-build/package/readback job took16m37s.
Application source is pinned independently to
`12b25af9272193ddfd0c298e1e9f48c5ff5b66e4`; the CI driver is
`d1a45d5faaeccb172724ddfe666ef7df99c526aa`. Later changes add tests/docs/evidence,
not different application behavior. The first Stage D attempt40036441685 was
canceled to include the observer's item/statistics lock-order correction before
final delivery; it is not the delivered package. Previous builds remain intact.

[Download the new portable package](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40036973045/artifacts/12538595771).
GitHub authentication may be required. Artifact retention is14days. This is a new
Actions artifact, not a Release or an inaccessible cloud-workspace link.

Inner ZIP: `VLC-3.0.24-stage246D-ci1-win64-12b25af92721-40036973045-1.zip`
(512479288bytes). SHA-256:

```text
96a30502d4c22190f02093352eb0c8b2ef55e61f88163a1863071ffe3d6859af
```

The GitHub artifact wrapper has a DIFFERENT digest. After downloading/extracting
the wrapper, verify this INNER ZIP before the PowerShell installation steps above.
The fresh package includes both unchanged worker algorithms, the separate official
LibVLC runtime, the Stage D launcher and modified Qt plugin. No libmpv DLL is bundled.

| Verification | Actual result and limit |
|---|---|
| Cross-build and linking | Pinned VideoLAN image, GCC6.4.0, Qt5.6.3 and matching prebuilt contrib passed. No fallback dependency builds or relaxed safety gates. |
| Package | ZIP integrity, PE executables, required scripts/licenses, new diagnostic/exact markers and independent upload/download SHA verification passed. |
| Windows Qt/dummy-output launch | Exit0 with diagnostics OFF and ON. OFF:0heartbeats/0diagnostic statistics samples. ON:30heartbeats/30available stats samples, positive decoded-video counters. This proves the modified plugin executes, not RTX3060 rendering or popup acceptance. |
| Existing worker regression | Fast/exact/LibVLC requests, rapid latest-pending scheduling and process cleanup completed without reported failures. |
| Existing image analysis |423plausible fixture OCR observations:211fast,166exact,46LibVLC. All148exact non-offset images within100ms provisionally; the known +5s-origin fixture still fails17of18. No new accuracy guarantee or frame-PTS confirmation is asserted. |
| Host Qt regression | Before correction, image handoff fails at the observed old-size/new-position assertion. After correction, both image/text handoffs, live anchor,100reversals, edge/fullscreen geometry and existing lifecycle/generation/cache/protocol tests pass. |
| Diagnostic progress tests | Pause/resume, unavailable stats, media/counter/time reset, audio-only progress, decode/display progress and GUI gaps pass. |
| CI guard tests | Stage D:45tests,44pass/1skip. Stage C control:45tests,42pass/3skip (Stage D-specific guards skip). |
| Static/tool checks | Bash syntax, ShellCheck0.10.0, actionlint1.7.7, Python analyzer tests and git whitespace checks pass. Qt5.6.3 moc/strict GCC checks are described above. |

The native Qt test runs minimized with dummy A/V outputs, no mouse automation and
no physical GPU/display acceptance. It does not reproduce the original movie's
hardware freeze. Normal clicking/dragging/keyboard/media switching/fullscreen and
A/V sync on the user's Windows PC remain manual checks, even though relevant
source paths are preserved and host/controller tests pass. Do not infer a permanent
freeze fix from a short successful run.

[Machine-readable validation excerpts](../prototypes/stage_2_4_6D/evidence/ci-validation.json)
record API/Checks notices and artifact digests. Full logs, worker headers, BMPs and
Qt smoke logs are in the run's evidence/native/image artifacts. Some notice titles
retain Stage B/C names because those proven test harnesses are reused; the package
manifest identifies the actual Stage D source. No private screen recordings or
movie frames are committed to Git.

**Disposition:** targeted popup geometry defects corrected and tested; their
relationship to the physical transient jump still needs confirmation. Foreground
freeze remains unresolved, with a validated opt-in diagnostic build for the next
reproduction. The Stage C exact engine and all selectors are preserved. Draft PR18
targets Stage C; no merge, promotion or follow-on integration is performed here.
