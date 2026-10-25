# Stage 2.4.8B — actual software-only libmpv build

## Outcome

**Built and exercised a Windows x64 candidate DLL**, without modifying VLC,
workers, the approved libmpv runtime or Stage F artifacts. It is suitable for
**isolated Windows smoke/device testing**, not selection as the shipping runtime.
Native Windows acceptance, performance parity and redistribution review remain
open. No public binary artifact, portable ZIP or Release was produced.

Final clean candidate:

- Path in this cloud: `/workspace/stage248B-final/prefix/bin/libmpv-2.dll`.
- Size: **30,918,144 bytes** (unstripped experimental DLL).
- SHA-256: `7e61fa3fa8516bff04be3c92cd022cd732da9c8d72cc077c5104e6ae79c236d6`.
- Final build elapsed: **219.876 seconds** using four jobs, from new sources/build/
  sysroot directories and a hash-verified download cache. Timing includes tool and
  source extraction/install, excludes prior network retrieval.
- Target sources, tool downloads, hashes, license notes and observed host tool
  hashes: [inputs.lock.json](../../ci/runtime/inputs.lock.json).
- No source patches. Cross file, actual command list, configuration/build logs and
  results are retained here and in the local source companion.

The initial no-Lua DLL compiled but was rejected by the unchanged worker with
`libmpv option load-scripts: option not found`. This was a real compatibility
failure, not a successful candidate. Corrected builds include Lua **5.1.5** because
the worker sets `load-scripts=no` and `ytdl=no`. Neither worker algorithm was changed.

## Build and dependency closure

See [build instructions](../../ci/runtime/README.md),
[actual commands](commands.json) and [logs](logs/).

| Input | Pin / role |
| --- | --- |
| mpv | `e471496c56f0f49c133bd88b50aed62b845f1909`, same client/render API source revision as approved DLL |
| FFmpeg | `9a861e9687d30e657d59521817cad18405995983`, reduced native software-decoder build |
| libplacebo | `cee9b076f2c63104ccfd497fa79c39a867293ec4` / 7.360.1, GPU backends disabled |
| libass | `bbb3c7f1570a4a021e52683f3fbdf74fe492ae84` / 0.17.4 |
| FreeType | `42608f77f20749dd6ddc9e0536788eaad70ea4b5` / 2.13.3, optional compression/font dependencies disabled |
| FriBidi | `68162babff4f39c4e2dc164a5e825af93bda9983` / 1.0.16 |
| HarfBuzz | `ea6a172f84f2cbcfed803b5ae71064c7afb6b5c2` / 11.0.0 |
| Lua | official 5.1.5 archive, SHA-512 matched against VLC 3.0.24 contrib; SHA-256 locked |
| fast_float / glad | exact libplacebo submodule commits in lock; build/embedded-source roles recorded |
| Vulkan-Headers | libplacebo's pinned header submodule, build/public headers only; **not loader** |
| LLVM-MinGW | 20251021 UCRT Ubuntu22.04 x64 archive; clang 21.1.4 / LLVM `222fc11f2b8f25f6a0f4976272ef1bb7bf49521d`; compiler/sysroot binary checksum verified |
| pkgconf | source-pinned 2.3.0 native build, private pkg-config search paths |
| Meson / Ninja / CMake | 1.8.3 / 1.11.1.4 / 3.31.6, exact wheels/checksums |
| Python / Jinja2 / MarkupSafe | host Python 3.12.14; 3.1.6 / 3.0.3 locked wheels |
| Native bootstrap | Debian13, GCC14.2.0-19, GNU Make4.4.1 and observed host executable hashes; no hermetic OCI image or bit-for-bit rebuild claim |

The target build uses static libraries, explicit cross-compilers/sysroot,
restricted pkg-config directories, disabled feature autodetection and Meson
`nofallback`. It does not pull a dependency from an unspecified moving branch.
Host POSIX tools/native generators remain documented bootstrap dependencies;
this is not a sandbox proof that every host file read has been traced.

Native H.264, HEVC, MPEG4, MJPEG and rawvideo decoders, MP4/MOV and Matroska demuxers
and required software filters are enabled. Networking, hardware accelerators,
external codecs, Vulkan and optional GPU backends are disabled. This is not a
full-feature replacement for mpv as a playback product. External x86 assembly is
also disabled; this is an important performance limitation, not a feature benefit.

### Failures diagnosed and corrected

- libplacebo's disabled-backend stubs still include Vulkan public headers. Added
  exact pinned **headers** and cross include paths, without linking the loader.
- Static libplacebo contains C++ float conversion code. The C DLL linker initially
  lacked C++ symbols; explicit static `-lc++` linkage fixed it. No warning was hidden.
- A first CLI attempt passed `-static,-lc++` as one compiler argument; corrected to
  an actual Meson argument array. The final clean driver uses only the corrected form.
- A parallel exploratory configure reached libass before HarfBuzz installation;
  the committed clean driver builds these sequentially.
