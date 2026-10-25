# Isolated software-only libmpv build experiment

**Status: source-verified design and partial commands, not a validated end-to-end
build recipe.** No runtime was built or substituted. The current VLC application
and worker remain unchanged. This document identifies exactly what is missing
rather than presenting an unpinned provider rebuild as reproducible.

## Source inputs identified

| Input | Candidate commit | Meaning |
| --- | --- | --- |
| mpv | `e471496c56f0f49c133bd88b50aed62b845f1909` | Same public client/render API implementation revision as current runtime |
| FFmpeg | `9a861e9687d30e657d59521817cad18405995983` | Existing recorded revision resolved to full commit |
| libplacebo | `cee9b076f2c63104ccfd497fa79c39a867293ec4` | v7.360.1 candidate, dereferenced annotated tag; **not** asserted original runtime input |

mpv at this pin requires Meson >=1.3.0, requests C11 and C++20, and unconditionally
requires libplacebo >=7.360.1, libass >=0.12.2 and six FFmpeg libraries. Disabling
Vulkan does **not** remove libplacebo or libass. A claim that this can be built from
mpv+FFmpeg alone would be incorrect.

**Missing lock entries:** libass, its required font/text dependencies, libplacebo
embedded/generated third-party inputs, MinGW/Windows sysroot, compiler/linker,
Meson/Ninja/Python/pkg-config/build utilities and any FFmpeg external libraries
actually selected. Record exact versions, hashes, sources, notices and patches
before building. Do not silently use system packages, current branches or provider
caches as a source lock. Disable optional network/disc/script/GPU/encoding libraries
unless required by the unchanged worker's tested functionality.

The available validated VLC compiler reports GCC 6.4.0; it is not validated for
this C++20 dependency build, and Meson is absent on PATH in this audit environment.
No attempt to build with that legacy toolchain is claimed. Use a separately pinned
modern cross-build toolchain; it need not replace VLC's working toolchain, because
the worker communicates across an unchanged DLL ABI/process boundary. Validate the
result with the **existing worker binary**, not only a newly compiled worker.

## Build sequence and source-verified option sets

After completing the full lock, build in a new isolated directory/sysroot. Obtain
all sources at exact commits, archive applied patches and preserve configure logs.
Do not execute the provider's moving-branch update commands against a historical
package and call it a reconstruction. Modern toolchain/container digest and a
Meson cross file must be explicit lock inputs. Use no build-tree directories from
Stage F, and prohibit dependency downloads/fallback during configuration.

Order: toolchain/sysroot -> font/text dependencies -> libass -> minimal FFmpeg
libraries -> non-GPU libplacebo -> libmpv. Preserve license notices and assemble
corresponding sources while producing binaries, not after losing the build inputs.

For pinned FFmpeg, begin with `--disable-autodetect --disable-programs --disable-doc
--disable-network --disable-vulkan --disable-hwaccels --enable-cross-compile
--target-os=mingw32 --arch=x86_64`, plus the locked cross-prefix/sysroot and deliberate
library/static/PIC decisions. Native software H.264/HEVC decoders and MOV/Matroska
demuxers must remain enabled. This is a **candidate configure set, not an executed
command**; audit the pinned configure help, enabled component list and resulting
imports before using it. Encoding libraries such as x264/x265 are not required
merely to decode those formats. Do not claim that eliminating them clears all
license obligations or that the result reproduces the original broad runtime.

The following Meson option names were verified in the indicated sources. `cross.ini`
and all installed pkg-config files **must first come from the completed lock**.
These are commands for that future isolated environment, not runnable setup today:

```sh
# libplacebo v7.360.1: meson_options.txt
meson setup build-placebo src/libplacebo --cross-file cross.ini \
  --wrap-mode=nofallback --auto-features=disabled --default-library=static \
  -Dvulkan=disabled -Dvk-proc-addr=disabled -Dopengl=disabled \
  -Dgl-proc-addr=disabled -Dd3d11=disabled -Dshaderc=disabled \
  -Dglslang=disabled -Ddemos=false -Dtests=false
meson compile -C build-placebo
meson install -C build-placebo

# mpv: meson.options (not meson_options.txt at this revision)
meson setup build-mpv src/mpv --cross-file cross.ini \
  --wrap-mode=nofallback --auto-features=disabled --default-library=shared \
  -Dlibmpv=true -Dcplayer=false -Dgpl=true -Dbuild-date=false \
  -Dvulkan=disabled -Dgl=disabled -Dplain-gl=disabled -Dd3d11=disabled
meson compile -C build-mpv
meson install -C build-mpv
```

The pinned mpv source includes software rendering independently of the OpenGL
backend; retaining `vo=libmpv` and `MPV_RENDER_API_TYPE_SW` is the intended contract.
Configuration/build success and backend equivalence remain untested. Set prefix,
static pkg-config linkage and cross-file search isolation deliberately, and audit
subproject/submodule sources even when `--wrap-mode=nofallback` is used. That flag
alone is not an all-network sandbox or a transitive source lock.

## Validation required before selecting the binary

1. Record final DLL SHA-256, SDK/API version, full source lock and actual compiled
   features. Inspect all normal/delay/dynamic imports and corresponding sources.
2. Require no `vulkan-1.dll` anywhere in the runtime dependency closure; test API
   loading on clean Windows without a Vulkan ICD/driver. A grep alone is insufficient.
3. Start the unchanged exact and fast worker binaries against the candidate DLL
   using an isolated copy, never overwriting the approved runtime. Verify public
   exports and no missing DLLs or runtime compiler-library dependencies.
4. Run H.264, 10-bit HEVC, VFR, long GOP, MP4/MKV, timestamp-origin and near-end
   fixtures, cancellation, media switch and shutdown tests. Compare cold/warm
   latency, burned-in timestamps, image quality, memory/CPU and playback isolation
   with the approved DLL. Command ACKs do not prove displayed-image accuracy.
5. Verify license/source closure for the new runtime and existing VLC package,
   then seek approval before changing offline launcher/packaging behavior.

This experiment does not promise GPU acceleration, new format support or an
accuracy fix; it must preserve the existing software preview behavior. If disabling
GPU features changes image quality/performance, report the measurement before
selecting it. Complete source and runtime hashes do not yet exist for this proposal.
