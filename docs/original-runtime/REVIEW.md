# Stage 2.4.8C — original-runtime offline packaging readiness

## Decision

**Redistribution gate remains closed.** This branch adds startup diagnostics,
regression tests and original-runtime/source metadata. It does not produce an
offline ZIP, include libmpv, remove the existing Stage F first-launch setup,
weaken the packager's DLL rejection, or change the approved runtime.

Base: `release/self-contained-windows` at
`6ac6ea2f4164d242b5d8a2534393effe92988da7`.
References: [PR23](https://github.com/Jau1Agiota/VLC-Timeline-Preview/pull/23),
[PR24](https://github.com/Jau1Agiota/VLC-Timeline-Preview/pull/24).
The replacement DLL/build inputs from PR24 are **not** incorporated or proposed.
Its source companion cannot certify the different original binary.

Original DLL SHA-256, rechecked locally without modifying the file:
`04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`.
Size: 121,108,480 bytes. Original SDK asset/archive pins and the source/dependency
plan are in [dependencies-and-source.json](dependencies-and-source.json).

## New startup prerequisite check

`extras/package/portable/Start-Portable.ps1` now probes loadability of
`vulkan-1.dll` after runtime/worker integrity checks and before loading libmpv.
It uses `LoadLibraryEx`, releases its reference, preserves the existing
DLL-directory/default-directory search policy, and does not search CWD/PATH.
An already present runtime-local loader is probed consistently with the existing
libmpv load policy; this is compatibility handling, **not** permission to add or
redistribute such a file. No Vulkan loader is bundled by this branch.

The diagnostic distinguishes a wrong-architecture error and otherwise explains
that the loader or one of its dependencies could not be loaded, with the Windows
error number. It directs users to their PC/graphics vendor rather than loose DLL
sites. The subsequent existing libmpv load check remains and can identify other
missing/transitive dependencies. It no longer automatically blames Vulkan after
that prerequisite check has passed.

**System policy:** a compatible Windows x64 Vulkan loader must be available.
Compatible, up-to-date official graphics drivers commonly provide it, but this
is not guaranteed. The check never enumerates GPUs, creates a Vulkan device or
requires a Vulkan-capable GPU. It does not install drivers, modify Windows or
download dependencies. A loadable loader without a Vulkan-capable GPU is not
rejected on that basis. Application compatibility on every Windows installation
is not claimed. Other libmpv/Windows prerequisites still apply.

No playback, worker EXE/source, scheduling, exact/fast seeking, cache, fullscreen,
orange glow, launcher C source or default backend setting changed. The launcher
filename remains **Start VLC Timeline Preview.exe**. Existing Stage F folders,
ZIPs, DLLs and branches are not updated by this work.

## Original binary redistribution findings

**Confirmed:** the pinned shinchiro release and its GitHub-reported SDK digest
match the existing lock. It links provider run39518569492 and recipe commit
`1f8d2e76255abc48f0456d9be99b4cab9c6566e6`. The published mpv source identity is
`e471496c56f0f49c133bd88b50aed62b845f1909`, FFmpeg resolves to
`9a861e9687d30e657d59521817cad18405995983`. The recipe prefers static linking,
enables Vulkan, and FFmpeg enables GPL/version3 and many external libraries.
There are 17 published recipe patch files; their names/hashes are retained in
our manifest. Individual pinned recipe inputs exist (for example OpenSSL's
`GIT_RESET`), while others follow moving branches and a cached sysroot. Recipe
availability is useful evidence, not the complete historical link/source manifest.

**Confirmed:** the release lists 12 binary assets and no corresponding-source
asset. The SDK contains DLL/import library/API headers, not the complete linked
sources/notices. Retained workflow artifacts list binaries/debug packages; the
separately uploaded build-log artifact has one-day retention and is not listed.
That does not mean the public console logs or all other possible materials vanished.

**New recovery:** direct cloud retrieval of the public console-log archive failed
at `results-receiver.actions.githubusercontent.com` with `Forbidden`. A read-only
GitHub-hosted job then retrieved it with storage HTTP200: 103,665 bytes, SHA-256
`ff018dd35dbafa890bfd0d670206b9abd5b1652fb5b114d29d7990dfeb5821c9`.
[Published-log receipt](published-log-recovery.json) records matching x64 log names.
The revision-observation extraction found no `HEAD is now at` observations. This
is not proof that the logs contain no other useful information, and is not a
static source lock. Complete original static dependency/configuration closure
has **not** been established from the materials retrieved.

**Unresolved:** exact versions/revisions and modifications for every actually
linked static library, complete generated configuration/link membership and cached
sysroot correspondence, all applicable copyright/NOTICE/license material, license-
version compatibility and required corresponding-source/relinking delivery.
A source URL, binary hash, outer recipe commit or public debug archive alone does
not satisfy all these obligations. A current branch rebuild would not prove
historical correspondence and is explicitly outside this stage.

**GPL is not a prohibition.** Redistribution is permitted under applicable terms
when those obligations are met. mpv/API-header ISC notices do not relicense the
DLL; this build must not be described as LGPL-only. Process separation does not
waive binary distribution obligations. Missing evidence is not a finding of an
existing license violation or proof that permission can never be completed.

## Concrete source-delivery plan

Before authorizing a ZIP using the **original** DLL:

1. Complete a per-binary/static-member mapping for the original DLL: match published
   versions, exact source commits/tarballs and hashes, patches/submodules, configure
   results, toolchain/sysroot runtime code and build scripts to the historical
   original build. Retain the pinned provider checkout and actual logs, not only
   our extracted metadata. A human may obtain missing historical build material
   from the provider; no external contact is made by this task.
2. Collect complete matching library sources, modifications and applicable license,
   copyright and NOTICE files. Select compatible GPL/LGPL versions/routes, preserve
   permissive/exception notices and evaluate static LGPL replacement/relinking
   requirements. Do not invent a written source offer or assume upstream links
   alone satisfy every license version.
3. For modified VLC, ship source access for the **future artifact's exact commit**,
   including changes and build/install scripts, plus matching contrib/patch sources
   and configuration. Preserve original COPYING/COPYING.LIB/AUTHORS/THANKS and all
   applicable module/asset notices. Review static libraries inside plugins too.
4. Audit the separate official VLC3.0.24 helper runtime independently, preserving
   its notices and matching configuration/source materials. Official provenance
   does not automatically discharge our obligations as a redistributor.
5. Produce a binary-to-source/license inventory with file hashes and source archive
   checksums. Identify Windows-provided components separately. Treat Vulkan as a
   documented prerequisite, with **no bundled loader or driver** in the plan.
6. Arrange durable equivalent access to complete corresponding sources alongside
   the future binary release. A separate versioned source companion can keep large
   sources out of the application ZIP. Verify accessibility before publishing any
   binary; retain sufficient build/relinking material and notices for all applicable
   components. Responsible human distribution review must explicitly complete the gate.

Until then the existing packager still rejects `libmpv-2.dll` and populated caches.
No new override, environment switch, private bundling path or network fallback is
introduced to bypass it.

## Offline layout after clearance — not implemented

Keep VLC, separate official helper, existing exact/fast worker EXEs, launcher name,
relative paths and private writable data layout. Include **only the original
hash-verified DLL**, required notices and manifest, plus documented OS prerequisites.
Remove runtime download/prompt/extraction code entirely, verify bundled hashes,
and fail helpfully if files or Vulkan are unavailable. Do not fetch a substitute.
Retain removable-folder worker staging and the existing fixed-drive media policy.
These packaging/startup changes require the gate above; this branch does not make
the current first-launch-online launcher an offline product.

## Verification and acceptance

Tests and CI evidence are recorded below after execution. Hosted Windows loader
checks are not physical VLC GUI acceptance. No libmpv or VLC build is required
for these preparatory diagnostics, and none is performed in this stage.

After clearance, a new candidate ZIP must be tested by the user:

1. Extract into a **new** writable folder, including a path with spaces. Keep the
   existing Stage F installation separate. Record package checksum and Windows/
   graphics-driver versions; verify the original libmpv DLL checksum above.
2. Disconnect networking **before first launch**. Double-click the unchanged
   launcher name. Require no installer, elevation, manual PowerShell or download.
3. On a compatible loader installation, verify normal MP4/MKV A/V playback, pause,
   exact seeking, retained-image timestamp labels, rapid hovering and media changes.
   Compare burned-in known footage; image capture/labels do not prove frame PTS.
4. Check fullscreen/windowed positioning, orange glow, monitor/window edges and
   return from fullscreen. The existing timestamp-origin limitation is not fixed here.
5. Close/reopen offline and check worker cleanup/profile isolation. Test moving
   the application folder separately from unsupported removable/network media.
6. On a separate test Windows environment with the loader unavailable, require an
   actionable diagnostic and no download/install attempt. **Do not remove or rename
   system/driver DLLs on a working PC to simulate this.** Use isolated fault tests.
7. A system with a loadable loader must not be rejected just because no Vulkan-
   capable GPU is detected. No GPU enumeration is part of this startup check.

Do not mark this checklist passed until physical testing occurs. No merge,
Release, public offline ZIP or VideoLAN contact is part of this delivery.

## Actual recovered inventory and test results

The original Stage F reconstruction inventory has now been recovered successfully
through bounded metadata annotations, overcoming this cloud's artifact-download
restriction **for this JSON metadata**, not for application binaries.
[stage-f-files.json](stage-f-files.json) contains **1,341 regular files**, including
**725 DLLs and 8 EXEs**, with sizes, SHA-256 and normal PE imports. It comes from
pinned Stage F reconstruction run40221095044, not a new build in Stage 2.4.8C.
Neither `libmpv-2.dll` nor `vulkan-1.dll` is in that pre-existing package inventory.
The original worker hashes are now recorded from the actual recovered inventory;
no worker is rebuilt or replaced. This inventory also identifies official-helper
extras such as `axvlc.dll`; their source/license mapping must not be assumed covered
merely by having VLC's root source tree. Static dependencies still require mapping.

[Windows CI run40228088175](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40228088175)
**passed**; [receipt](windows-ci.json). It ran only the dedicated preparatory job:
other build/worker/image jobs were skipped. Tests use Windows2022 inbox x64
PowerShell5.1. The native C# interop compiled through `Add-Type`; a real loader
probe and a private invalid-image fixture verified error193 capture. Mocked probes
covered present loader without GPU APIs, missing/transitive failures, wrong
architecture, repeated cleanup and runtime-local compatibility. No main launcher
setup body, runtime download, VLC build, libmpv build or GUI playback test ran.

Read-only hosted recovery also obtained provider console-log metadata. The workflow
uses only the existing read-only GitHub token, never a PAT/cloud secret; its API
Authorization header is not forwarded to signed storage. Raw provider logs and
signed URLs are not uploaded or printed. Inventory recovery transports JSON only;
Actions artifacts in a public repository are not confidential binary storage.

Local verification:

- **47 passed, 1 skipped:** Stage F CI guard/packaging/finalization suite. The
  unapproved libmpv/cache rejection still passes; no licensing guard was changed.
- **5 passed:** provenance collector tests, including credential absence, rejected
  redirect, credential stripping, HTTP failure sanitization and invalid archives.
- Bash syntax, ShellCheck, actionlint, JSON/link and whitespace checks passed.
- Original approved DLL checksum remains unchanged; Stage F worktree is untouched.
- **No physical Windows acceptance or offline artifact verification** is claimed.

Developers can run the non-downloading startup tests using inbox PowerShell:

```powershell
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File test/ci/portable-vulkan.ps1
powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File test/ci/portable-vulkan.ps1 -NativeProbe
```

These commands are developer regression tests, not ordinary-use instructions.
The manual workflow input `portable_dependency_only=true` runs them without a VLC
build. Do not dispatch an ordinary Stage F build expecting it to contain this new
script: that build intentionally remains pinned to its historical source. A future
cleared packaging stage must pin the approved Stage C launcher source explicitly.

**Next step:** responsible human review of the original-runtime historical source/
static-library/notice gaps and recovered package mapping. If those materials cannot
be completed, the requested original-runtime offline ZIP remains blocked. No
replacement runtime or source-based rebuild is proposed as a workaround here.
