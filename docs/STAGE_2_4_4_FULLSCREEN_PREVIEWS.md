# Stage 2.4.4: fullscreen popup placement and presentation

This branch starts from master `4f9aa938828a8e231c1944ace94d18b59c20d81e`
and merges the existing Stage 2.4.3 history at
`2f7cb820e893c8dc53e40cddbab9c57a2fe1a0fc`. Master contained CI but did not
contain the thumbnail implementation. Existing worktrees and packages are not
inputs to the new build and are preserved.

## Evidence and changes

The supplied Windows diagnostics prove that the fullscreen preview's 326x210
rectangle cannot fit into its 800x64 controller owner. Accepted hover events and
Qt visibility do not establish native popup visibility. Run 39797145725 completed
compilation, packaging, upload and independent checksum verification; its later
finalization failure is separate from the successful build.

SeekSlider identifies fullscreen by exact equality with MainInterface's existing
`getFullscreenControllerWidget()`, not by dimensions, window flags, the active
window or a class-name guess. The controller is created by MainInterface in
`main_interface.cpp`; its own InputControlsWidget contains the second SeekSlider.
The associated embedded video is managed through the same main interface, but
Windows video output may use a separate native fullscreen window:
`modules/video_output/win32/events.c` creates `hfswnd` with no parent, and
`common.c:CommonControlSetFullscreen` reparents the video into it and hides the
previous root. There is no new access to vout internals or playback state.

The explicit `PopupContext` distinguishes this controller from ordinary windows.
Windowed previews retain the existing owner/screen intersection and placement
algorithm. Fullscreen previews use the available geometry of the monitor at the
hover target, allow the image above the floating toolbar, and clamp horizontally.
Their bottom stays above the timeline; if the timeline is in a taskbar-reserved
strip, the image stays inside available geometry. No room above, oversized images
and invalid geometry still fall back to the timestamp. Both positive and negative
monitor origins are supported in Qt logical coordinates. Fullscreen timestamps
also use that available geometry; windowed timestamp behavior remains unchanged.

Native stacking/ownership is still a hypothesis. The scoped presentation change:

- Keeps the existing tooltip QWidget parent, window flags and manual lifetime.
- Associates its QWindow transient parent with the actual fullscreen controller.
- Uses WA_ShowWithoutActivating only for the fullscreen context.
- On Windows, refreshes only the visible fullscreen popup with SetWindowPos,
  HWND_TOPMOST and SWP_NOACTIVATE/NOOWNERZORDER/NOMOVE/NOSIZE/SHOWWINDOW.
  The existing tooltip already has WindowStaysOnTopHint. No other window is
  raised, activated or assigned an always-on-top policy.
- Restores the default transient association/attribute on a switch to windowed
  context. Existing hide/leave/drag/controller cancellation remains in place.

This corrects an unowned, Qt-only presentation path without assuming that a
QWidget parent change solves the problem. It is a defensible device-test change,
**not proof of the cause or of fullscreen visual success**. No extractor, cache,
process, seeking, focus activation, video output or fullscreen auto-hide code is
modified. In particular no SetForegroundWindow/SetActiveWindow call is added.

## Diagnostics and device acceptance

Retain `VLC_TIMELINE_HOVER_DEBUG=1`. Preview placement now logs `context=fullscreen`
or `windowed`. Fullscreen `stage=native-popup` reports the popup HWND, actual
native owner, foreground HWND, native visibility before/after, topmost style,
SetWindowPos success/error and native pixel rectangle. Existing logical rectangles
remain available for DPI comparisons. Logging stays opt-in and rate-limited.
Neither IsWindowVisible nor a successful native call establishes on-screen pixels.

Extract the new portable package into a new directory, keeping thumbnail-preview
intact. From its root in Windows PowerShell:

```powershell
$env:VLC_TIMELINE_HOVER_DEBUG = '1'
.\vlc.exe --no-one-instance --verbose=2 --file-logging --logfile="$PWD\stage244-hover.log"
```

Check timestamps alone (including audio-only media) and thumbnails for local
MP4/MKV in windowed and fullscreen modes. Test all timeline edges, a moved
fullscreen toolbar, multiple monitors including a negative origin, scaling/DPI,
taskbar configurations, rapid movement, dragging, media switches, controller
hide/show, fullscreen exit/re-entry, pause/play and shutdown. Confirm that focus,
keyboard input, audio, playback, seeking and auto-hide remain unchanged. Compare
native owner/foreground/rectangles if invisible; do not infer success from logs.
The existing burned-in PTS accuracy gate remains separate and unpassed.

## Validation and build delivery

Host Qt 5.15 offscreen tests exercise windowed bounds, the reported 800x64 toolbar,
monitor edges/origins, unavailable/oversized placement, exact context selection,
real tooltip image/timestamp fallback, unchanged flags/parent and transient owner
restoration. Existing hover mapping, cache, debounce, cancellation, lifecycle and
diagnostic tests are retained. One initial run encountered the existing tight
40-ms debounce test's scheduling assertion; a repeated run passed. Full Windows
Qt 5.6.3 compilation and visual device results must be reported separately.

The CI driver will pin a separate source commit for Stage 2.4.4 and retain Stage
2.4.3 as the default manual selection. Image, SDK, contrib hashes, isolated helper
runtime, disk threshold, cleanup allowlist and final ZIP/readback verification
remain unchanged. New artifacts/ZIPs must say Stage 2.4.4 and include run/attempt
identifiers. No Release publishing or automatic trigger is introduced.

