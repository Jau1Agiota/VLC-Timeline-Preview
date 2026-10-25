# Stage 2.4.8 — self-contained Windows release readiness

## Decision: packaging blocked

Audit date: 2026-10-25. Baseline:
`31c583d5f83f18e06d9853d6e59adca26c1f68e9`, Stage F.
Branch: `release/self-contained-windows`.

**No Stage 2.4.8 executable, offline ZIP or checksum has been produced.**
The user requires licensing clearance before creating a distributable artifact.
This audit cannot establish it. Production code, workers, launcher, workflow and
packaging are unchanged; the existing rejection of bundled libmpv stays enabled.
No release, tag, workflow dispatch or VideoLAN contact is authorized or performed.
The README is a readiness document, not a claim that a new download exists.

## Evidence and provenance

Confirmed means inspected repository material or the specific fetched recipe;
unverified means evidence still needed, not proof of prohibited redistribution.

| Component / binary family | Known origin / license evidence | Outstanding clearance |
| --- | --- | --- |
| Modified `vlc.exe`, `libvlc.dll`, `libvlccore.dll`, plugins and supporting libraries | VLC 3.0.24 plus Stage F changes; GPL-2.0-or-later / LGPL-2.1-or-later vary by file and linked component; repository headers, COPYING, COPYING.LIB | A per-file inventory of the final assembled package, actual linked dependency versions, notices and complete matching source/build material is not established. Root COPYING alone is insufficient. |
| `Start VLC Timeline Preview.exe`, private workers and helper | Existing project GPL-2.0-or-later declarations; build sources remain in this repository | Retain headers and supply exact corresponding source and build scripts; audit all linked library obligations rather than treating each executable as a single-license object. |
| Separate official LibVLC helper runtime, including its plugins/DLLs | Official `vlc-3.0.24-win64.7z`; preserve distinct runtime and its notices | Official download provenance does not itself complete this project's redistribution obligations or enumerate all bundled components. |
| `libmpv-2.dll` and static dependencies inside it | shinchiro mpv-winbuild-cmake, release 20261021; mpv `e471496c56f0f49c133bd88b50aed62b845f1909`; FFmpeg `N-127893-g9a861e968` recorded in project provenance | Complete static dependency revision/configuration/patch/license mapping and corresponding source have not been assembled or verified. Not cleared for bundling. |
| Public libmpv client/render headers | ISC notices in the pinned headers | Preserve notices; this grant does not cover the entire DLL implementation. |
| `vulkan-1.dll`, Windows/UCRT imports | Existing worker provenance reports Vulkan and platform imports; software rendering/decoding is used | Vulkan loader is not guaranteed on a clean PC. Identify exact loader dependencies and redistribution terms, or establish a compatible runtime without this external requirement. No new runtime or engine is selected here. Windows system-component exceptions must be assessed separately. |
| Fonts, translations, artwork, Qt and other VLC/contrib libraries | Existing source notices and contrib recipes | Match actual shipped assets/binaries to versions and notices; retain applicable font and asset licenses. A source-tree recipe list is not a complete binary inventory. |

**Inventory limitation:** this table identifies known binary families, not every
third-party binary. A final Stage F extracted package and its complete dependency
closure have not been audited here. Static dependencies also cannot be fully
identified by listing DLL imports. The requirement to identify *every* bundled
third-party binary is therefore unmet; no compliance certification is claimed.
Older Stage 2.4.3 archives are not substitutes for Stage F inventory.

### Immutable inputs

- VLC contrib revision: `910cc9cada6080dd777fbafb2ebdd0c24f702c30`;
  archive SHA-256 `49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c`.
- Official helper SDK/runtime URL:
  <https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.7z>;
  SHA-256 `1ed59c09152e78aff84663fa76efab26057716576db74332858810e5ef54ae1f`.
- mpv SDK archive: `mpv-dev-x86_64-20261021-git-e471496c56.7z`;
  SHA-256 `efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633`.
- libmpv DLL SHA-256:
  `04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`.
- Provider recipe commit: `1f8d2e76255abc48f0456d9be99b4cab9c6566e6`;
  linked provider run `39518569492` is recorded in Stage F.

Pins come from `ci/windows/build-diagnostic.sh`,
`prototypes/stage_2_4_6B/dependencies.json` and
[Stage F evidence](STAGE_2_4_6F_PORTABLE.md). Hash identity is not legal clearance.

