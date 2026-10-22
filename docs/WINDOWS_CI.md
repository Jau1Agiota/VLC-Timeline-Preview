# Windows diagnostic pilot (Stage CI-1)

Status: implemented and locally checked; **not yet executed on a GitHub-hosted
runner**. A successful hosted pilot is required before calling this pipeline
operational. No existing ZIP, build directory, release or application code is an
input or output of this change. The old Stage 2.4.3 ZIP remains unchanged; its
SHA-256 is `d33f5a8f046178908ea0d4d14e867c3ab28779412f4f4b84bfa453393f0cdb4f`.
A fresh build is not expected to reproduce that ZIP byte for byte.

## Files and ownership

| File | Purpose |
| --- | --- |
| `.github/workflows/windows-diagnostic-pilot.yml` | Manual pilot, separate checkouts, temporary artifacts and readback |
| `ci/windows/build-diagnostic.sh` | Hosted-only preparation, dependency guards, cross-build and cleanup of this run's processes |
| `ci/windows/package-diagnostic.py` | Provenance, bounded helper smoke tests, metrics, ZIP and checksum verification |
| `test/ci/windows-diagnostic-guards.sh` | Local, synthetic safety/packaging regression tests; no downloads or VLC build |
| `docs/WINDOWS_CI.md` | Pilot operation, acceptance and remaining risks |

The CI branch starts from the default branch so its PR changes only these five
files. The **driver** checkout uses `github.workflow_sha`; the **application**
checkout separately uses `2f7cb820e893c8dc53e40cddbab9c57a2fe1a0fc`, the final
Stage 2.4.3 source. Both disable persisted checkout credentials. Full application
Git history is required for the upstream contrib identity calculation. The
source must be clean and have no existing target build/contrib directories.
New CI files deliberately live outside `extras/ci` and `extras/package/win32`,
which participate in that identity calculation.

## Fixed inputs

| Input | Pin / verification |
| --- | --- |
| Official VideoLAN Linux x64 image | `registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7` |
| Compiler / linker | Assert MinGW-w64 target `x86_64-w64-mingw32`, GCC `6.4.0`, binutils `2.27` |
| Contrib identity | `910cc9cada6080dd777fbafb2ebdd0c24f702c30`; assert upstream script yields this value |
| Contrib archive SHA-256 | `49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c` |
| Target Qt | Assert prebuilt headers contain `QT_VERSION_STR "5.6.3"` |
| Official VLC SDK/runtime archive SHA-256 | `1ed59c09152e78aff84663fa76efab26057716576db74332858810e5ef54ae1f` |

Contrib URL:
`https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-910cc9cada6080dd777fbafb2ebdd0c24f702c30.tar.zst`

SDK/runtime URL:
`https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.7z`

These archive hashes record previously verified bytes fetched from official
HTTPS locations; they are not independently signed publisher attestations.
Actions are pinned to full commit IDs (versions annotated in the workflow).
Host apt packages come from the runner's configured Ubuntu repositories; the
runner image and host test-tool versions are not byte-for-byte locked.

## Execution sequence and safeguards

1. Only `workflow_dispatch` is configured. Job permission is `contents: read`.
   No PAT, cloud network secret, release command, release permission or publisher
   is present. Actions artifact delivery uses the runner's own runtime credentials.
2. Allocate a new `$RUNNER_TEMP/vlc-ci1-RUN_ID-RUN_ATTEMPT` with an ownership
   marker. Refuse existing output directories. All results are new run-specific
   files; there is no build cache or reuse of a cloud worktree.
3. Start resource measurements before safety tests, preparation or downloads.
   Validate the clean pinned checkout, then run the focused guard/package tests.
4. Require GitHub's `github-hosted`, Linux/X64, Ubuntu 24.04 environment markers.
   Cleanup accepts only these exact preinstalled SDK directories:
   `/usr/local/lib/android`, `/usr/share/dotnet`, `/opt/ghc`,
   `/usr/local/.ghcup`. Reject aliases, symlinks, mountpoints and overlap with
   checkout/output/Docker storage. No Docker prune, arbitrary cleanup path or
   project/build deletion is allowed. This driver intentionally refuses local
   WSL2 and self-hosted runner execution.
5. Install host-only Qt development tools, FFmpeg, DejaVu font and 7-Zip through
   apt. Require at least **35 GiB available**, and require Docker storage and
   output to share the measured filesystem. Reject Docker's `vfs` driver.
   Require at least 14 GiB physical RAM for the 12 GiB container plus host.
   Recheck free space immediately before pulling the pinned image. If capacity
   is insufficient, stop; do not broaden cleanup or start a contrib source build.
6. Run existing pinned-source hover mapping and Qt offscreen controller tests.
   Fetch contrib/SDK only over verified HTTPS and compare exact SHA-256 values
   before use. Pull the image by digest and record its inspected identity.
7. Generate all existing 30-second burned-in-PTS fixtures using the unchanged
   Stage 2.3 generator. This requires FFmpeg/libx264/AAC/drawtext on the host.