- The no-Lua API-option failure required Lua support. The cloud proxy blocks
  `https://www.lua.org/ftp/lua-5.1.5.tar.gz` with a CONNECT 403. Used the existing
  unchanged official source archive, verified against VLC's SHA-512 and locked
  SHA-256. No unidentified mirror or certificate bypass. Source companion includes it.

## ABI, imports and runtime loading

[Static verification](binary-verification.json): **54 mpv exports**, no missing
exports relative to the approved DLL, all imports used by both unchanged worker
EXEs satisfied. No normal import of `vulkan-1.dll`, `libc++.dll`, a separate
libwinpthread or compiler-runtime DLL appears. Remaining normal imports are
Windows platform DLL/API-set names; that is not a clean-Windows compatibility proof.

[Wine API probe](api-probe.json): both DLLs load and report public client API
**131078 / 2.6**, exit 0. [Loaded module observation](wine-loaded-modules.json):
final candidate exact worker produced three HEVC10 images, exit 0, and loaded no
Vulkan module. This exercises recursive load resolution for that path under Wine.
Optional/delay-load paths and native Windows system-component closure remain
unverified. No claim of support for Windows older than the Stage F baseline.

Wine 10.0~repack-6 and its required Debian `libz-mingw-w64` were extracted into an
isolated test area. Earlier relocated Wine8/setup attempts failed before candidate
execution. The test setup needed correct Wine server/data paths and a native zlib
DLL for Wine's own user32 implementation. That DLL is **not imported by our
candidate** and is not an additional Windows application redistributable.

Directly inheriting a Unix stdin pipe made the unchanged worker's `PeekNamedPipe`
return failure and exit 0 with no frame; that was **not** counted as success. The
new test-only Win32 bridge creates real Windows pipes, enforces bounded reads,
uses a kill-on-close Job Object, and requires complete payloads and clean child exit.
No production helper or IPC protocol was modified to accommodate Wine.

## Actual comparison workloads

[Raw results](wine-matrix-raw.json), [summary](wine-matrix-summary.json):
**48/48 cases**, **384/384 full frame payloads**, both approved and candidate DLLs,
exact and fast unchanged workers, 12 synthetic media variants. Includes MP4/MKV,
H.264, 10-bit HEVC, B-frames, long GOP, VFR, scene cuts and positive-origin media.
Each persistent worker receives 500, 25000, 1000, 15000, 28000, 5000, 29500 and
1000 ms requests consecutively; requests remain within its one-active contract.
This models rapid legal back-and-forth engine requests, **not physical Qt hovering**.
All candidate matrix rows identify the final `7e61...36d6` DLL, not an earlier build.

| Runtime / mode | First image median / p95 (12) | Warm image median / p95 (84) |
| --- | --- | --- |
| Approved exact | 559 / 1449 ms | 85 / 470 ms |
| Candidate exact | 339 / 1063 ms | 98.5 / 647 ms |
| Approved fast | 480 / 1089 ms | 43 / 95 ms |
| Candidate fast | 427.5 / 1002 ms | 54.5 / 190 ms |

These are **Win32 pipe image-handoff** times under Wine, not command ACKs or actual
popup display latency. First-image measurements include media/worker startup,
not a pure cold-cache benchmark. Baseline cases ran first, OS caches were not
cleared, and other test/build activity existed. Do not infer a statistically
controlled speed improvement from the lower first-image numbers. Warm numbers
regressed in this run, particularly p95; native assembly is disabled and this
candidate is **not recommended as the shipping performance configuration**.

Worker-reported CPU time is preserved in raw headers. `private_bytes` returned
zero under Wine and is not a trustworthy memory measurement. No usable native
Windows CPU/memory/GPU telemetry or long-form private movie benchmark was collected.

[Edge results](wine-edges.json): **12/12 expected outcomes** across both DLLs:
0 ms ordinary request and 29950 ms VFR MP4/MKV requests produced payloads; audio-only,
corrupt and missing inputs exited bridge code3 with no image. No outer timeout.
Successful frame handoff still says nothing by itself about near-end frame PTS.
Normal cases require worker exit0 and bounded bridge cleanup; native shutdown and
foreground VLC playback isolation were not retested in this standalone experiment.

### Image correctness limits

[Comparison evidence](image-comparison.json): no full-frame byte-identical match
among the 192 paired outputs; median RGB mean absolute error **2.736/255**, max
**3.644/255** per frame. Different conversion/optimization builds can differ; this
metric alone neither establishes cause nor proves temporal correctness.

Five images were visually inspected: candidate exact ordinary request1000 showed
burned PTS1.000; HEVC request15000 showed15.000; VFR request29500 showed29.500.
Positive-origin request1000 showed0.000 in **both** approved and candidate images.
That known mismatch persists. Most observations remain unreviewed, callbacks do
not expose verified PTS, and **no frame-accuracy gate has passed**. Native visual
quality/color and timing review are still required.

## Corresponding-source and licensing status

