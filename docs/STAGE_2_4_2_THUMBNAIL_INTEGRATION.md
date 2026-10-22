# Stage 2.4.2 — native thumbnail integration

## Scope and architecture

Branch: `feature/stage-2.4.2-thumbnail-integration`. Based directly on validated `1f4a729cecdb56bc49e49212558271191fb5634c`. Implementation/build commit: `f37cb04f9c676d931da75e1cffc48999068a65e3`. A separate worktree at `/workspace/VLC-stage242` was used; `/workspace/VLC-Timeline-Preview` remains on the validated baseline. Stage 2.3 source, fixtures, previous results and runtime files were not replaced. No playback-core files changed.

The existing style-derived Stage 2.4.1 geometry remains the mapping source. `SeekSlider::updateHover` now drives a Qt-owned controller as well as the existing optional hover diagnostics. The current input URI/session, positive duration, seekability, actual video ES choices, mouse-button/drag state, visibility and application activation are checked. Audio-only media, network URLs, non-seekable media and unsupported inputs keep the ordinary timestamp tooltip. Requests beyond the helper's existing 24-hour range are rejected.

`TimelinePreview::Controller` (`util/thumbnailpreview.hpp/.cpp`) uses asynchronous **QProcess**, not a decoder on the GUI thread. It invokes the unchanged helper with:

```text
thumbnail-helper.exe LOCAL_FILE BUCKET_MS PRIVATE_NEW.bmp 5000
```

The helper resides at `APP/thumbnail-preview/thumbnail-helper.exe`, beside its **own matching libvlc/libvlccore DLLs and plugins**. Its working directory is that private runtime; `VLC_PLUGIN_PATH` is removed from its inherited environment. No foreground player handle, seek request, pause/volume action or decoder callback crosses this process boundary. Source/executable hashes record the reused Stage 2.3 implementation. No alternate decoding library or downloaded codec pack was added.

| Policy | Value/behavior |
| --- | --- |
| Hover debounce | 150 ms after the latest different bucket; movement inside a bucket does not restart it |
| Timestamp bucket | Floor to 250 ms, using current full-resolution duration and existing hover mapping |
| Queue/concurrency | One latest request per controller; weak global ownership cancels another timeline and waits for its child to be reaped before launching another |
| Stale rejection | Monotonic generation + request key + current media session + file fingerprint, rechecked on completion |
| Cache | Shared GUI-thread LRU, max 32 images and 8 MiB pixel data; SHA256 keys capped at 64 characters; normal container/key overhead is additional and bounded |
| Media identity | Input/session URI plus canonical local path, file size and modification time; key also includes bucket |
| Image bound | 320×180 ARGB32, max BMP size 230,454 bytes; dimensions checked before decoding the BMP |
| Process bounds | Existing helper watchdog 5 s; outer Qt timer 6.5 s including startup; stdout retained at most 4096 bytes, stderr drained/discarded |
| Windows containment | Private kill-on-close Job Object; 512 MiB process-memory limit; below-normal child priority; assignment failure cancels the request |
| Failure retry | At most a fresh mouse-driven retry of the same failed bucket after 2 s; no autonomous retry loop |
| Temporary data | One lazy private QTemporaryDir and one frame pathname per controller; remove after completion, cancellation/reap and normal teardown |

All control/cache/presentation callbacks run on Qt's GUI thread. The heavy extraction is exclusively in the child. Local file metadata queries and ingestion of the bounded BMP are still GUI-thread filesystem work, not a real-time IO guarantee. Direct UNC/network/removable source paths and non-local helper/temp locations are rejected before normal file inspection on Windows. File fingerprints are best effort: edits preserving size and mtime are not content-hashed.

The existing **TimeTooltip** paints the image above its timestamp/chapter text; there is no second competing popup or new playback control. Long preview chapter labels are elided. Image placement uses the intersection of the VLC owner window and the target monitor, with edge clamping and a gap above the timeline. If the image cannot fit, the original timestamp tooltip is restored. Mouse transparency/no-focus behavior is inherited from the existing tooltip.

The preview caption is `~HH:MM:SS.mmm`: the **approximate requested bucket**, not observed frame PTS. The Stage 2.3 helper still reports candidates with `frame_timestamp_verified=false`, including its explicitly reported near-end recovery. Neither exit zero nor a displayed thumbnail closes the accuracy gate. Quantization adds up to 249 ms relative to the raw hover position, on top of whatever extraction error exists.

## Lifecycle and failures

Leave/hide, any timeline press, dragging, window movement/resize/deactivation/closure, application deactivation, media changes and loss of seekability cancel pending work and clear the image. A 100 ms health check catches stationary-cursor input/file/state transitions. A paused seekable video remains eligible. Cached images survive ordinary leave/re-enter; ineligible/media transitions can clear the shared cache.