8. Run a normal Docker container, no privileged operations, Docker-in-Docker,
   Windows runner, nested virtualization or PRoot. Use runner UID/GID and bind
   only the fresh source, driver (read-only) and this run's output. Limit the
   container to four CPUs, 12 GiB RAM, 1,024 processes, a 90-minute external timeout and a private
   HOME/Wine prefix. The whole job has a 120-minute limit.
9. Inside the image, the upstream commands are:

   ```sh
   ./extras/package/win32/build.sh -a x86_64 -p
   make -C win64 package-win32-debug-zip
   ```

   Default debug configuration is retained. A `getconf` wrapper bounds the
   upstream parallelism to four. A `make` wrapper allows only verified-local
   `prebuilt`, `.luac` and `list` operations at the root contrib build directory.
   A failed prebuilt marks the run failed; subsequent attempts cannot fall back
   to `fetch`, target contrib compilation, or alternate package URLs. Native
   Lua `.luac` and upstream host-tool bootstrap builds remain expected. Lua
   5.1.5 is prefetched from official HTTPS and checked against the pinned source's
   SHA-512 recipe; host-tool bootstrap retains its upstream recipe checksums.
10. Verify the upstream ZIP CRC, target Qt version and Stage 2.4.3 diagnostic
    string in the newly built Qt DLL. Record PE headers/imports. Compile the
    unchanged helper against **the official SDK's `sdk/lib/libvlc.lib`**:

    ```sh
    x86_64-w64-mingw32-gcc -std=c11 -O2 -Wall -Wextra -Werror \
      -municode -D__USE_MINGW_ANSI_STDIO=1 \
      -I"$sdk/sdk/include" /vlc/prototypes/stage_2_3/thumbnail_helper.c \
      "$sdk/sdk/lib/libvlc.lib" -o "$sdk/thumbnail-helper.exe"
    ```

11. Wine smoke-probe the new main Qt plugin's registration. Exercise the helper
    against ordinary MP4/MKV at 1,000 and 14,750 ms, plus audio-only, corrupt,
    missing-file and short-deadline cases. Every helper has a 15-second external
    bound. Its minimum legal internal timeout is 100 ms: a fast candidate may
    beat that deadline, so report `watchdog_triggered` rather than falsely claiming
    timeout coverage. Existing Qt fake-helper tests cover deterministic controller
    timeouts/cancellation. Candidates are BMP-size/dimension/protocol checked;
    **neither Wine nor a BMP establishes frame PTS accuracy**. Each result records
    wall time and exit status. Terminate only this run's private Wine server and,
    on failure, the container identified by this run's cidfile.
12. Reuse `extras/package/thumbnail-preview.sh` to compose the fresh main app and
    complete official runtime in `thumbnail-preview/`. Do not mix their DLLs or
    plugins. The packager requires x64 PE signatures, both license files, private
    runtime plugins and the diagnostic marker. It rejects symlinks and build
    material. Main `plugins/plugins.dat` and the composition script's obsolete
    Stage 2.4.2 checksum filename are omitted; the private runtime cache is kept.
13. Create a versioned ZIP, verify every member's CRC and SHA-256, then upload
    the delivery directory with `actions/upload-artifact`, compression disabled,
    no overwrite and 14-day retention. Download the artifact by its returned ID
    into a new directory. Compare the downloaded manifest with the producer's
    original and repeat full ZIP/hash verification. Only then is delivery passed.
14. Always summarize measurements and upload logs/metrics as a separately named
    evidence artifact, including failures when the runner remains able to execute
    finalization steps. Hard VM termination can interrupt samples/uploads; missing
    measurements are explicitly unavailable, never reported as zero.

## Package verification and measurements

The delivery artifact contains exactly the portable ZIP plus
`PACKAGE_MANIFEST.json` and `SHA256SUMS`. The ZIP name includes VLC version,
Stage 2.4.3, CI-1, source prefix, run ID and attempt. The internal
`BUILD_MANIFEST.json` records source/driver commits, dependency pins, selected
source hashes, fixed toolchain assertions and individual packaged-file hashes.
It explicitly marks native Windows GUI and frame PTS accuracy as unverified.
The external manifest records the final ZIP size, SHA-256 and internal manifest
hash. `SHA256SUMS` covers the final ZIP and external manifest, avoiding circular
self-hashes. Verification rejects unsafe/duplicate paths and unexpected members.
Hashes detect corruption; the run identity and reviewed workflow establish trust.

`metrics/host.jsonl`, `container.jsonl` and `events.jsonl` contain UTC and monotonic
time, phase, filesystem availability, kernel memory and cgroup CPU/memory/OOM
counters. `summary.json` aggregates measured spans, minimum free space, memory
peaks and Docker exit/OOM state. Kernel memory is VM-wide; cgroup peaks cover the
cgroup lifetime. `vlc-time.json` and `package-time.json` record actual elapsed,
child CPU time and largest-child RSS on success or ordinary command failure;
use container cgroup peak for aggregate memory. Logs include toolchain/PE
inspection, upstream ZIP verification, helper outputs and the full driver log.
No environment/token dump is performed.