The source-first approach removes the need to reconstruct the old shinchiro
DLL's unknown static revision closure for **this candidate**. Every library built
by our driver has an exact source hash; no patches were applied. Full matching
archives, including embedded/build source inputs, are preserved locally. LLVM and
MinGW runtime source archives were obtained and hashed separately:
[toolchain source lock](../../ci/runtime/toolchain-sources.lock.json).
Those pins follow the toolchain recipe defaults and compiler-reported LLVM commit;
a full independent attestation/rebuild of the vendor toolchain is not claimed.

[Notice index](notices-index.json) identifies notice files by path/hash in exact
source trees; complete archives retain file headers, not just top-level license
summaries. Main grants include mpv GPL2-or-later, FFmpeg configured GPL/version3,
libplacebo/FriBidi LGPL2.1-or-later, libass ISC, HarfBuzz MIT/file-specific grants,
FreeType FTL-or-GPL choices, Lua MIT, fast_float alternatives and LLVM exception/
MinGW notices. Build-only tools/headers must not be mistaken for linked libraries.
Before distribution choose compatible versions/routes, preserve per-file notices,
review static LGPL conversion/relinking obligations, and assess the compiler runtime
exceptions and any applicable source requirements. GPL is **not** a ban on sharing.

A local **source-only** companion has been assembled and hash-checked:
`/workspace/stage248B-runtime/libmpv-stage248B-source-materials-final.tar`;
[checksum/size](source-kit.json). Includes 15 complete source archives, lock/build
scripts, actual commands, generated cross file, configure/build records and logs.
No downloaded compiler binary, generated candidate DLL, worker EXE or populated
application runtime cache is added; complete source archives retain upstream
source/test materials.
`ci/runtime/source-kit.py --fetch-missing` provides a locked retrieval/assembly
mechanism. The source tar is not a public release asset or legal clearance.

Remaining distribution gates: verify source/notice completeness and license choices
for actual static members/toolchain runtimes; arrange equivalent durable source
access with future binaries; finish the **whole VLC/official-helper** inventory and
corresponding sources; human distributor review. No LGPL-only claim, written offer
or automatic permission to ship the assembled application is invented.

## Stage F inventory recovery

The historical source/branch was not changed. The registered manual workflow now
has an opt-in metadata-only job that builds pinned Stage F application source
`e29d1dc98c01e64de60de0fe2f299acd2af57aca` with its unchanged toolchain/contrib.
It uploads **only file/import/hash inventory JSON**, never candidate/VLC binaries.

[Run 40221095044](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40221095044)
**passed**. [Inventory artifact 12608774775](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40221095044/artifacts/12608774775),
74,126 bytes, GitHub archive SHA-256
`af40feca9161705febfb3cb7453c9b9157818af8e0f924cc008eb78c1003d048`.
Metadata snapshot: [stage-f-ci.json](stage-f-ci.json). Seven-day retention; this is
not a permanent Release link. A public repository's Actions artifacts are not
confidential private storage; no candidate binary was uploaded on that assumption.

Downloading this new metadata artifact back into the cloud still fails with
`Forbidden` at `productionresultssa13.blob.core.windows.net`. Signed URLs omitted.
Therefore the complete inventory was generated/retained successfully in CI, but
its contents have **not** been reviewed here. Filenames/imports also do not resolve
statically linked component licenses automatically. Download via GitHub for review;
whole-package compliance is not declared complete merely because CI is green.

## Tests, next steps and Windows acceptance

- Final clean cross-build succeeded; strict-warning API/bridge probes compiled.
- Seven new build/inventory/parser tests passed; six prior inventory tests passed.
- CI safety/packaging suite: 41 passed, 7 skipped. Bash syntax, ShellCheck,
  actionlint, JSON, local links and whitespace checks passed.
- Wine cases/probes above are actual execution; **no native Windows tests or VLC
  GUI tests** were performed. Existing application source/EXEs remain unchanged.

Recommended next step: review sources/notices and recovered Stage F inventory;
then run controlled Windows tests using the candidate in a **separate** test copy.
Do not replace the approved DLL or change default packaging. For a performance
candidate, lock a native assembler and measure an assembly-enabled FFmpeg build
before selecting a runtime; do not assume the current C-only build is equivalent.

Use [test commands](../../ci/runtime/README.md) to rebuild locally and run the
unchanged worker matrix with the Win32 bridge. Start with API load, ordinary H.264
MP4/MKV and 10-bit HEVC; verify actual burned-in timestamps (including offsets),
image quality/color, widely spaced exact/fast requests, memory/CPU, failures and
shutdown. Only after licensing review should an isolated application copy be wired
to the candidate for hover/fullscreen/playback acceptance. Physical offline-first-
launch acceptance and public ZIP approval remain separate gates. Stop before any
release/merge/runtime replacement or VideoLAN contact.

The unchanged Stage F launcher still validates the **approved** runtime hash.
Do not drop this candidate into that cache: it should be rejected. Device smoke
tests use a separate worker/DLL folder; changing package manifests or startup
selection requires a subsequent approved packaging change. The available unchanged
worker EXEs were copied and hashed, not rebuilt for these tests; identity against
every EXE in the recovered Stage F package remains subject to inventory review.