Cancellation invalidates generations immediately and kills only the owned child/job; a newer result cannot be replaced by an older completion. Starting-process races are also rejected in the started callback. No new child starts while a prior global extraction is still being reaped. Teardown disconnects process callbacks before members are destroyed, kills the private job/child and performs a teardown-only wait of up to 1000 ms before Qt's process destruction. Ordinary hovering/cancellation does not wait for decoding. Windows kill/job behavior still needs device testing.

Missing helper files, nonzero exits, malformed/truncated JSON, mismatched request/dimensions, oversized/invalid images, startup errors and timeouts show no preview and no modal error dialog. The original timestamp/chapter behavior continues. Failure diagnostics use the existing `VLC_TIMELINE_HOVER_DEBUG=1` switch and VLC verbosity 2; successful captures are not labeled timestamp-verified. Normal builds do not emit new hover/failure diagnostics without that switch.

Normal cleanup is tested. A hard application/OS crash can leave its small temporary directory/BMP; no global sweep deletes other sessions' data. The job closes with the parent once assigned; the helper's own watchdog is an additional safeguard. Filesystem stalls, OS termination failures, hardware-driver contention and actual Windows handle behavior are not proven by the Linux tests.

## Changed files

- `modules/gui/qt/util/input_slider.cpp/.hpp`: reuse the detector, validate current input, cancel on lifecycle events; existing click/drag/wheel seek paths remain in place.
- `modules/gui/qt/util/thumbnailpreview.cpp/.hpp`: process orchestration, generation checks, cache, limits and placement helper; no Q_OBJECT/moc requirement.
- `modules/gui/qt/util/timetooltip.cpp/.hpp`: optional image/caption and bounded placement. The header now depends on Qt/config directly so the real widget can be tested without starting VLC.
- `modules/gui/qt/Makefile.am`: compile/distribute controller sources.
- `extras/package/thumbnail-preview.sh`, root `Makefile.am`: compose an isolated new bundle, refuse existing/recursive outputs, check x64 binaries and plugin marker, preserve both inputs and record hashes. This script is outside contrib-rebuild-triggering `extras/package/win32`.
- `test/modules/gui/qt/thumbnail/*`, `test/Makefile.am`: actual Qt event-loop/subprocess/tooltip tests and optional real-helper protocol test; distributed with source.

## Actual verification

| Check | Result |
| --- | --- |
| Existing Stage 2.4.1 mapping suite | Passed |
| Qt offscreen controller/widget suite, host Qt 5.15.15 | Passed with C++11, `-Wall -Wextra -Werror`, production QString macros |
| Coverage | Hover/bucket mapping, debounce/latest request, kill/reap/stale rejection, shared LRU limits and reuse, missing/invalid helper, malformed output, timeout, oversized cache input, file changes, input invalidation, window deactivation, destruction cleanup, one global child, real tooltip painting/edge placement/long chapter/no-space fallback |
| Real unchanged Windows helper through Wine 11 adapter | 298×180 candidate at requested 14,750 ms; 2,755 ms wall time including debounce, 274 Qt heartbeat ticks; PTS unverified |
| New controller compiled against pinned Qt 5.6.3 / GCC 6.4.0 | Passed, including production QString macros |
| Configured Windows VLC build | Passed; initial lambda-capture error corrected, then all modified Qt source/header/moc dependencies explicitly forced to rebuild |
| Upstream debug portable packaging | `package-win32-debug-zip` passed, exit 0 |
| New package contains rebuilt Qt DLL | Main executable/core/Qt DLL hashes match the untouched new upstream package; helper hash matches the existing Stage 2.3 executable |
| Windows DLL-loader/plugin-cache probe under Wine | Passed under Wine 11: new Qt DLL registered (`Qt interface` descriptor in 6,180-byte probe cache); full bundle cache generation exit 0 |
| Packager protection checks | Existing output, recursive output and missing helper rejected with expected exit 2 |
| Native Windows GUI/playback/DPI/Job tests | **Not performed** |

The full VLC build retained its existing warning configuration; it reported a nonfatal enum/integer conditional warning at `input_slider.cpp:436` and upstream libtool/Makefile warnings. The standalone controller and automated tests passed strict warnings. Plugin cache generation also emitted a Wine desktop application-database update warning; plugin registration succeeded.

The real-helper test uses Linux Qt plus a test-only adapter executing the actual Windows helper under Wine. It validates asynchronous protocol/image handling, not a Windows Qt window or foreground playback. The fake helper is only a regression-test subprocess and is never packaged as the extraction helper.

