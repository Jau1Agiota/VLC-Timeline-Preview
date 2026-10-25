# Stage 2.4.8E — Windows desktop integration

This is an **unofficial VLC modification**, not an official VideoLAN release or
an endorsed product. The application keeps VLC's existing name, icon, executable
and window title. No decoder, worker, VLC production source or thumbnail algorithm
is changed. The Stage D offline package was reported working by the user; Stage E
requires fresh physical Windows acceptance.

## Obtain the first-party tools

Run the manual **Windows diagnostic pilot (CI-1)** GitHub Actions workflow
on `feature/stage-2.4.8E-windows-integration` with **offline_prototype_only=true**.
Leave other switches false. This skips every application/dependency build job.
Its Windows x64 tools ZIP contains the
new launcher, assembly scripts, source, instructions and checksum. It contains
**no VLC application, libmpv DLL or assembled offline application**. The workflow
has `contents: read` and cannot publish a release. Artifacts expire after 14 days.

The verified tools pilot passed in [run 40239015824](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40239015824).
Download the [tools artifact](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40239015824/artifacts/12615990861)
(GitHub sign-in required; expires 8 November 2026). It is not the application ZIP.
The GitHub artifact wrapper SHA-256 is
`c4f6115bead9041763239856af0771d8153c73e908d5780ff12b5101bcea6306`;
the enclosed versioned tools ZIP has its own separate `.sha256` sidecar.

Extract these tools into a new folder. Verify the downloaded ZIP against its
`.sha256` file with `Get-FileHash -Algorithm SHA256` before using it.

## Assemble privately on Windows

Use an intact final Stage F extraction (application source commit
`e29d1dc98c01e64de60de0fe2f299acd2af57aca`) and your original verified DLL. Stage D
and F installations remain untouched. Create a new output parent folder first.
Replace the following example paths with your own:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "C:\Users\You\Downloads\Stage E tools\portable\Assemble-Offline.ps1" `
  -BaseApplication "C:\Users\You\Downloads\VLC Stage F" `
  -RuntimeDll "C:\Users\You\Downloads\VLC Stage F\portable-data\runtime\libmpv-2.dll" `
  -LauncherExecutable "C:\Users\You\Downloads\Stage E tools\Start VLC Timeline Preview.exe" `
  -Destination "C:\Users\You\Downloads\Private tests\VLC-stage248E-local-01"
```

The assembler replaces only the copied launcher and portable support files. All
producer-listed Stage F files are checked first; used caches are excluded. Every
archived file is read back and hashed. A new ZIP, SHA-256 sidecar and
`OFFLINE_PROTOTYPE_MANIFEST.json` are produced, without overwriting existing outputs.
The original DLL pin remains:

`04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`

Extract the resulting ZIP into another writable folder. Double-click
**Start VLC Timeline Preview.exe**. Neither startup nor registration downloads
anything. No installer, administrator access, machine-wide DLL or registry edit
is required for ordinary portable operation. Windows x64 PowerShell 5.1 and the
loadable x64 Vulkan system loader remain prerequisites. No Vulkan GPU capability
test is imposed. Do not copy loose system DLLs into the package.

## Optional registration

Double-click **Register as Video Player.cmd** in the extracted application.
This explicitly registers **VLC media player** for the current user. Then choose
it with Explorer **Open with > Choose another app**, or **Settings > Apps >
Default apps**. Windows controls the final default selection. The description
identifies this portable entry as unofficial; an installed official VLC can
appear with the same familiar display name.

Extensions: `.mp4`, `.mkv`, `.avi`, `.mov`, `.webm`, `.wmv`, `.flv`, `.m4v`,
`.mpg`, `.mpeg`, `.ts`.

The folder-specific ID `VLCTimelinePreview.<24 hex digits>` owns:

- `HKCU\Software\Classes\<ID>.Video` (ProgID, icon and launcher command).
- `HKCU\Software\VLCTimelinePreview\Apps\<ID>` (capabilities and owner marker).
- Its named value in `HKCU\Software\RegisteredApplications`.
- Its named value only in each extension's `OpenWithProgids` key.

No HKLM, UserChoice, extension default value, official `VLC.*` key, DDE or global
`Applications\vlc.exe` registration is written. Remove is idempotent and verifies
owner markers before any edits. Shared extension/OpenWith keys are never deleted.
Registration is not automatic, and never changes the default selection.

Double-click **Remove Registration.cmd** before moving or deleting the application.
If it has already moved, use the new tool's script with the **old absolute path**:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "D:\New folder\portable\Register-Player.ps1" -Action Remove -RegisteredFolder "C:\Old portable folder"
```

