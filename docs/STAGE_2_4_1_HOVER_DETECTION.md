# Stage 2.4.1 — native timeline hover diagnostics

## Implementation and scope

Branch: `feature/stage-2.4.1-timeline-hover`. Base: `prototype/stage-2.3-libvlc-extraction` at `023a9a952ad1411835ad0a88c23f711ae2adba6a` (includes merged near-end investigation). Target remains VLC 3.0.24 / Windows x64. This stage adds instrumentation only; the extraction helper, fixtures, measurements and Stage 2.3 runtime are unchanged.

`modules/gui/qt/components/controller.cpp`, INPUT_SLIDER, constructs horizontal **SeekSlider** instances and connects `positionUpdated` / `inputCanSeek` to them. Existing `sliderDragged` goes to `InputManager::sliderUpdate`. No connections or playback-control paths were changed.

Changed files:

| File | Purpose |
| --- | --- |
| `modules/gui/qt/util/input_slider.cpp` | Opt-in logging on existing move, enter, leave and hide handlers; read-only input queries and style geometry |
| `modules/gui/qt/util/input_slider.hpp` | Private diagnostic method and constructor-time switch |
| `modules/gui/qt/util/seekhover.hpp` | Pure integer coordinate-to-milliseconds mapping |
| `modules/gui/qt/Makefile.am` | Distribute the new header with the Qt sources |
| `test/modules/gui/qt/seekhover.cpp` | Focused mapping tests, independent of playback/Qt initialization |
| `test/modules/gui/qt/run.sh` | Standalone host test and optional MinGW test compilation |
| `test/Makefile.am` | Add `test_modules_gui_seekhover` to the existing check suite |
| This report | Build evidence and native testing instructions |

**Runtime switch:** set `VLC_TIMELINE_HOVER_DEBUG=1` before starting a new VLC process. Each slider reads it at construction. Other values/unset leave this instrumentation quiet, even with normal VLC debug logging enabled. With the switch enabled, `msg_Dbg` emits `timeline-hover`; use verbosity 2 and the existing file logger to collect it. No new VLC option or dependency is added.

The new method executes before the original handler logic and never accepts/rejects events, calls `setValue`, emits seek signals, invokes a helper or changes input variables. Existing click, drag, wheel, keyboard, chapter, tooltip and accessibility behavior is left in place. Drag events are logged with `dragging=1`, `valid=0`; any seeking during that drag belongs to the existing implementation. A release recovered by the original move handler can leave the diagnostic's pre-handler `dragging` flag true for that one event.

## Timestamp and geometry contract

- All coordinates are widget-local **Qt logical pixels**, using event position or `mapFromGlobal(QCursor::pos())` for lifecycle events. There is no physical-pixel or global-screen-width calculation.
- A temporary `QStyleOptionSlider` is positioned at the slider minimum and maximum, and `SC_SliderHandle` rectangles are queried with the **widget supplied**. Their horizontal centers determine `start_x`, `end_x` and travel `span`. This accounts for style offsets, margins, handle size and resize/DPI geometry. Only the temporary option changes; the actual slider value never changes.
- The custom SeekStyle paints left-to-right; its query uses that direction. Classic QSlider keeps the direction from `initStyleOption`, including reversed style endpoints. The legacy click mapper explicitly passes `false` for reversal and caches a handle length obtained without the widget argument. It is **not reused or fixed**: diagnostics follow rendered style geometry, and native device checks must assess any small rounding difference or pre-existing RTL mismatch with legacy click/tooltip mapping.
- The hover region is the horizontal handle-center travel interval over the **full slider height**, including its chapter strip. It excludes horizontal end margins and points above/below the widget. The helper rejects invalid/zero-span/out-of-widget endpoint geometry and non-horizontal sliders.
- The current input's `length` is read through `MainInputManager::getInput()` and `var_GetInteger`, then converted with the verified VLC 3.0.24 `MS_FROM_VLC_TICK` macro. The existing `positionUpdated` signal truncates duration to whole seconds (`input_manager.cpp::UpdatePosition`); it is unchanged. Reading current length avoids using stale or whole-second cached duration for this measurement.
- Mapping is `floor(duration_ms * offset / span)`, with offset clamped to the endpoint interval and reversed when appropriate. Quotient/remainder arithmetic prevents multiplication overflow even for INT64_MAX duration. Fractional microseconds are truncated to milliseconds. This is the requested timeline coordinate, **not decoded frame PTS**.
- `inside` describes geometry only. `valid=1` additionally requires positive duration, enabled/seekable slider, current `can-seek`, PLAYING or PAUSED input, no drag and no active popup. Leave/hide always log `valid=0`, even if a tooltip/popup causes leave while the cursor is geometrically inside. No input, stopped/ended/opening/error state, unknown duration and non-seekable input are ineligible.
- Unknown duration/invalid geometry yields `timestamp_ms=-1`. Outside the horizontal region, known-duration timestamps are clamped to 0 or duration but `inside=0`, `valid=0`; these are not eligible hover samples. An inactive input may still have a geometrically calculated timestamp: always honor `valid`.

