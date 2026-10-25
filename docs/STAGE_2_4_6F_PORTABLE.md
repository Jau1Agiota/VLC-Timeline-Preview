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

## Short Windows acceptance procedure

1. Download the **ready-to-extract Stage F Portable** artifact below, extract once
   into a new writable folder with spaces, and open its versioned application
   folder. Keep earlier installations separate.
2. Double-click **Start VLC Timeline Preview.exe** with internet available. Accept
   the first-launch setup information dialog and wait for VLC. No console commands
   or administrator setup should be needed. If setup fails, retain the newest
   `portable-data/startup-error-*.log` and report its message; do not replace DLLs.
3. Open an internal-drive MP4/MKV through VLC's normal menu. Check the orange glow,
   retained-image timestamp, rapid reversals, dragging, media changes, fullscreen,
   window edges and ordinary audio/video against Stage E. This is user acceptance,
   not a new frame-accuracy or playback-freeze investigation.
4. Close this VLC, disconnect internet, and double-click again. The verified cache
   should be reused. Keep an existing installed VLC running independently if desired
   to check instance/profile isolation.
5. With Stage F closed, move the entire folder to writable USB storage and launch
   again, opening an internal-drive movie. Its helper is staged in local TEMP.
   The unchanged engine still excludes videos stored on removable/UNC paths from
   previews. Close before unplugging; physical USB and other-device testing is pending.

For developers, the reproducible clean build is the existing **Windows diagnostic
pilot (CI-1)** Actions workflow: select this branch, `stage=2.4.6F`,
`benchmark_only=false`, `native_only=false`. Source and driver pins are independent;
see the final evidence below and [WINDOWS_CI.md](WINDOWS_CI.md) for the unchanged
image/contrib/toolchain, resource limits and source-verification gates. Temporary
artifacts expire after 14 days; no Release or tag is published by this stage.

## Final build and delivery record

- Application/portable bootstrap source: `e29d1dc98c01e64de60de0fe2f299acd2af57aca`.
- CI driver: `630528dcc8e3c3a327503ceeadc6450610d76947`.
- [Final clean run 40192202973](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40192202973).
- [Recommended one-extraction portable ZIP](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40192202973/artifacts/12599395616).
  Artifact name: `VLC-Timeline-Preview-Stage-246F-Portable-e29d1dc98c01e64de60de0fe2f299acd2af57aca-40192202973-1`.
  GitHub-reported artifact ZIP SHA-256:
  `5989fb3de476dabc75d1d43128fccb17ec5a8f108a1dfa0dd90011aa652bccd7`
  (521,719,362 bytes). This is the ZIP to extract once and double-click its launcher.
- [Developer canonical ZIP/checksum/manifests](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40192202973/artifacts/12599270689).
  Inner ZIP: `VLC-3.0.24-stage246F-ci1-win64-e29d1dc98c01-40192202973-1.zip`;
  SHA-256 `6b91dbc53e9e6f7d60ec22462589c41145f32ba186c6f1d1633b1435aa1d4f1c`
  (512,581,595 bytes). The CI producer and independent artifact readback verify this
  canonical ZIP, every member hash/CRC, provenance and external SHA256SUMS.
  The ready ZIP is a direct extraction of those verified contents, re-archived by
  GitHub Actions; its container hash differs. It does not contain a nested ZIP.
- Earlier Stage F artifact from run 40188859730 is superseded: it failed native
  startup. Earlier Stage E and all previous builds remain untouched.

Final native validation passes the **shipped** bootstrap: inbox Windows PowerShell
and tar runtime setup, cache reuse with unchanged DLL write time, rejection and
restoration of a deliberately corrupted cached worker, forced fixed-drive helper
staging/cleanup, and native process startup from a folder containing spaces.
Existing fast/exact/LibVLC regressions report no failures. Existing Qt/dummy-output
smoke exits zero both ways: diagnostics off has 0 heartbeats/0 statistics samples;
on has 30 heartbeats/30 samples and positive decoded-video counters. This is not
physical Windows GUI, audio output, thumbnail appearance, USB or offline testing.
First native consent-dialog interaction remains part of manual acceptance; CI
performs setup through the noninteractive test switch before cached native launch.

Local checks: strict pinned GCC 6.4.0 launcher build, PE GUI/x64/system-import
inspection, Bash syntax, ShellCheck 0.10.0, actionlint 1.7.7, host Qt regressions;
48 guard/package tests (47 pass, 1 skipped), plus Stage E control (45 pass, 3
skipped). No protected production/worker source differs from Stage E.
Machine-readable API/check evidence: [stage_2_4_6F_ci.json](evidence/stage_2_4_6F_ci.json).
Artifact checksums/digests above distinguish independently verified canonical ZIP
from GitHub's reported ready-artifact digest; no cloud re-download is claimed.
Physical acceptance remains pending; [PR #20](https://github.com/Jau1Agiota/VLC-Timeline-Preview/pull/20)
remains a draft. No tag, Release, merge or previous-build replacement occurred.

The inherited image-analysis job completed without scorer failures; this is not a
universal accuracy gate. Its known positive-origin fixture still shows roughly
5-second errors, fast mode remains keyframe-coarse, and actual image PTS/human
review remain unverified. Stage F neither changes nor claims to resolve those
existing limitations. Final run, native job and image-analysis job all succeeded.
