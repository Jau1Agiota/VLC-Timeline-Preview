# Stage 2.4.8D — private offline portable prototype

The launcher is now **offline-only**. A local assembly script combines your
existing Stage F application with your existing original libmpv DLL. It performs
no downloads, installs or runtime builds. The resulting private ZIP is intended
for your own Windows acceptance tests; public redistribution remains uncleared.
That existing restriction does **not** block this private local prototype workflow.

No change to VLC playback/Qt, thumbnail workers, exact/fast seeking, scheduling,
caching, fullscreen or orange-glow styling. `Start VLC Timeline Preview.exe` is
copied unchanged from Stage F and invokes the new package-relative script.

## Assemble on your Windows PC

1. Keep the original Stage F installation intact. Select its actual application
   folder containing `vlc.exe`, `Start VLC Timeline Preview.exe` and
   `BUILD_MANIFEST.json`, not an outer artifact/download folder. Close that VLC
   during assembly so its immutable input files remain stable.
2. Check out/download this branch's source tree, keeping its directory structure.
   No C compiler, Python, MPC-QT, SDK or codec installation is needed to assemble.
3. Locate your existing verified DLL, normally
   `STAGE_F_FOLDER\portable-data\runtime\libmpv-2.dll`, or another local copy.
   The assembler accepts only SHA-256:
   `04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`.
4. In Explorer create an output **parent** folder outside both the Stage F folder
   and repository checkout, for example `VLC-PrivateTests` under your Downloads folder. Allow space for
   another full application folder and a ZIP. Choose a new destination name.
5. Run this one-time developer/assembly command in PowerShell, replacing the
   example paths with yours:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "$env:USERPROFILE\Downloads\VLC-Source\extras\package\portable\Assemble-Offline.ps1" `
  -BaseApplication "$env:USERPROFILE\Downloads\VLC Stage F" `
  -RuntimeDll "$env:USERPROFILE\Downloads\VLC Stage F\portable-data\runtime\libmpv-2.dll" `
  -Destination "$env:USERPROFILE\Downloads\VLC-PrivateTests\VLC-Timeline-Preview-stage248D-local-01"
```

This creates the new folder, its sibling `.zip` and `.zip.sha256`. It prints the
actual ZIP checksum; there is no single prepublished checksum because the package
uses your local Stage F inputs. A second execution with the same destination
**refuses to overwrite** any output. Choose a new name for another assembly.
If an operation fails, newly created partial outputs remain for inspection; no
cleanup targets your original installation. Reparse points/junctions are rejected;
copy inputs into an ordinary local folder if necessary. Use reasonably short paths
for inbox PowerShell/.NET Framework path-length compatibility.

The expected Stage F producer source is
`e29d1dc98c01e64de60de0fe2f299acd2af57aca` (the validated final Stage F package).
Every producer-listed file is checked before copying. Missing/modified inputs
fail clearly; use an intact original extraction rather than editing its manifest.
These local hashes detect corruption/mismatches; they are not publisher signatures.

## Ordinary use after assembly

Disconnect networking, extract the **whole new ZIP** into a new writable Windows
folder, open its versioned application folder and double-click
**Start VLC Timeline Preview.exe**. Then open local MP4/MKV through VLC's menu.
No PowerShell command, runtime setup prompt, download or administrator access is
part of normal startup. You do not install libmpv or MPC-QT separately.

A loadable Windows x64 `vulkan-1.dll` remains a system prerequisite. Compatible
current official graphics drivers commonly provide it, but not every machine
has it. The inherited startup diagnostic probes loadability without enumerating
GPUs or requiring a Vulkan-capable GPU; it installs/downloads nothing. Other
Windows/PowerShell5.1/WinForms prerequisites and enterprise policies still apply.
Do not remove system DLLs or obtain loose DLLs from unknown sites.

## Implementation and layout

- `extras/package/portable/Assemble-Offline.ps1`: validates original DLL, Stage F
  producer manifest and hashes, mandatory x64 PE files, notices, path containment
  and non-reparse inputs. It copies only producer-listed files into a new folder,
  excluding used profiles/caches and unused runtime-downloader scripts. Original
  application/worker EXEs and DLLs are not patched or rebuilt.
- `thumbnail-mpv/libmpv-2.dll`: your supplied original DLL, beside the existing
  exact and fast worker EXEs. Startup pins its hash independently and checks
  selected executable/core/plugin/worker hashes against the new local manifest.
- `portable/Start-Portable.ps1`: offline startup, existing Vulkan diagnostics,
  libmpv/exact defaults, child-only configuration/temp environment and existing
  removable-folder staging. No network/retrieval/extraction path remains.
- `OFFLINE_PROTOTYPE_MANIFEST.json`: new payload hashes, original DLL pin, private
  prototype status, no runtime downloads and no public-redistribution clearance.