The input pointer is read and used only on the GUI thread, where MainInputManager owns the current reference, as in existing Qt dialogs. No core callbacks or worker threads are added. The three input-variable reads are not an atomic playback snapshot; a transition can make a single diagnostic stale. The next mouse event reads current state again. No persistent hover state is cached.

## Actual build and test results

| Check | Actual result |
| --- | --- |
| Standalone production mapping tests, host C++11 | Passed with `-Wall -Wextra -Werror -pedantic` |
| Same tests under host undefined-behavior sanitizer | Passed, including INT64_MAX duration/extreme coordinates; assertions stay active with NDEBUG |
| Same tests cross-compiled with pinned MinGW GCC 6.4.0 | Compiled/linked successfully as Windows x64 **mapping-test executable** |
| Windows mapping-test executable under Wine 11 | Passed; not Windows GUI/device testing |
| Shell syntax / patch whitespace | Passed |
| Configured Automake check target | Added but unrun: no configured build tree is present |
| Modified VLC Windows application build | **Blocked before compilation**, Docker exit 125; no application/package generated |
| Windows desktop, fullscreen, DPI, playback/accessibility checks | Unrun |

Tests cover exact endpoints, midpoint with fractional-second duration, end margins, Y boundaries, style offsets, resizing/odd spans, reversed endpoints, missing duration, invalid geometry, non-horizontal orientation, clamping, monotonicity, and overflow limits. They do not instantiate SeekSlider or verify native styles, input-state delivery, logger behavior or playback isolation.

The pinned image is:

```text
registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7
```

Actual build attempt after the source change:

```sh
docker run --rm --pull=never --network host --cpuset-cpus=0-3 --memory=16g \
  -v /workspace/VLC-Timeline-Preview:/vlc -w /vlc \
  registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7 \
  bash -lc './extras/package/win32/build.sh -a x86_64 -p && make -C win64 package-win32-debug-zip'
```

Error: `docker: Error response from daemon: No such image: registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7` (exit **125**). No compiler/configure step ran inside that container. There is no current `win64/` or installed contrib tree in this cloud checkout. Only extracted compiler files and downloaded archives survive earlier work; those are not a complete configured application environment.

`docker info` still reports **vfs**, with zero installed images/containers. The cloud workspace has a 32 GiB filesystem, about 19 GiB free. The earlier documented full-image launch exhausted storage on this same setup (see WINDOWS_BUILD.md). This run deliberately used `--pull=never` rather than repeating that full-copy storage failure or removing any existing artifacts. **The current observed error is missing image, not a newly measured out-of-space error.** The local WSL2/Docker Desktop baseline is not accessible from this cloud session.

An additional unconfigured syntax probe with pinned GCC 6.4.0 and official contrib Qt **5.6.3** headers exited 1 in existing `include/vlc_threads.h:90`, `poll was not declared in this scope`. It lacks generated configure/fixup definitions and is **not accepted as a successful Qt translation-unit build**. No hand-written config header or unrelated toolchain was substituted. The modified Qt plugin still requires compilation in the validated environment.

Cloud artifact directory: `/workspace/artifacts/vlc-stage-2.4.1-hover/`. It contains `windows-build.log`, `windows-build.exit`, mapping test logs/binaries, and syntax-probe diagnostics. **There is no Stage 2.4.1 `vlc.exe`, Qt plugin DLL or portable Windows test application to deliver.** The mapping test executable is not a replacement application, and an existing official/Stage 2.3 runtime cannot demonstrate this patch.

## Build locally without overwriting the validated baseline

