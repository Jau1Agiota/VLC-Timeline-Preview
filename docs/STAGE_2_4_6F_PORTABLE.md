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

Final build/CI/artifact evidence is recorded below after execution.

Windows preflight exposed `Get-FileHash` missing in the native-bootstrap context,
although direct PowerShell test invocations resolved it. The bootstrap now imports
Windows PowerShell's built-in utility module explicitly from `$PSHOME`, independent
of caller module discovery. Failed startup also records the underlying exception
in a unique portable-data/startup-error-*.log before showing a dialog. An initial
network download failed; subsequent unchanged download attempts succeeded. No
network-cause or retry workaround is claimed. Normal startup performs no network
request once the cached runtime exists and verifies correctly.

A separate source-bootstrap preflight passed native job 121164149173 in
[run 40191794369](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40191794369).
That run tested the corrected script over the earlier extracted package; it was
explicitly not final ZIP validation. Final CI removes that replacement and tests
only the script actually shipped in the new checksummed ZIP.

Removable-storage detail: the unchanged preview controller requires helper and
media paths on fixed local drives (thumbnailpreview.cpp localPath). When the
application is on USB/removable storage, startup stages a checksum-verified
helper/DLL copy in the user's local fixed-drive TEMP, retains it for that VLC
process and removes it on exit (forced termination can leave that private temp
folder). The download cache stays in portable-data. Videos on removable/UNC
storage remain ineligible for previews, as in Stage E; ordinary playback works.
VLC's native portable-directory support (src/win32/dirs.c) stores its own profile
and Qt INI files under portable/; child APPDATA isolation supplements this.
Do not share a writable extracted folder between mutually untrusted users.

## Additional pinned runtime provenance verified during Stage F

GitHub's release API confirms the 20261021 SDK asset is 31,622,190 bytes and
reports SHA-256 `efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633`,
matching the unchanged lock. Its release body links successful provider workflow
[39518569492](https://github.com/shinchiro/mpv-winbuild-cmake/actions/runs/39518569492),
whose API reports build-recipe commit `1f8d2e76255abc48f0456d9be99b4cab9c6566e6`.
At that recipe commit, [mpv.cmake](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/packages/mpv.cmake)
uses `--prefer-static`, enables libmpv and Vulkan, and does not select `gpl=false`;
[ffmpeg.cmake](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/packages/ffmpeg.cmake)
explicitly enables `--enable-gpl --enable-version3` and numerous external libraries.
The [pinned mpv Copyright](https://github.com/mpv-player/mpv/blob/e471496c56f0f49c133bd88b50aed62b845f1909/Copyright)
confirms default GPLv2-or-later and that linked FFmpeg can affect final licensing.
Thus this is not an LGPL-only runtime. Source recipe availability alone does not
prove a complete, matching corresponding-source bundle for all linked revisions.
The Stage F choice remains private publisher download, not DLL redistribution.