A moved package has a new ID and must be registered again. Old entries do not
magically follow it. Missing files produce launcher/registration errors; removal
does not need the old files. Windows may retain chooser history or ask for a new
default after removal; we do not edit protected history to hide that.

## Explorer arguments and instance policy

The C bootstrap base64-encodes the original UTF-16 command line. PowerShell uses
Windows `CommandLineToArgvW`, validates existing absolute local file paths, and
CRT-quotes them into `ProcessStartInfo.Arguments` with `UseShellExecute=false`.
Paths are never interpreted as PowerShell/cmd code or VLC options. Spaces,
Unicode and literal shell metacharacters are supported. Missing files, relative
paths, UNC/device paths and launcher command-line options are rejected. Raw
Explorer arguments are bounded to 8,000 UTF-16 code units. Advanced diagnostics
still use the script's existing explicit switches, rather than the native EXE.

A no-file launch focuses the last tracked VLC **from this folder**, if it is still alive,
validated by PID, process start time and executable path. If none is found it
starts normally. A file-open request intentionally starts a separate, initialized
portable session. Multiple file arguments in one request share that session.
This limitation preserves the existing `--no-one-instance` isolation: VLC 3.0.24
uses global `VLC ipc <version>` objects (`src/win32/specific.c:99`), so enabling its
standard reuse can hand requests to official VLC or another portable folder.
There is **no single-window media handoff in Stage E**. Explorer can invoke once
per selected file, yielding several sessions; use VLC's Open Multiple Files to
avoid that. Concurrent launches while the startup lock is held fail visibly
rather than race initialization. Media requests are not silently discarded.

## Physical Windows acceptance (pending)

Use a freshly extracted path containing spaces and Unicode. Disconnect the
network **before first launch**. Keep the known working installation unchanged.

1. Launch without registration; confirm normal VLC name/icon, playback and no downloads.
2. Register as an ordinary user; verify no UAC and existing defaults unchanged.
3. Choose this entry through Open with; open a local filename containing spaces,
   Unicode and `&`. Test Windows default selection separately, then double-click.
4. Launch without a file again; verify this copy is focused. Open a second media
   file and confirm the documented separate-session behavior, not official VLC reuse.
5. Check MP4/MKV playback, audio, exact seeking, timestamps, retained-image label,
   orange glow, fullscreen, rapid hover and media switching.
6. Close and reopen offline. Confirm no stale helpers and no changes to Stage F.
7. Repeat registration and removal; official VLC and existing default selections
   must not be changed by our utilities. Windows itself may ask for a replacement.
8. Remove before moving; move to a new folder and register again. Also test removal
   using `-RegisteredFolder` for a deliberately missing old folder.
9. Missing/incorrect original DLL must fail clearly without a network fallback;
   a missing Vulkan loader must preserve Stage C's actionable driver guidance.

## Release preparation and blockers

See [release checklist](RELEASE_CHECKLIST.md). No public complete application ZIP,
DLL upload, tag or GitHub Release is authorized. The original runtime's matching
corresponding-source/static-dependency evidence remains unresolved. This does not
prevent the authorized private local prototype. Public redistribution remains
**not cleared**, and the assembly manifest explicitly records that status.

Tests and CI results are recorded in `docs/evidence/stage_2_4_8E_tests.json`.
Automated Windows runner tests do not establish physical device acceptance or
perceived thumbnail performance.

## Recorded verification

The Windows 2022 / inbox PowerShell 5.1 run passed native C compilation, native
EXE-to-PS5 Unicode/path forwarding, CRT quoting, registry registration/removal,
unchanged default/UserChoice values, owner-conflict rejection, missing/moved
folder cleanup, per-folder live/stale process tests, offline assembly and Vulkan
mock/native probes. The launcher is linked with `/MT` and its direct imports are
checked against a Windows-system DLL allowlist. This requires no separate Visual
C++ runtime installation. The native fixture window was not VLC playback.

Local pinned GCC 6.4.0 cross-compilation passed with `-Wall -Wextra -Werror`.
The public guard/packaging suite passed 47 tests with 1 skip; Bash syntax,
ShellCheck and actionlint passed. Linux PowerShell fixture tests and genuine-DLL
copy/hash/ZIP checks passed with synthetic VLC files; no complete real application
was assembled in the cloud. The first CI test assertion confused an absent
extension parent with an empty newly created key; it was corrected to compare
actual default values. No production default-association behavior was changed.

No physical Windows playback or default-app chooser acceptance is claimed.
