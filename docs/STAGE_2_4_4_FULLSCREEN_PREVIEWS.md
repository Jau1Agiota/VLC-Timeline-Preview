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
