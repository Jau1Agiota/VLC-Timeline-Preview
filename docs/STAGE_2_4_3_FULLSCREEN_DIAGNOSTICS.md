# Stage 2.4.3 — fullscreen thumbnail diagnostics

## Scope and findings

Branch: `investigate/stage-2.4.3-fullscreen`; baseline: `65c5b7655e0e6d203744faf48d58668e9182b91b`. Diagnostic implementation/build commit: `9e576f41a430f76652c6d5c369f3dfd9327a6fbc`. Worktree: `/workspace/VLC-stage243`.

**No fullscreen fix is included.** The extraction algorithm, eligibility predicates, seeking, cache, process/job isolation, cancellation, timeouts, tooltip flags, show/hide decisions and placement rules remain the baseline's. The active playback instance is not modified. The controller detaches its existing global observer at teardown before its diagnostic/process members are destroyed; the existing kill/reap cleanup is retained. Observations run on the existing GUI-thread paths; no new scheduled timers, decoding, file queries or process waits were added.

User-reported Windows observations: windowed previews work; fullscreen receives enter/move events, maps a 30,000 ms duration correctly and reports hover `valid=1`, active/seekable, without dragging. These were not independently reproduced on a Windows device here.

Confirmed by source inspection and headless tests:

- `thumbnailCurrent()` has additional predicates beyond hover `valid`: token identity, widget visibility/under-mouse, mouse buttons, video choices, popup and application state, input state, seekability and duration. Their original evaluation/return behavior is retained.
- `enterEvent()` shows the tooltip only if enabled, whole-second `inputLength > 0`, and no popup. Later `setTip()` and `setPreview()` do not show a hidden widget. The new tests explicitly preserve this behavior.
- `TimeTooltip::adjustPosition()` intersects the monitor with the owning-window bounds. When a preview will not fit, it discards the image and lays out the original timestamp tooltip. This fallback is preserved and tested.

Unresolved hypotheses, **not established root causes**:

1. Fullscreen's enter event may see a zero/stale `inputLength`, suppressing the only show call even though the live input duration used by hover mapping is valid later.
2. A fullscreen-only eligibility predicate such as application activation or `underMouse()` may reject extraction despite `valid=1`.
3. The toolbar's small owner rectangle may reject the image. This alone does **not** explain the missing ordinary timestamp tooltip.
4. Qt may consider the popup shown while its screen position/z-order leaves it obscured. Qt visibility/activation/rectangle logs do not prove native compositor visibility; device observations remain necessary.

## Diagnostic coverage

All new records require the exact environment value `VLC_TIMELINE_HOVER_DEBUG=1` **before launching VLC**, plus debug verbosity. The VLC logger prefix is `timeline-diagnostic`. Existing `timeline-preview:` failure messages remain. Ordinary builds without the switch stay quiet.

Every record carries `slider`, `tooltip`, `owner`, and `owner_class`; controller records also carry `controller`. Pointer IDs identify objects within one VLC run, not across runs. Outer IDs use `%p`; inner IDs are hexadecimal without a `0x` prefix or leading zero padding. Correlate by slider first; do not assume which slider was created first. Owner class/geometry and the windowed/fullscreen sequence distinguish the controller instances.

| Component/stage | Meaning |
| --- | --- |
| `slider / hover` | Existing position/span/timestamp/duration/validity fields plus `inputLength_s`, under-mouse, visibility, application state, mouse buttons, popup identity and tooltip/slider active/visible state |
| `slider / eligibility` | Actual boolean return (`accepted`) and all individually sampled rejection reasons; token match, video choice count, input state, live duration and seekability are included |
| `slider / enter-show` | Whether the unchanged enter-time show condition allows showing, with its inputs |
| `slider / leave-hide` | Outside-widget and slider/tooltip activation state for the existing leave-time hide decision |
| `controller / hover` | Request reached the controller; requested/bucket time and local-file identity validity |
| `controller / launch`, `spawn` | Debounce callback reached; pending/process/other-controller state, then an actual helper start request |
| `controller / finished` | Child exit/status, `latest` generation check, bucket, generation IDs and retained stdout size |
| `controller / cache-hit`, `failure`, `cancel` | Cached presentation, extraction failure or existing cancellation; no new retries or cancellation rules |
| `controller / activation` | Application/owner show/hide/activation/state observations, even before an eligible hover, using the controller's **existing** global event filter |
| `controller / window-event` | An existing owner/application event causing cancellation |
| `tooltip / show-call`, `event-visibility` | Explicit show call; actual Qt show/hide/activation/z-order/state events with before/after visibility, flags, active-window ID and rectangle |
| `tooltip / set-tip`, `set-preview` | Content updated, requested position/time and resulting visibility; neither method adds a show call |
| `tooltip / preview-placement` | Target, requested size, screen, owner bounds, their intersection, proposed rectangle and `fallback` |
| `tooltip / placement-final` | Final rectangle, target/screen/owner bounds, image presence and visible/hidden state; coordinates are Qt logical pixels |

