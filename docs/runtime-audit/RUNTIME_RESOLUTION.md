# Stage 2.4.8A: runtime resolution investigation

Date: 2026-10-25. Base `6ac6ea2f4164d242b5d8a2534393effe92988da7`.
No application, controller, worker, launcher or workflow changes. No runtime
replacement, release ZIP, build dispatch, Release or upstream contact.

## Answers

1. **Existing binary:** GPL permits redistribution subject to its terms. There is
   no finding that this runtime is non-redistributable. However, **demonstrably
   complete obligations cannot currently be established** from retrieved materials.
2. **Alternative:** a source-pinned software-render libmpv build is technically
   plausible without changing the application. Its full dependency/toolchain lock,
   build and compatibility tests are **not complete**. It is not yet a verified,
   reproducible replacement binary.
3. **Vulkan:** the source supports disabling Vulkan, and a separately licensed
   loader is another plausible route. Neither is a **verified binary-level fix**
   for this package. Do not claim clean Windows compatibility.
4. **Remaining gaps:** exact static link/source closure of existing mpv, final VLC
   package inventory, notices/source delivery and clean Windows acceptance.
5. **Next implementation:** complete an isolated locked software-runtime build
   experiment and its source bundle, without altering or replacing the shipping
   runtime. Compare using the unchanged worker before approving packaging changes.

## New measured evidence

The existing local SDK archive was read without modification. Both archive and
DLL match the approved hashes (see `measured-sdk-inventory.json` and Stage 2.4.8).
`7z l` lists **six files**: four public API headers, import library and DLL. There
are no runtime license files, source archives or Vulkan loader in that SDK asset.
This does not prove the publisher provides no notices elsewhere.

`objdump -p` confirms the 121,108,480-byte DLL has normal imports of
`vulkan-1.dll`, including `vkGetInstanceProcAddr`, `vkEnumeratePhysicalDevices`,
`vkCreateDisplayPlaneSurfaceKHR` and other exports. This is not merely a GPU
backend that the worker's `hwdec=no` setting can avoid loading. Windows must resolve
normal imports before the libmpv API can be used. Missing loader failure on a
fresh physical Windows machine has not been reproduced in this audit.

Measured imports, file sizes and SHA-256 values are recorded in
[measured-sdk-inventory.json](measured-sdk-inventory.json). Platform-looking names
are **not automatically marked resolved**: API-set/OS version support requires a
supported-Windows check; optional LoadLibrary paths are not in the normal table.

### Provider provenance: available versus missing

Public GET requests confirmed the release and its linked successful workflow:

- [Release 20261021](https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/releases/tags/20261021):
  12 binary assets; no corresponding-source archive listed.
- [Provider run 39518569492](https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/actions/runs/39518569492):
  recipe HEAD `1f8d2e76255abc48f0456d9be99b4cab9c6566e6`, conclusion success.
