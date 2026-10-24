# Stage 2.4.6E visual polish — Design 8

Base: `6c0adcea11a89485b5e55f00c196776260145d87` (Stage D).
Branch: `feature/stage-2.4.6E-thumbnail-visual-polish`.
Only TimeTooltip painting/layout and visual tests change in application source.
No worker, seek, cache, generation, cancellation, playback, hardware or media-switch
algorithm changes. The unresolved foreground freeze is on hold: Stage D's opt-in
playback diagnostics are preserved, with no new investigation or claimed fix.

## Visual specification

- Original image pixels/size/aspect are preserved: no crop, stretch, blur or extra
  image copy. A320x180 candidate occupies exactly320x180 logical pixels.
- No white frame, footer or thumbnail arrow. Four transparent pixels on either
  side and eight below contain the glow; the320x180 popup is328x188 overall.
- Lower image third fades from transparent black to black at alpha190/255.
  Upper image pixels are unchanged. This is an overlay, not altered extraction.
- Bottom-edge orange glow uses #FF8800 and #FFA32B with transparent falloff.
  A single warm bottom-edge highlight replaces a rectangular border. The glow
  stays within the fixed painting/placement bounds, with transparent outer edges.
- One centered white timestamp sits inside the image near the bottom, in the
  existing small Verdana font, bold, on a restrained translucent dark pill with a
  thin orange outline, following the approved screenshot. No separate opaque footer.
- Formats:00:29,12:45,1:10:52 (whole seconds, no fractional or internal labels).
  Audio-only/ineligible media retain VLC's original timestamp/chapter tooltip.

## Provenance and diagnostics

The visible label belongs to the DISPLAYED candidate's own requested timestamp.
When a newer hover is pending, the retained image keeps its prior label until a
replacement arrives. It never silently acquires the new hover's time. This applies
to exact, fast and LibVLC candidates. Formatting does not claim actual frame PTS:
those runtime semantics remain unchanged/unknown, not verified by a player clock.
Hover/request millisecond precision, generations and unknown actual PTS remain in
optional diagnostics. The new `style=glow-fade-bottom` log includes both hover and
image-request metadata; ordinary painting never displays hover/req/~ or IDs.

## Rendering, positioning and files

`modules/gui/qt/util/timetooltip.cpp/.hpp`: adjustPosition uses fixed image/glow
bounds, a compact internal timestamp rectangle and existing Stage D atomic
setGeometry/live timeline anchoring. previewTimestamp formats the image's own
request. paintEvent clears alpha and draws the unchanged image, two lightweight
QPainter gradients, one highlight and a small antialiased pill/text. The opaque
paint attribute is disabled so transparent edges do not retain stale background.
No QGraphicsEffect, GPU-specific path, offscreen image processing, timer, indexing
or disk effect is added. Existing window flags, transient ownership, fullscreen
policy, monitor clipping, nonactivation and mouse transparency are untouched.

`test/modules/gui/qt/thumbnail/controller_test.cpp` adds image-pixel, aspect,
alpha/borderless, gradient/glow, pill alignment, formatting, retained-image label
and rapid-hover assertions. run.sh saves a synthetic Qt-rendered preview for
review. Existing Stage D atomic geometry, live-anchor, fullscreen, dismissal,
media/cancellation/controller and diagnostic tests continue to run.
CI adds a separately pinned Stage E package and checks its visual marker, while
reusing the established toolchain/runtime/worker/native Qt and image regressions.
No existing build/package or master/baseline branch is overwritten.

## Validation

Local host Qt visual/geometry/controller tests pass, including100rapid label/hover
changes without relabeling a retained image. Pixel checks verify unchanged upper
video pixels, lower fade, orange glow, transparent margins, no white footer,
readable centered text and160x90/320x180 aspect preservation. Stage D positioning,
request lifecycle and diagnostic regressions pass. Qt5.6.3 moc accepts the header.
The synthetic render is a visual-property test, not physical Windows acceptance.
Actual Windows CI/package/native results and SHA will be recorded after the run.

## Install, select and test on Windows

Download the NEW Stage E artifact and extract its wrapper into a new folder.
Verify the inner ZIP, choose fresh install/runtime paths, and never overlay C/D:

```powershell
$m=Get-Content .\PACKAGE_MANIFEST.json -Raw | ConvertFrom-Json
$zip=Join-Path (Get-Location).Path $m.zip
if((Get-FileHash $zip -Algorithm SHA256).Hash.ToLowerInvariant() -ne $m.sha256){throw 'ZIP checksum mismatch'}
$install='C:\VLC-246E-Install1'
if(Test-Path $install){throw 'Choose a new folder'}
Expand-Archive $zip -DestinationPath $install
Set-Location (Join-Path $install $m.root)
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Setup-Runtime.ps1 -Destination C:\VLC-246E-Runtime1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -SeekMode exact -Runtime C:\VLC-246E-Runtime1 -Media 'C:\Videos\movie.mkv'
```

Use -SeekMode fast for the unchanged fast backend or -Backend libvlc for the
existing fallback. The unchanged Stage D launcher supports -Diagnostics and
-PlaybackDiagnostics; its optional logs still use `%TEMP%\vlc246D-*.log`.
Both flags remain opt-in, and the backend/default selector behavior is unchanged.
The checksum-pinned libmpv runtime remains a private download; no unapproved DLL
is bundled. Runtime setup copies the workers belonging to this package.

Check actual movie frames in bright/dark scenes, beginning/end and hour-long times.
Verify a single minimal timestamp, no white frame/footer, gentle orange glow and
smooth bottom fade. Rapidly reverse across distant targets: old images must retain
their own labels. Check stationary hover, cache hits, leave/reenter, drag/seek,
media switching, popup dismissal and normal playback/audio. Repeat windowed and
fullscreen, toolbar auto-hide, monitor edges and mixed DPI. Check vertical stability,
focus and full image aspect. A host render/CI smoke is not proof of device success.

Very small images may have insufficient room for the timestamp pill; the video is
never stretched to fit text. Qt5.6/mixed-DPI native composition and font substitution
remain device-test limitations. Existing Stage C accuracy limitations are unchanged.
Freeze investigation remains paused and no elimination is claimed. Omitting both
flags restores quiet operation. Rollback: close only Stage E and launch the preserved
Stage D/C folder using its own launcher/runtime. No merge is performed before user
acceptance; stop after this visual stage.