### Build provenance and artifact

The exact pinned compiler and matched contrib archive were reused:

```text
Image: registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7
Compiler: x86_64-w64-mingw32-g++ 6.4.0; Qt 5.6.3 static
Contrib identity: 910cc9cada6080dd777fbafb2ebdd0c24f702c30
Contrib SHA256: 49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c
Helper SHA256: 19ce2b893adf1f5ca24da123d812c979fe003efa23fa37da6b18add5a6e61103
```

This cloud build used verified official OCI filesystem layers through PRoot rather than Docker's space-consuming vfs copy. PRoot 5.1 required a build-only seccomp adapter returning ENOSYS for statx/faccessat2 so libc could use the older syscalls it translates correctly; positive and negative file checks were preserved. The original `/bin/sh` and pinned compiler were used for the successful build. The host CA bundle was mounted for normal certificate validation. A getconf wrapper limited the wrapper's job count to the actual four-CPU quota; no VLC feature checks, diagnostics or source recipes were disabled. The existing verified contrib archive was supplied via a local file URL. Setup attempts, compatibility source and exact commands are retained in evidence. This is not a claim of bit-for-bit stock Docker reproducibility.

The optional pinned-binutils debug-stripping experiment crashed on a dependency. **Every main-runtime file in the new output was restored from the untouched successful debug package. Delivery is unstripped**, so it is large. The validated baseline and original Stage 2.3 runtime were never stripped or replaced.

- Original successful debug package: `/workspace/VLC-stage242/win64/vlc-3.0.24-win64-debug.zip`.
- Ready-to-test isolated bundle: `/workspace/artifacts/vlc-stage-2.4.2/vlc-3.0.24-stage242/`.
- Download archive: `/workspace/artifacts/vlc-stage-2.4.2/VLC-Stage-2.4.2-win64.zip`.
- Evidence/log directory: `/workspace/artifacts/vlc-stage-2.4.2/`.

`STAGE_2_4_2_BUILD.json` records implementation SHA, source-file hashes, compiler/Qt versions and binary hashes; `STAGE_2_4_2_SHA256SUMS` records both main and private runtimes. PE inspection verifies x64 and the Qt plugin's imports. The plugin's `timeline-preview:` marker and its source-package hash establish that the modified plugin is in this distribution. Later documentation commits do not change these implementation files. The final archive checksum is in the sibling `SHA256SUMS` file (kept outside the archive to avoid a self-referential hash).

## Reproduce on the validated WSL2/Docker host

Use a **new worktree**, not the validated worktree's `win64/`. Fetch this branch and add it with `git worktree add --detach NEW_SOURCE origin/feature/stage-2.4.2-thumbnail-integration`. Keep Git common metadata accessible read-only at its original absolute path inside Docker, and mount the new worktree at its original absolute path (as documented in Stage 2.4.1) so Git worktree references resolve. Reuse your validated UID/proxy/CA settings and the pinned image. Inside the new source worktree/container:

```sh
set -e
export VLC_CONTRIB_SHA="$(extras/ci/get-contrib-sha.sh win64)"
test "$VLC_CONTRIB_SHA" = 910cc9cada6080dd777fbafb2ebdd0c24f702c30
export VLC_PREBUILT_CONTRIBS_URL="https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-${VLC_CONTRIB_SHA}.tar.zst"
./extras/package/win32/build.sh -a x86_64 -p
make -C win64 package-win32-debug-zip
test -f win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll
grep -a -q 'timeline-preview:' win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll
x86_64-w64-mingw32-objdump -f win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll
unzip -t win64/vlc-3.0.24-win64-debug.zip
```

On the WSL host, compose a **new** output from that new app and your existing validated Stage 2.3 portable directory (which must contain the matching `thumbnail-helper.exe`, DLLs and plugins):

```sh
bash extras/package/thumbnail-preview.sh \
  "$PREVIEW_SOURCE/win64/vlc-3.0.24" "$VALIDATED_STAGE23_PORTABLE" "$NEW_PREVIEW_OUTPUT"
```

Do not copy only the Qt DLL into an installed VLC or replace the original helper/runtime. No helper source changes or rebuild are required. Preserve licenses/SDK files and record both sets of hashes. To run the focused tests on a Linux host with Qt5 development headers/tools and an offscreen platform plugin:

```sh
bash test/modules/gui/qt/run.sh /tmp/stage242-hover-tests-NEW
bash test/modules/gui/qt/thumbnail/run.sh /tmp/stage242-controller-tests-NEW
```