All gates in `thumbnailCurrent()` are listed when it returns, including multiple simultaneous reasons. The diagnostic snapshot re-reads state after the decision; asynchronous input changes can make the reason snapshot differ from the just-computed return. `accepted` is the original decision, not recomputed from the log. If hover `valid=0` short-circuits the call, no new eligibility evaluation is forced.

Repeated identical records are suppressed per component/channel; continuously changing hover/layout/request samples are limited to four per second/channel. Eligibility and lifecycle transitions bypass the sampling interval but still suppress identical consecutive records in that channel. Absence of one sampled record is not proof a particular mouse event never ran. Collect a fresh launch and deliberate pauses; helper starts/completions include generation IDs. Logs do not include media paths/tokens or image pixels.

Changed sources: `input_slider.cpp/.hpp`, `thumbnailpreview.cpp/.hpp`, `timetooltip.cpp/.hpp`; new header `previewdiagnostics.hpp` (distributed through `modules/gui/qt/Makefile.am`). Focused tests extend `test/modules/gui/qt/thumbnail/controller_test.cpp`. No extraction-helper or playback-core source changed.

## Build and test evidence

- Existing hover mapping suite: passed.
- Actual host Qt 5.15.15 event-loop/controller/tooltip regression suite: passed with C++11, `-Wall -Wextra -Werror` and production QString macros. Added checks cover exact opt-in/default silence, duplicate/rate suppression, request stages, cancellation/activation records, actual show/hide events, hidden content updates and unchanged placement fallback.
- Pinned Windows x64 GCC 6.4.0 / Qt 5.6.3: all nine objects depending on the changed headers, including both affected moc objects, compiled and the Qt plugin linked successfully. The baseline nonfatal enum/integer conditional warning and libtool static-library warnings remain; no diagnostic was disabled.
- Final diagnostic DLL loaded and registered under Wine 11: `Qt interface` descriptor in a 6,184-byte cache; exit 0. A new disposable prefix was used and removed after its private wineserver stopped. This is not Windows GUI/fullscreen validation.
- New portable archive: all members passed CRC/SHA256 comparison against their input bytes; the new Qt DLL and unchanged application/core/helper hashes match the manifest. Preserved baseline DLL and both previous ZIP hashes match their pre-build values. Details: `package-tests.log`.
- **Native Windows GUI/fullscreen/playback tests were not performed.** The host tests do not instantiate the real VLC input manager or establish the Windows eligibility failure. No frame-accuracy gate is claimed.

Only a **new** build directory (`/workspace/artifacts/vlc-stage-2.4.3/build/modules`) was written. The matching Stage 2.4.2 generated Makefile flags, config, contrib and core import library were read; Qt objects were copied (not hardlinked to the baseline), nine dependency-identified objects/moc files were rebuilt against the new sources, then the private copy of the plugin was relinked. Compiler commands and dependent target list are in evidence. This is an incremental plugin build, **not a fresh full VLC rebuild**. It uses the same verified PRoot/compiler environment documented in Stage 2.4.2. The existing `win64/` directories and Stage 2.4.2 artifacts were not cleaned, configured or rebuilt.

The new archive contains the complete existing portable application/private helper runtime with **only its main Qt DLL replaced**, old main plugin cache omitted so VLC can rescan, and Stage 2.4.2 metadata moved into `baseline-provenance/`. Private helper files/cache are unchanged. New manifest/report/checksums describe the diagnostic package. Licenses and attribution are retained. Main `vlc.exe`/core version metadata remain from the baseline; identify this build by the **new Qt DLL hash**, manifest and diagnostic marker, not the main executable's version string. It remains unstripped and large.

Artifacts:

