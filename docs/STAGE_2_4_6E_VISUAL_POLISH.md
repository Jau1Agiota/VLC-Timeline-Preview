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
Completed Windows CI/package/native results and SHA are recorded below.

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
The preserved launcher may still identify its diagnostic status as Stage D; the
Stage E archive/root and BUILD_MANIFEST identify this visual build.
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

## Completed Windows delivery

[CI40093906672](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40093906672)
passed all applicable jobs: clean Windows cross-build/package, independent ZIP
upload/download checksum readback, native worker/Qt regressions and image analysis.
Application source: `e577355ed22aea9d2703aad794f79a68f0a1ce42`.
CI driver: `3317089613c48f87b9a8f5154b8b18e90486d829`.
Subsequent documentation/evidence commits do not change the shipped application.

[Download the new portable Windows artifact](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40093906672/artifacts/12560288217).
Inner ZIP: `VLC-3.0.24-stage246E-ci1-win64-e577355ed22a-40093906672-1.zip`
(512485450bytes). SHA-256:

```text
7b3cc632fec2c6c11714cfd81f1e502f48c309153affc4abc4c177b26128c332
```

This checksum is for the INNER application ZIP, not GitHub's artifact wrapper.
Both are new/versioned; older packages remain intact. Artifacts require GitHub
access and expire after14days; no Release was created or changed.

[Download the small synthetic Qt render](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40093906672/artifacts/12560283261)
for a lightweight preview of the borderless/fade/glow/pill treatment. It uses an
owned flat-color test image and host offscreen Qt, not the user's movie or a
physical Windows screenshot. The same source's pixel/geometry assertions passed
in CI. Font substitution/native DPI/composition still require device review.

- Stage E safety/packaging guards:46tests,45pass/1skip. Preserved Stage D control:
  46tests,44pass/2skip (the additional Stage E-specific check skips).
- Host Qt tests include all Stage D cases plus the Stage E pixel, aspect, alpha,
  gradient/glow, time formatting/alignment, retained-image and100rapid-hover checks.
- Strict final tooltip syntax check against actual GCC6.4.0/Qt5.6.3 and Qt5.6.3 moc
  passed. Bash syntax, ShellCheck0.10.0, actionlint1.7.7 and whitespace checks pass.
- Native Windows Qt/dummy A/V smoke exited0 with diagnostics OFF and ON:0heartbeats
  OFF,30heartbeats/30available statistics samples and positive video counters ON.
  This checks diagnostic preservation; it is not new freeze investigation or a fix.
- Existing fast/exact/LibVLC worker protocol, repeated/rapid requests, shutdown and
  image-analysis collection passed. Extraction code and Stage C's known accuracy
  limitations are unchanged; no universal accuracy/performance claim is added.

[Validation excerpts and artifact digests](evidence/stage_2_4_6E_ci.json) retain the
API/Checks evidence. Complete native headers, BMPs, logs and image analysis are
in the CI artifacts. Existing B/C/D notice titles come from reused harnesses; the
package manifest identifies Stage E. No private recording/media or runtime DLL
was committed to Git. Physical Windows hover/fullscreen/mixed-DPI appearance,
smoothness and playback acceptance remain pending. PR19 is draft and unmerged.