- [Retained artifacts](https://api.github.com/repos/shinchiro/mpv-winbuild-cmake/actions/runs/39518569492/artifacts):
  eight binary/debug artifacts, no logs artifact listed at inspection time.
  This does not establish whether other historical logs/caches can be recovered.
- [Workflow recipe](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/.github/workflows/mpv_clang.yml#L124):
  logs upload has one-day retention; binary/debug artifacts do not constitute
  complete source. No large artifact download or provider contact was attempted.
- [Git update logic](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/cmake/custom_steps.cmake#L79):
  recipes can reset to an upstream tracking branch. Pinning this outer recipe
  commit does **not** pin every dependency revision used by that historical run.

`provider-recipes.json` records all **108** CMake package recipes, source/tag/URL
text declarations, recipe hashes and matching-name patch hashes from this checkout.
It includes optional packages and build tools. It is **not** the actual dependency
link graph or a license determination. CMake conditionals, variable expansion,
extra patch mechanisms, cached libraries and submodules require separate review.
The complete checkout, not only this extracted list, is necessary build material.

FFmpeg's recorded short revision resolves through its public commit API to
`9a861e9687d30e657d59521817cad18405995983`. This resolves that source identity;
it does not reconstruct exact configure output or dependent libraries. The pinned
FFmpeg recipe enables GPL/version3 and many libraries; the mpv recipe enables
Vulkan and prefers static linking. Final binary license selection needs the actual
linked configuration and all file/component grants. GPL is not a reason to ban
redistribution, and process isolation is not an exemption from distribution terms.

## Comprehensive package inventory: current limit and executable procedure

The full Stage F final ZIP is not present in the inspected local artifact paths;
older Stage 2.4.3 ZIPs cannot identify Stage F contents. A fresh read-only
`gh run download 40192202973` attempt for the named ready-to-extract artifact
failed at `productionresultssa8.blob.core.windows.net` with `Forbidden`
(exit 1); signed URL/query details are intentionally not recorded. The Stage F evidence report
identifies temporary Actions artifacts, not an inspected extraction here. Therefore
**a comprehensive final-package inventory is still incomplete**. No binary names,
versions or licenses are invented to fill this gap.

The new read-only scanner inventories **every regular file** under a supplied
extracted root, hashes it, records normal PE imports through GNU objdump, and leaves
unknown version/license/source/static membership fields explicit. It rejects
symlinks, malformed PE files, overwrites and output inside the package. Use on an
approved extracted Stage F package, without modifying that package:

```sh
python3 tools/release-audit/inventory.py '/path/to/extracted/Stage F' /tmp/stage-f-inventory.json
```

It requires Python 3 and GNU objdump. Perform on a quiescent, trusted local copy;
this is not a hostile-input sandbox. PE import scanning cannot discover every
runtime `LoadLibrary`, Qt/plugin lookup, static library, font or configuration
requirement. Recursively resolve imports against the actual package and supported
Windows installation, compare plugin/worker load tests, and merge source manifests
with the scan. Never label all `api-ms-*` or unfamiliar DLL names bundled/cleared
based on naming alone. No source build is needed for inventory.

The [notice manifest](notice-manifest.json) maps known component families to
source/notice obligations and unresolved states. It is intentionally not an
SBOM claiming all linked components are known. The scanner's six-file SDK run is
actual evidence; running it later against the full portable folder is required.

## Vulkan options

**Source-confirmed cause:** mpv's recipe explicitly enables Vulkan. mpv's pinned
`meson.build:1340–1368` adds Vulkan dependencies and sources when enabled;
libplacebo's recipe also includes Vulkan. The normal import table proves the
resulting load requirement, but does not attribute every imported function to a
particular static object without a link map.

**The worker does not require Vulkan conceptually:** the unchanged accurate worker
requests `MPV_RENDER_API_TYPE_SW`, `hwdec=no` and renders BGR0 through libmpv. mpv
builds `video/out/libmpv_sw.c` and exposes this public software API. This is source
support for trying a non-Vulkan build, not a measured claim of equivalent speed
or fidelity.

**A — bundle a loader:** inspect/build a matching Windows x64 Khronos loader.
Candidate version `v1.4.309` resolves to
`809941a4ca137df69dc9c6e8eb456bd70309197c`.
Its [LICENSE.txt](https://github.com/KhronosGroup/Vulkan-Loader/blob/809941a4ca137df69dc9c6e8eb456bd70309197c/LICENSE.txt)
says majority Apache-2.0, with more permissive file-level exceptions. These licenses
provide redistribution routes with notice/attribution requirements; no blanket
permission for an unidentified prebuilt DLL is asserted. Retain applicable license,
copyright and NOTICE files and identify modifications; inspect all selected
sources/headers/dependencies. Supply other GPL-covered sources as required for
the overall package. Apache-2.0 and GPL version selection need component review
(GPLv3 compatibility differs from GPLv2-only). Do not copy a driver's/system DLL.

A privately bundled loader could satisfy normal import resolution without a Vulkan
ICD for a software-only path, but verify exports, recursive imports, load behavior,
Windows baseline and actual worker use with no GPU Vulkan driver. An ICD/driver
is not supplied by a loader; do not promise general Vulkan rendering support.
This option does not resolve missing corresponding sources of the existing DLL.

**B — build without Vulkan (preferred experiment):** disable it in **mpv,
libplacebo and FFmpeg**, not only the top-level application. Preserve SW rendering
and H.264/HEVC/MP4/MKV/VFR decoding. Confirm no Vulkan imports recursively. No worker
API or cache/controller changes are needed to test this direction.

## Source delivery plan

Use a versioned source companion linked beside the future binary Release asset,
not a source archive inside the user-facing application ZIP. Before distribution:

1. Freeze every source commit/tarball hash, submodule, patch, compiler/sysroot and
   build-tool version; include actual config output and deterministic build scripts.
2. Include corresponding modified VLC, worker, launcher and covered library sources,
   plus required build/install material and applicable static-link/relinking data.
   For static LGPL libraries, assess the applicable version and combined license,
   and provide replacement/relinking materials where required. Do not merely list
   upstream URLs or infer fulfillment from a project's top-level COPYING.
3. Map every binary/asset hash to source/version/license, preserve all notices;
   separate permissive libraries, GPL/LGPL components and genuine OS-provided DLLs.
4. Select a source-access/delivery method satisfying each applicable GPL/LGPL
   version. Co-host complete matching source with equivalent access where required;
   verify availability before binary publication. Do not invent a written offer.
5. A responsible human distributor reviews compatibility and fulfillment. Keep
   this review distinct from performance and Windows device acceptance.

## Offline layout proposed after clearance

Keep `Start VLC Timeline Preview.exe` and the existing VLC/official helper layouts.
Stage the approved DLL and unchanged accurate/fast workers under the private
portable runtime location expected by the launcher. Include NOTICE/LICENSES and a
source-mapping manifest, versioned filename, per-file hashes and final ZIP checksum.
Do not ship headers/import libraries, debug symbols, fixture movies, source archives,
SDK downloads or caches in the application ZIP. The source companion is separate.

After explicit approval, adapt launcher setup to verify only bundled files and
fail helpfully when missing, with **no downloader or network fallback**. Test
relative paths, spaces, removable-folder worker staging and private profiles.
Current Stage F code remains first-launch-online; this plan does not alter it.
The existing packager's rejection of bundled libmpv stays enabled until clearance.

## Remaining gates

- [ ] Complete historical DLL link/source/notices closure, or build/verify a locked replacement.
- [ ] Inventory actual full Stage F package and all static components/assets.
- [ ] Resolve Vulkan recursively with a tested loader or a no-Vulkan build.
- [ ] Assemble corresponding-source delivery and obtain distributor review.
- [ ] Only then approve offline launcher/packager implementation.
- [ ] Run clean Windows CI plus physical offline-first-launch acceptance.
- [ ] Create any public ZIP only after checks and approval; no Release in this stage.

See [alternative build investigation](ALTERNATIVE_BUILD.md) for source pins,
verified option names, missing lock entries and compatibility gates.

## Checks performed on this branch

- Six new inventory regression tests passed: explicit unknown licensing, symlink
  rejection, PE import recording, malformed PE failure, output overwrite rejection
  and output-inside-package rejection.
- Existing CI guard/packaging suite: 48 tests, 41 passed and 7 skipped. It continues
  to reject unreviewed bundled libmpv; no guard was weakened.
- JSON parsing, relative documentation links and all six SDK file hash rechecks
  passed. Bash syntax, ShellCheck 0.10.0, actionlint and Git whitespace checks passed.
- No replacement compilation/linking, real-media comparison, clean Windows loading,
  GUI acceptance or GitHub-hosted workflow execution was performed. No replacement
  binary hash exists; the reported hashes identify only the existing pinned SDK.

### Reproduce the inventory checks

```sh
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover -s test/release-audit -v
bash test/ci/windows-diagnostic-guards.sh
```

Source references for the software-build assessment:
[mpv build](https://github.com/mpv-player/mpv/blob/e471496c56f0f49c133bd88b50aed62b845f1909/meson.build),
[mpv options](https://github.com/mpv-player/mpv/blob/e471496c56f0f49c133bd88b50aed62b845f1909/meson.options),
[software renderer](https://github.com/mpv-player/mpv/blob/e471496c56f0f49c133bd88b50aed62b845f1909/video/out/libmpv_sw.c),
[mpv Copyright](https://github.com/mpv-player/mpv/blob/e471496c56f0f49c133bd88b50aed62b845f1909/Copyright),
[libplacebo options](https://github.com/haasn/libplacebo/blob/cee9b076f2c63104ccfd497fa79c39a867293ec4/meson_options.txt),
[FFmpeg configure](https://github.com/FFmpeg/FFmpeg/blob/9a861e9687d30e657d59521817cad18405995983/configure).
The FFmpeg configure text confirms the documented flag family and Vulkan feature;
configuration and linking were not executed. These public materials establish
plausible build controls, not complete historical binary reconstruction.