Use the existing pinned image in your WSL2/Docker Desktop environment. Create a **new source worktree and build/package directories**; do not switch or clean the baseline checkout's untracked `win64/`. Paths below assume your repository is `$HOME/src/VLC-Timeline-Preview`; adjust that one path as needed. These local commands are supplied for execution, not claimed as completed here.

```sh
VLC_REPO="$HOME/src/VLC-Timeline-Preview"
HOVER_SRC="$HOME/src/VLC-Timeline-Preview-stage241"
HOVER_OUT="$HOME/vlc-stage241"
test ! -e "$HOVER_SRC" && test ! -e "$HOVER_OUT" || exit 1
git -C "$VLC_REPO" fetch origin \
  refs/heads/feature/stage-2.4.1-timeline-hover:refs/remotes/origin/feature/stage-2.4.1-timeline-hover
git -C "$VLC_REPO" worktree add --detach "$HOVER_SRC" origin/feature/stage-2.4.1-timeline-hover
mkdir "$HOVER_OUT"
VLC_GIT_COMMON="$(git -C "$VLC_REPO" rev-parse --path-format=absolute --git-common-dir)"
git -C "$HOVER_SRC" rev-parse HEAD > "$HOVER_OUT/source-commit.txt"
git -C "$HOVER_SRC" status --porcelain > "$HOVER_OUT/source-status.txt"
bash "$HOVER_SRC/test/modules/gui/qt/run.sh" "$HOVER_OUT/host-tests"
set -o pipefail
docker run --rm --cpuset-cpus=0-3 --memory=16g --user "$(id -u):$(id -g)" \
  -e GIT_CONFIG_COUNT=1 -e GIT_CONFIG_KEY_0=safe.directory -e GIT_CONFIG_VALUE_0="$HOVER_SRC" \
  -e WINEPREFIX=/stage241/wine -e XDG_CACHE_HOME=/stage241/cache \
  -v "$VLC_GIT_COMMON:$VLC_GIT_COMMON:ro" -v "$HOVER_SRC:$HOVER_SRC" \
  -v "$HOVER_OUT:/stage241" -w "$HOVER_SRC" \
  registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7 \
  bash -lc 'set -e
    x86_64-w64-mingw32-g++ --version
    MINGW_CXX=x86_64-w64-mingw32-g++ bash test/modules/gui/qt/run.sh /stage241/toolchain-tests
    export VLC_CONTRIB_SHA="$(extras/ci/get-contrib-sha.sh win64)"
    export VLC_PREBUILT_CONTRIBS_URL="https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-${VLC_CONTRIB_SHA}.tar.zst"
    ./extras/package/win32/build.sh -a x86_64 -p
    make -C win64/test test_modules_gui_seekhover.exe
    make -C win64 package-win32-debug-zip
    test -f win64/vlc-3.0.24/vlc.exe
    test -f win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll
    grep -a -q "timeline-hover widget=" win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll
    x86_64-w64-mingw32-objdump -f win64/vlc-3.0.24/vlc.exe win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll > /stage241/pe-headers.txt
    unzip -t win64/vlc-3.0.24-win64-debug.zip
    sha256sum win64/vlc-3.0.24/vlc.exe win64/vlc-3.0.24/plugins/gui/libqt_plugin.dll win64/vlc-3.0.24-win64-debug.zip' \
  2>&1 | tee "$HOVER_OUT/build.log"
```

The worktree and its Git common directory are mounted at their original absolute paths so Git metadata remains readable inside Docker; the baseline metadata mount is read-only. The numeric UID/GID keeps new build outputs owned by your WSL user, with private Wine/cache paths. Reuse any proxy/CA arguments already required by your validated container.

This new worktree needs its own configured build; the existing baseline's config.status can refer to its original source paths, so simply pointing its Makefile at another checkout does not establish source provenance. Reuse the validated dependency image/cache; keep certificates/checksums enabled. The debug ZIP target follows the release's `package-win-common` path and does not request an installer or strip the diagnostic module.