The second suite uses a **native host** fake helper; it does not need media or Wine. `CXX`, `MOC` and optional `QT_DEV_ROOT` select host tools/headers. Optional `REAL_PREVIEW_HELPER` and `REAL_PREVIEW_MEDIA` enable the real-protocol test through a native executable/adapter. Use a private Wine prefix for any Wine adapter. Never describe these headless tests as physical Windows GUI acceptance.

## Windows launch and manual acceptance (pending)

Extract the ready bundle to a **new** `C:\VLC-Stage242`, giving `C:\VLC-Stage242\vlc-3.0.24-stage242\vlc.exe`. In PowerShell:

```powershell
$App = 'C:\VLC-Stage242\vlc-3.0.24-stage242'
$Media = 'C:\VLC-Stage23\fixtures\ordinary.mp4'
$Logs = 'C:\VLC-Stage242\logs'
New-Item -ItemType Directory -Force $Logs | Out-Null
$Log = Join-Path $Logs ('preview-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
$env:VLC_TIMELINE_HOVER_DEBUG = '1'
Push-Location $App
& '.\vlc.exe' '--ignore-config' '--no-one-instance' `
  '--no-one-instance-when-started-from-file' '--intf=qt' '--verbose=2' `
  '--file-logging' "--logfile=$Log" $Media
Pop-Location
# After testing, disable diagnostic output for future launches in this shell.
# Remove-Item Env:\VLC_TIMELINE_HOVER_DEBUG -ErrorAction SilentlyContinue
```

In another PowerShell window, tail the printed logfile with `Get-Content ACTUAL_LOG_PATH -Wait | Select-String 'timeline-hover|timeline-preview'`. Failure messages use `timeline-preview`; the unchanged hover diagnostics provide position/duration/validity. No new diagnostics are expected when the switch is absent. Use Get-FileHash on the main Qt DLL and private helper to compare with the packaged manifest.

| Manual case | Required observation |
| --- | --- |
| Normal MP4/MKV, playing and paused | Hover without buttons; cold image eventually appears above the timeline with approximate bucket time; playback/audio/focus remain unaffected |
| Rapid movement | Old image clears as the bucket changes; only the latest stable request appears; no queue of helper processes |
| Cached point | Leave and re-enter a recently visited bucket; image reuses cache without another extraction |
| Click/drag/wheel/keyboard/chapter/accessibility | Original controls behave as baseline; dragging clears/cancels images and never adds an extra seek |
| Switch media/end/stop/non-seekable/audio-only | Old image disappears, old completion never appears on new media; ordinary timestamp tooltip remains where supported |
| Left/right edge, resize, DPI/monitors | Image stays in the owning VLC window and target monitor; no-space case restores ordinary tooltip |
| Missing helper | Close VLC, rename **only the new bundle's** private helper, start a fresh test process/unseen point: timestamp tooltip, no modal error/freeze; restore it afterwards |
| Failure/timeout | Corrupt/unsupported input does not crash/freeze the main player; automation covers deterministic child errors/timeout; inspect diagnostics and owned child lifetime |
| Shutdown during extraction | Exit VLC during a cold request; its helper disappears and session temp files are cleaned normally; no foreground/other VLC process is killed |
| No diagnostics switch | No new hover/failure logs; previews still work when the private runtime exists |

Use Process Explorer/Get-CimInstance to inspect helper PID/parent and counts, and listen/check foreground continuity while repeating the earlier isolation scenarios. A successful BMP/exit code is not PTS evidence; retain burned-in timecode review if measuring accuracy.

## Remaining limitations

- Native Windows popup rendering, Job containment/memory limits, cleanup, multiple-monitor/DPI behavior and uninterrupted playback have **not** been tested here. Wine plugin loading and Linux Qt rendering do not replace them.
- Cold process startup is seconds in the measured Wine setup. No persistent helper/prefetch engine is introduced; the small cache covers only recently visited buckets.
- The popup must fit above the timeline **inside its owner window**. In particular, a short floating fullscreen toolbar may fall back to timestamp-only rather than put an image outside that window. Fullscreen-native overlay support needs device validation/design before claiming it works.
- Initial support is ordinary local fixed-drive MP4/MKV, with the helper's existing timestamp limit. Network/live/removable media, unsupported codecs, very large decoders exceeding 512 MiB and slow extractions fall back. Multiple video tracks/ordered editions, transforms/subtitles/HDR and active-player options are not copied to the independent decoder.
- Approximate candidate timing remains experimentally limited. Do not claim the Stage 2.3 accuracy gate has passed, and do not widen recovery windows or trust player clocks to hide inaccurate candidates.

This is Stage 2.4.2 only. The PR is for review and Windows testing; no merge or next-stage work is automatic.