Build source is pinned to `e879ce656c218eddec13926f03b5f0615d14dc61`; CI driver
selection is committed separately. Final local validation passed the host Qt
suite (including the fullscreen cases), standalone hover mapping, the 38 CI guard/
packaging tests under each stage selection, Bash syntax, ShellCheck and actionlint.
Build/device results will be recorded after the hosted attempt.

Additional target checks: the existing pinned GCC 6.4.0 compiler compiled
`timetooltip.cpp` and `thumbnailpreview.cpp` against the matching Qt 5.6.3 headers
with `-Wall -Wextra -Werror`, including the Windows-only code. `input_slider.cpp`
compiled with the established Qt build warning flags and prior generated config;
it retains the pre-existing enum/non-enum conditional warning in updateHover.
A trial with extra `-Werror` also rejected that existing warning; no suppression
or production warning-policy change was introduced. These were isolated object
checks under /tmp, not a package build and not native Windows execution.

Qt API/behavior verification: the pinned QtGui/qwindow.h declares
`setTransientParent`. Official Qt 5.6.3's
[`qwindowswindow.cpp`](https://github.com/qt/qtbase/blob/v5.6.3/src/plugins/platforms/windows/qwindowswindow.cpp)
implements transient ownership through GWL_HWNDPARENT, shows ToolTip windows
without activation, and implements raise() using HWND_TOP (not HWND_TOPMOST).
This supports the selected APIs and distinguishes the new presentation path;
it does not prove why the old popup was invisible on the tested device.
The 13 existing extraction-policy/accuracy-gate tests also passed unchanged.

## Files in the Stage 2.4.4 correction

- `modules/gui/qt/util/input_slider.cpp` and `.hpp`: select the actual fullscreen
  controller context before tooltip presentation.
- `modules/gui/qt/util/thumbnailpreview.cpp` and `.hpp`: shared context and bounds
  policy; controller/extraction/cache code is unchanged.
- `modules/gui/qt/util/timetooltip.cpp` and `.hpp`: fullscreen available geometry,
  transient/nonactivating presentation and native diagnostics.
- `test/modules/gui/qt/thumbnail/controller_test.cpp`: focused geometry, context
  and real-tooltip ownership/fallback regressions.
- `.github/workflows/windows-diagnostic-pilot.yml`,
  `ci/windows/build-diagnostic.sh`, `ci/windows/package-diagnostic.py` and
  `test/ci/windows-diagnostic-guards.sh`: optional, allowlisted stage selection and
  matching source/filename verification. Existing Stage 2.4.3 remains the default.
- `docs/WINDOWS_CI.md` and this report: operation and evidence.

The separate history merge also brings the previously validated implementation,
prototype fixtures/tests and reports into this master-based feature branch.

## Hosted build result

Run: https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39813320204

- Application source: `e879ce656c218eddec13926f03b5f0615d14dc61`.
- CI driver: `ec30a5f994e632a28a8dd984df4120d48d828f80`.
- Prepare/test/build/package: **success**, 16:22:02–16:37:47 UTC, 2026-10-22.
  This step includes the pinned Windows x64 compilation/link, Qt-plugin marker/PE
  and Wine registration checks, private helper build/smoke and ZIP verification.
- Upload, independent download and producer-manifest/ZIP checksum readback:
  **all success** (readback verification finished 16:38:04 UTC).
- Final resource/container reporting: **failure**, separately after readback.
  Evidence upload and job-summary steps succeeded. This is not an overall green
  workflow and not a compilation failure. No speculative CI-reporting fix is part
  of Stage 2.4.4. The cloud cannot retrieve the evidence/log downloads because
  their artifact redirect hosts return Forbidden; the exact reporting cause is
  not diagnosed here. Check the evidence artifact on GitHub.

Package artifact:
`vlc-stage244-win64-e879ce656c218eddec13926f03b5f0615d14dc61-39813320204-1`

[Download package artifact](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39813320204/artifacts/12441632578)

It contains `VLC-3.0.24-stage244-ci1-win64-e879ce656c21-39813320204-1.zip`,
`PACKAGE_MANIFEST.json` and `SHA256SUMS`. Verify the extracted delivery files before
unpacking the portable ZIP. GitHub reports the **outer Actions artifact** digest
`sha256:997bab5a1ae7a8327388984c8512fa91d12e2dd89bcdc7e96e6efcd53c86fe6f`;
this is not the inner portable ZIP's SHA-256. Its actual checksum is supplied in
SHA256SUMS and was verified by the independent hosted readback.

[Evidence artifact](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39813320204/artifacts/12442346442)
(`vlc-ci1-evidence-39813320204-1`). Artifacts retain the established 14-day policy.
Download locally with the existing authenticated GitHub CLI:

```sh
gh run download 39813320204 --repo Jau1Agiota/VLC-Timeline-Preview \
  --name vlc-stage244-win64-e879ce656c218eddec13926f03b5f0615d14dc61-39813320204-1 \
  --dir ./stage244-delivery-39813320204
cd ./stage244-delivery-39813320204
sha256sum --check SHA256SUMS
```

**Windows fullscreen visual acceptance remains pending.** Neither the host Qt
checks, target compilation, Wine smoke, native visibility APIs nor successful
artifact verification prove that the thumbnail/timestamp is visible above the
actual fullscreen video surface. Video-output modes, DPI, focus, controller
hide/show and multiple-monitor behavior require the device tests above.