- `/workspace/artifacts/vlc-stage-2.4.3/VLC-Stage-2.4.3-fullscreen-diagnostic-win64.zip`
- `/workspace/artifacts/vlc-stage-2.4.3/VLC-Stage-2.4.3-evidence.zip`
- `/workspace/artifacts/vlc-stage-2.4.3/SHA256SUMS` (external archive checksums)
- `/workspace/artifacts/vlc-stage-2.4.3/build-manifest.json` (source, binary and baseline hashes)

For host regression reproduction, use a new output directory:

```sh
bash test/modules/gui/qt/run.sh /tmp/stage243-hover-NEW
bash test/modules/gui/qt/thumbnail/run.sh /tmp/stage243-controller-NEW
```

Qt5 development tools/libraries and an offscreen plugin are required; optional `QT_DEV_ROOT` and `MOC` select matching extracted host tools as documented in Stage 2.4.2. Evidence includes `build-commands.sh`, the copied Makefile/diagnostic command template and root/compiler invocation for the incremental build. For an independent WSL/Docker rebuild, use a fresh worktree/build directory and the pinned image/build instructions in `STAGE_2_4_2_THUMBNAIL_INTEGRATION.md`; select this investigation branch. Never run make/configure/clean in the validated worktree to test this stage.

## Windows launch and collection

Extract the **new** archive into `C:\VLC-Stage243`; do not extract over Stage 2.4.2. In PowerShell (adjust the fixture path):

```powershell
$App = 'C:\VLC-Stage243\vlc-3.0.24-stage243-diagnostic'
$Media = 'C:\VLC-Stage23\fixtures\ordinary.mp4'
$Logs = 'C:\VLC-Stage243\logs'
New-Item -ItemType Directory -Force $Logs | Out-Null
$Log = Join-Path $Logs ('fullscreen-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
$env:VLC_TIMELINE_HOVER_DEBUG = '1'
Write-Host "Collecting: $Log"
Get-FileHash "$App\plugins\gui\libqt_plugin.dll" -Algorithm SHA256
Push-Location $App
& '.\vlc.exe' '--ignore-config' '--no-one-instance' `
    '--no-one-instance-when-started-from-file' '--intf=qt' '--verbose=2' `
    '--file-logging' "--logfile=$Log" $Media
Pop-Location
```

In another PowerShell window, substitute the printed path:

```powershell
Get-Content 'C:\VLC-Stage243\logs\ACTUAL_LOG_FILENAME.log' -Wait |
    Select-String 'timeline-diagnostic|timeline-preview'
```

Collect from a fresh launch: hover windowed for several seconds; move away; enter fullscreen; reveal its controller; hover a stable timeline point for at least 8 seconds; move between a few points and leave/re-enter; return to windowed and repeat; exit VLC. Preserve the **complete logfile** including startup, not only filtered lines, and note when fullscreen/windowed transitions and visible/absent tooltips occurred. Collect ordinary MP4 first; then MKV if useful. Avoid menus/other applications initially; deliberately test deactivation separately. Close only this diagnostic instance. To disable diagnostics for subsequent launches:

```powershell
Remove-Item Env:\VLC_TIMELINE_HOVER_DEBUG -ErrorAction SilentlyContinue
```

Interpret by the fullscreen slider ID:

1. Check hover `valid`, `inputLength_s` and `enter-show allowed`. `allowed=0` with `inputLength_s=0`, followed by set-tip/preview records that remain hidden, supports the enter-time visibility hypothesis.
2. Check `eligibility accepted=0 reasons=...`. Hover `valid=1` alone does not authorize extraction. `app_state=4` is Qt ApplicationActive; buttons zero and popup ID zero mean no pressed buttons/popup.
3. Check controller hover → launch/spawn → finished, **or cache-hit**. No spawn is expected on a cache hit. Cancel/window events or `latest=0` explain discarded work; failure records identify helper issues.
4. Check `preview-placement fallback=1` and compare owner/intersection height against requested size. Then inspect final timestamp-only geometry; a placement fallback does not itself establish why the tooltip is invisible.
5. Check show/hide/activation sequence, `visible_after`, hidden state, flags and final screen rectangle. If Qt reports a shown, correctly placed tooltip but none is visible, record the visible desktop/controller state for native z-order investigation; do not infer an extraction failure from that alone.

Stop here. Review Windows diagnostic evidence before changing fullscreen eligibility, show/hide behavior, window flags or placement.