Expected local artifact **only after a successful build**: `$HOVER_SRC/win64/vlc-3.0.24-win64-debug.zip`; unpacked application: `$HOVER_SRC/win64/vlc-3.0.24/`. Verify ZIP integrity (`unzip -t`), inspect both `vlc.exe` and `plugins/gui/libqt_plugin.dll` as PE32+ x86-64 with the pinned objdump, retain source SHA/status/build log and hashes, and copy the **entire** new portable folder to `C:\VLC-Stage241\vlc-3.0.24`. The executable launcher may be unchanged by this Qt-only patch: its hash alone cannot prove modified source. Successful compilation of input_slider.cpp, the rebuilt Qt DLL, marker inspection and provenance records together establish what was built; a native marker log then confirms the module actually loaded.

## Exact Windows PowerShell launch and collection

After the successful local build/copy above, in PowerShell (the paths below intentionally contain no spaces):

```powershell
$App = 'C:\VLC-Stage241\vlc-3.0.24'
$Media = 'C:\VLC-Stage23\fixtures\ordinary.mp4'
$Logs = 'C:\VLC-Stage241\logs'
New-Item -ItemType Directory -Force $Logs | Out-Null
$Log = Join-Path $Logs ('hover-' + (Get-Date -Format 'yyyyMMdd-HHmmss-fff') + '.log')
$env:VLC_TIMELINE_HOVER_DEBUG = '1'
Push-Location $App
& '.\vlc.exe' '--ignore-config' '--no-one-instance' `
  '--no-one-instance-when-started-from-file' '--intf=qt' '--verbose=2' `
  '--file-logging' "--logfile=$Log" $Media
Pop-Location
$Log
```

The new-process flags prevent forwarding this launch to an already-running VLC. Do not launch the unchanged baseline or helper expecting new diagnostics. In a second PowerShell window, tail the path printed above:

```powershell
Get-Content 'C:\VLC-Stage241\logs\hover-REPLACE-WITH-PRINTED-NAME.log' -Wait |
  Select-String 'timeline-hover'
```

To collect just the diagnostic lines after closing VLC:

```powershell
Select-String -Path $Log -Pattern 'timeline-hover' |
  ForEach-Object { $_.Line } | Set-Content ($Log + '.hover.txt')
Get-FileHash -Algorithm SHA256 "$App\vlc.exe", "$App\plugins\gui\libqt_plugin.dll"
# Disable for future launches in this shell; restart VLC to apply.
Remove-Item Env:\VLC_TIMELINE_HOVER_DEBUG -ErrorAction SilentlyContinue
```

Diagnostic field shape (illustrative, **not a captured GUI result**): `timeline-hover widget=... event=move x=160 y=9 width=320 height=18 start_x=9 end_x=311 span=302 timestamp_ms=15010 duration_ms=30021 inside=1 valid=1 active=1 seekable=1 dragging=0`. The real style supplies the endpoints; do not assume these example pixel values on your device.

## Native acceptance and remaining risks

1. With the environment switch absent, verify no `timeline-hover` messages, even at verbosity 2. With it enabled, verify the marker comes from the copied rebuilt Qt module.
2. During MP4 and MKV playback, move without buttons across the timeline, margins and adjacent widgets. Endpoint samples should map to 0/duration; midpoint to approximately half duration (integer floor). Check duration against VLC's current input, including fractional-second files. Playback must continue naturally without position jumps, pauses, volume changes or focus changes caused by hovering.
3. Repeat paused, stopped, no-input, unknown-duration and non-seekable cases. Ineligible states must not log `valid=1`. Qt may not deliver mouse moves to disabled widgets, so silence there is expected; an enter/leave/hide diagnostic is only emitted if Qt delivers that event.
4. Resize, use classic/shiny sliders, enter/leave fullscreen, switch media, move across different-DPI monitors and test RTL/native styles if used. Endpoints must follow current style geometry. Check that old-media duration is not reused on a new movement. No stationary-pointer/media-change refresh is implemented.
5. Verify existing clicks, drag-to-seek, chapter selection, wheel, keyboard/shortcuts, focus and accessibility behavior against baseline. Dragging still seeks through existing code; instrumentation must not add seeks. Leave/hide must invalidate diagnostics. Review any style-specific one-pixel/legacy-mapper differences before building on this instrumentation.

GUI event delivery, native style rendering, file logger behavior and actual playback/accessibility isolation remain unverified. Enabled logging is synchronous and intentionally verbose; it can add UI overhead, so use it for investigation and turn it off afterwards. No popup, extraction invocation, cache or debounce engine exists in this patch. **Stop at Stage 2.4.1; require the user's native hover test and approval before any next-stage integration.**