The old image was about 2.8 GB compressed / 8.6 GB unpacked. **35 GiB free,
12 GiB container memory, four CPUs, and roughly 30–90 minutes are pilot sizing
assumptions, not clean hosted-build measurements.** Evidence from the first run
must establish actual peaks and build duration. A capacity failure is useful
pilot evidence, not permission to relax checks.

## Local checks (no VLC build)

```sh
bash -n ci/windows/build-diagnostic.sh test/ci/windows-diagnostic-guards.sh
bash test/ci/windows-diagnostic-guards.sh
shellcheck ci/windows/build-diagnostic.sh test/ci/windows-diagnostic-guards.sh
actionlint .github/workflows/windows-diagnostic-pilot.yml
```

The suite uses temporary synthetic PE headers, not executable Windows binaries.
It tests hosted-only/allowlisted cleanup, disk thresholds, output ownership and
symlinks, source rejection, fail-closed contrib invocations, separate runtime and
license requirements, diagnostic marker checks, output preservation, archive path
safety, checksums, producer-manifest comparison, telemetry and failed commands.
These checks do not replace the real hosted compiler/linker/Wine execution.

Implementation-time results: 17 Python tests plus shell guard checks passed;
ShellCheck 0.10.0, actionlint 1.7.7, Bash syntax and Python AST checks passed.
The pinned source's standalone hover mapping test also passed with the host C++
compiler. Full Qt controller tests were not run locally (Qt5Widgets development
metadata is unavailable here); the pilot installs it and runs that suite.
No image was pulled, VLC rebuilt, helper executed or workflow dispatched during
implementation. Windows linking, Wine smoke and artifact readback remain pending.

## Manual pilot instructions — not executed during implementation

GitHub requires a `workflow_dispatch` workflow to exist on the repository's
**default branch** before it can be dispatched. Review and manually merge this
CI-only PR there first. Do not merge application changes or change the pinned
source to accomplish that. Then open **Actions → Windows diagnostic pilot
(CI-1) → Run workflow**, select `master` (or the actual default branch) and
confirm once. Alternatively, after review/merge:

```sh
gh workflow run windows-diagnostic-pilot.yml \
  --repo Jau1Agiota/VLC-Timeline-Preview --ref master
gh run list --repo Jau1Agiota/VLC-Timeline-Preview \
  --workflow windows-diagnostic-pilot.yml --limit 5
# Substitute the chosen numeric run ID; use a NEW local output directory.
gh run watch RUN_ID --repo Jau1Agiota/VLC-Timeline-Preview
# Download the package artifact by its exact name shown in the run summary:
gh run download RUN_ID --repo Jau1Agiota/VLC-Timeline-Preview \
  --name ARTIFACT_NAME --dir ./ci1-download-RUN_ID
cd ./ci1-download-RUN_ID
sha256sum --check SHA256SUMS
# Optional full member verification using this reviewed driver's script:
python3 /path/to/checkout/ci/windows/package-diagnostic.py verify .
```

The downloaded Actions artifact is an outer container; verify the three delivery
files **after extracting/downloading it**, before expanding the portable ZIP.
Save evidence separately. Artifact links require GitHub access and expire after
14 days; save downloads before expiry. CI-1 creates no permanent Release link.

For Windows device testing, unpack into a new directory and keep both runtime
sets intact. From PowerShell inside the portable root:

```powershell
$env:VLC_TIMELINE_HOVER_DEBUG = '1'
.\vlc.exe --no-one-instance --verbose=2 --file-logging --logfile="$PWD\ci1-hover.log"
```

Check windowed and fullscreen controller hovering, timestamps/images, rapid
movement, seeking/dragging, media switching, edges/multiple monitors, failure
behavior and shutdown. Preserve the log. Fullscreen diagnostics are retained;
this stage implements no fullscreen fix and does not pass the Stage 2.3 PTS gate.

## Pilot acceptance and remaining risks

Acceptance requires a real GitHub-hosted run with: clean pinned source; verified
image/contrib/SDK/toolchain; no target contrib fallback; passing existing Qt tests;
successful full Windows cross-compilation/linking and plugin probe; passing helper
smoke checks; isolated complete runtimes; complete resource evidence; and final
ZIP plus artifact-readback verification. Native Windows GUI/audio/process-isolation
and burned-in PTS review remain separate manual gates even after CI passes.

Unproven until the pilot: image registry/archive availability from hosted runners,
runner capacity, clean-build host-tool downloads, headless Wine behavior and
completion inside the chosen budgets. Host apt/runner updates and upstream native
tool availability prevent byte-identical rebuild guarantees. The fail-closed
contrib guard intentionally needs review if upstream build commands change.
A 100-ms helper success means the real watchdog failure path was not exercised
in that run; consult results rather than infer coverage. Artifact retention/quota
is a delivery limit. There are no signing or native-device checks in CI-1.

Future release publishing requires a separate reviewed stage with narrowly scoped
`GITHUB_TOKEN` Contents write permission, re-verification of this manifest/ZIP,
versioned non-overwriting asset names and permanent Release links. **No such
stage or dormant publishing code is included here.**
