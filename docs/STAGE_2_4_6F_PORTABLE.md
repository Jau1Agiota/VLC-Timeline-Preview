# Stage 2.4.6F portable Windows edition

Base: Stage E 9135cf1778f16ab6dd272bbd38720d281e84e4c3.
Only packaging/startup and tests change; VLC production code and extraction
workers are identical. Stage E borderless glow, image-owned timestamp, Stage D
positioning, exact/fast modes and diagnostics are preserved. Freeze investigation
remains on hold. Draft PR only; no tag, release or merge.

## User operation

Download the Actions artifact, extract its versioned application ZIP into a new,
writable folder, and double-click **Start VLC Timeline Preview.exe**. First use
prompts for an internet download of the existing pinned libmpv distribution.
Archive and DLL checksums are verified, and the DLL is cached locally. Open a
video through VLC's normal menu. Subsequent launches reuse the verified runtime.
This is a first-launch-online portable edition, **not fully offline portable**.
Windows 10 1803+/11 x64 with inbox Windows PowerShell 5.1 and tar is required.
Missing imports can require an official GPU driver update; no loose DLL downloads.

See shipped README.txt and portable/DEPENDENCIES.md. libmpv redistribution remains
uncleared because exact corresponding-source/static dependency obligations are
not fully audited; the ZIP never bundles that DLL. Existing separate official
LibVLC runtime/licenses are retained. Do not redistribute populated portable-data.

## Implementation

`extras/package/portable/launcher.c` is an unsigned, Unicode Windows GUI bootstrap.
It locates its own root and the system PowerShell directly, never cmd/PATH. It
starts a hidden, package-relative script; users need no PowerShell commands.
`Start-Portable.ps1` validates package files, locks setup against concurrent
launches, bounds download to five minutes, verifies the pinned archive and DLL,
extracts only libmpv-2.dll with inbox tar, and copies the unchanged workers into
the private runtime. Cached DLL and workers are verified on each launch; mismatches
fail closed rather than overwrite. Setup temporary files are removed, previous
builds are untouched. Dependency/load failures show a dialog. No UAC/registry
setup, fixed drive paths, global environment changes or installed-VLC interaction.

The child gets libmpv/exact by default, existing optional fast/diagnostic switches,
--no-one-instance and private APPDATA/LOCALAPPDATA/TEMP/TMP. Logs, preview temporary
files, configuration and runtime stay under portable-data where practical.
OS/driver caches and Windows recent-file behavior are outside this guarantee.
Folder must be writable; do not move or unplug while VLC runs. Long-path support,
restricted enterprise execution policy and antivirus trust are not guaranteed.

## Build and validation

Existing pinned GCC 6.4.0 compiles the bootstrap with `-std=c11 -O2 -Wall -Wextra
-Werror -municode -mwindows`, linking only Windows libraries. Stage F uses the
existing manual Windows diagnostic workflow, full clean VLC build and separate
SDK helper runtime. The packager checks PE x64, required launcher/materials,
Stage E/D/C/B markers, rejects bundled libmpv, verifies ZIP integrity and per-file
and final ZIP SHA-256. No personal token, release publishing or automatic triggers.

`test/ci/portable-launcher.ps1` runs Windows inbox PowerShell and tar against the
real pinned download, verifies a second cached run, rejects a corrupted worker,
then starts the native launcher from a path containing spaces and closes only
the process it created. Existing worker/Qt regressions continue. Hosted smoke is
not physical Windows hover/render/USB/fullscreen acceptance.

Manual acceptance: fresh extraction under a spaced path, first-launch download,
second launch offline, open local MP4/MKV, exact/fast, rapid hover retained-image
labels, orange glow, dragging, media changes, fullscreen/multiple monitors,
ordinary A/V, separate installed VLC unaffected, diagnostic switches, writable
USB folder, blocked network, missing/corrupt runtime, close/relaunch. No accuracy
or playback-freeze resolution claims. Keep draft until user acceptance.

Build/CI/artifact evidence will be recorded after execution.