- `BASE_STAGE_F_BUILD_MANIFEST.json`: preserved original build provenance under
  a distinct name, avoiding a stale claim that the old manifest covers new scripts.
- `README.txt` and `portable/DEPENDENCIES.md`: current offline operation/notices.
  Other historical Stage F reports remain reference documents, not setup steps.

The assembler writes explicit forward-slash ZIP entry names for compatibility
with inbox PowerShell5.1/.NET Framework and reads/hashes **every archived file**
after creation. It checks for missing/duplicate entries and writes a final ZIP
SHA-256 sidecar. Existing outputs are never replaced. Runtime inputs and output
folder must remain intact/writable; no fixed installation directory is required.

The application creates its own `portable-data` and native portable profile.
Existing profiles/cache files are not copied from Stage F. Existing installed VLC
is isolated using `--no-one-instance` and private child environment. As before,
USB application folders can stage verified worker/DLL copies in local fixed-drive
TEMP, with bounded best-effort removal after VLC exits; OS/driver caches and forced
termination are outside a guarantee that all writes stay inside the folder.
Media on removable/network drives remains outside current preview support.

## Tests and actual limits

The accompanying evidence records available tests and hosted Windows CI. These
are separate from physical Windows/VLC acceptance:

- Synthetic fixture assembly exercises missing DLLs/files, wrong hashes, unsafe
  paths, spaced paths, repeat refusal, cache/profile isolation, ZIP readback and
  extraction. Fixture-only validator replacement is confined to the test process;
  **no production hash override, skip switch or environment bypass exists**.
- Windows tests invoke the real offline startup script in new inbox PS5.1 processes
  and require missing/wrong-hash DLL diagnostics before setup, Vulkan or playback.
  Existing real/mocked Vulkan-loader tests run unchanged.
- Supplemental Linux PowerShell7 tests used the genuine local original DLL with
  the real production validator for hashing, copying and ZIP readback. VLC/core
  files in that fixture were synthetic: **not a playable application package**.
- No complete real Stage F directory is available locally in this cloud workspace.
  The full private package must be assembled from **your Windows Stage F folder**.
  No physical GUI, full-package startup, offline-network-disconnection or playback
  acceptance is claimed by fixture tests. No VLC, libmpv or worker build was run.
- Public packaging's original DLL/cache rejection remains unchanged. This local
  assembler has no upload or Release code. Its dedicated manual CI job uploads
  no runtime, fixtures or ZIP and skips all build/provenance-investigation jobs.

## Physical Windows acceptance — pending

Record the private ZIP hash and Windows/driver versions. Test an extracted copy,
with networking disconnected **before its first launch**, not the assembly source:

- Fresh extraction into a writable path containing spaces; double-click the exact
  launcher name with no console, runtime download, installer, setup dialog or UAC.
- Normal local MP4/MKV playback, audio, pause, clicks and drag-to-seek.
- Responsive timeline hovering with the original orange glow, timestamps and
  retained-image timestamp behavior. Hovering must not seek foreground playback.
- Exact seeks, known timecoded footage and beginning/middle/end requests. Existing
  timestamp-origin/accuracy limitations are not fixed by packaging.
- Rapid wide back-and-forth hovering, media switching and stale-image cleanup.
- Fullscreen/windowed transitions, timeline/window edges and monitor placement.
- Clean close/restart while still offline; no remaining preview workers.
- Stage F still launches separately with its own configuration and original DLL;
  the new folder must not reuse its profile/runtime cache or alter an installed VLC.
- If desired, repeat from a writable external application drive using internal-drive
  media. Never unplug/move an application folder while VLC is running.

If a prerequisite is unavailable, record the message and Windows error. Never
rename/delete system/driver DLLs to simulate failure. Fault tests already cover
those branches. Send acceptance results before any subsequent integration/release.

### Actual verification evidence

[Windows CI run40231201344](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40231201344)
passed. It ran only `offline-prototype`; all application-build, worker, image and
provenance jobs were skipped. **Zero Actions artifacts** were uploaded. See
[test receipt](evidence/stage_2_4_8D_tests.json).

Inbox PowerShell5.1 initially exposed legacy ZIP backslash behavior and an unloaded
compression-type dependency. The assembler now explicitly loads the inbox ZIP
assemblies and emits canonical entry names. The final Windows run passed ZIP
assembly/readback/extraction, source preservation, validation failures, spaced
paths and repeat-execution checks, plus real missing/corrupt-DLL startup failures
and preserved Vulkan diagnostics. No playable VLC fixture was executed.

Supplemental Linux PowerShell7.4.13 tests passed with both synthetic runtime input
(scoped validator fixture) and the **genuine original DLL/real validator**. Original
DLL checksum remained unchanged. Test-created archives were temporary fixtures,
not a full application ZIP and not uploaded. Native GUI/network-off acceptance
remains pending. Stage F public guard suite and static checks are recorded in the
completion report; no public packager rule was relaxed.