This audit fetched the pinned provider recipes directly:
[mpv.cmake](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/packages/mpv.cmake),
[ffmpeg.cmake](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/packages/ffmpeg.cmake).
FFmpeg recipe lines 60–61 explicitly enable `--enable-gpl --enable-version3`.
This is not evidence of an LGPL-only runtime. Source repositories and build
recipes are useful provenance, but do not prove every selected source revision,
patch and generated configuration for the published DLL is obtainable and matched.

## Required resolution before a distributable package

1. Obtain the full matching runtime build manifest, logs, pinned transitive source
   revisions, patches, configuration and notices from the provider's published
   materials. Record any gaps. Do not silently substitute a different binary.
2. Inventory each final VLC/helper/worker/launcher executable, DLL, plugin and asset
   with hash, source version, license and source-delivery mapping. Include static
   dependencies, and investigate Vulkan's dependency closure on clean Windows.
3. Assemble accessible corresponding-source delivery for the exact binaries,
   including necessary build/install scripts and modifications. Evaluate the
   applicable GPL/LGPL versions and license compatibility per component. Provide
   required copyright/license notices and LGPL replacement/relinking material
   where applicable. Do not invent a written offer or assume upstream URLs alone
   satisfy every obligation. A source archive need not be inside the application
   ZIP, but its required delivery/access must be arranged before distribution.
4. Have the completed evidence reviewed by the responsible human distributor.
   If exact sources cannot be matched, consider a separately approved reproducible
   runtime build with audited dependencies; no such build is attempted here.
5. Only after clearance, amend packaging to include the verified runtime, remove
   network setup, preserve the launcher filename and relative-path behavior, and
   validate on a fresh offline Windows machine. Keep historical builds unchanged.

Neither a private download, process isolation, a checksum nor GPL-licensed project
code automatically clears redistribution. Conversely, missing evidence is not a
finding that redistribution is categorically forbidden.

## Verification status

This change is documentation-only. Existing focused CI safety/packaging tests and
available static checks are recorded in the accompanying PR. No new full VLC
build, Windows CI execution, packaged dependency-completeness verification or
physical Windows test is claimed. Historical Stage F CI remains historical
Stage F evidence, not Stage 2.4.8 certification. Build-dependent/Windows-only
checks cannot validate a package that was deliberately not created.

## Windows acceptance — pending

After clearance and a new verified CI artifact, a human tester must:

- Extract a fresh ZIP into a writable path containing spaces, on a machine without
  a populated project runtime cache. Keep previous installations separate.
- Disconnect networking **before first launch**. Double-click
  `Start VLC Timeline Preview.exe`; require no runtime fetch, administrator prompt,
  manual command or system-wide installation. Record Windows/GPU/driver versions.
- Open local fixed-drive MP4 and MKV; verify normal video/audio, pause and seeking.
- Hover near beginning, middle and end; verify orange glow, timestamp association,
  retained images, and fast back-and-forth motion without foreground seeking.
- Check precise requests against burned-in timestamps; record discrepancies.
  Command completion or a timestamp label is not proof of decoded frame accuracy.
- Test fullscreen, window edges, monitor changes and returning to windowed mode.
- Switch media and check stale images/cancellation; close and reopen offline.
- Verify no orphan workers and isolation from an installed VLC instance.
- Move the folder to another normal writable location and repeat; test portable
  storage with fixed-drive media separately from unsupported removable media.
- Retain logs, package manifest and SHA-256, and report pass/fail per item.

Do not mark acceptance complete until the user performs these checks. No merge,
public Release or replacement of Stage F artifacts is part of this stage.

### Actual checks on this documentation branch

- CI safety/packaging/finalization suite: 48 tests, 41 passed, 7 skipped.
- Stage 2.3 Python suites: 21 passed; Stage 2.4.6: 6 passed;
  Stage 2.4.6B: 6 passed; Stage 2.4.6C: 2 passed.
- Host C++ timestamp mapping test: passed with strict compiler warnings.
- Bash syntax, ShellCheck 0.10.0, actionlint and documentation relative-link
  checks: passed. No CI, production or helper source file changed.
- Host Qt development packages were not available through pkg-config; the Qt
  controller suite was not rerun. Native PowerShell/Windows GUI tests, real-media
  extraction benchmarks and a full VLC build were not run. No new GitHub-hosted
  CI result is claimed, and no package structure test can certify a nonexistent
  Stage 2.4.8 artifact. Existing packager tests retain their fail-closed DLL guard.
