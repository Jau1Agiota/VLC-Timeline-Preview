# Stage 2.4.8B isolated Windows libmpv experiment

These scripts build a **candidate DLL**, not VLC or a public portable ZIP. Never
copy it over the approved runtime. Use a new root, a Linux x64 host, Python
**3.12.14** with pip, GCC/G++/make and standard POSIX build utilities. The observed
host is Debian 13, GCC 14.2.0-19, GNU Make 4.4.1. `inputs.lock.json` records bootstrap
executable hashes, host platform and exact source/tool downloads. Meson/Ninja/
CMake/Jinja/MarkupSafe are installed privately from pinned wheels; pkgconf 2.3.0
is built privately. LLVM-MinGW 20251021 contains the fixed cross compiler/sysroot.

The host OS/utility chain is recorded, **not a hermetic pinned container**. Builds
on another host need their environment recorded and results reverified; byte-for-
byte reproducibility is not claimed. The current script deliberately checks the
Python version because the locked MarkupSafe wheel targets CPython 3.12. It does
not silently substitute system target libraries: the cross file, pkg-config
search directories, disabled autodetection and no-fallback configuration are
explicit. Native generators still use the documented host compiler/tools.

```sh
python3 ci/runtime/build.py /absolute/new-runtime-root --jobs 4
# Or reuse only a verified download cache, never a previous build/sysroot:
python3 ci/runtime/build.py /absolute/another-new-root \
  --download-cache /path/to/locked/downloads --jobs 4
```

The Lua origin `https://www.lua.org/ftp/lua-5.1.5.tar.gz` is blocked by this cloud
proxy (`Tunnel connection failed: 403 Forbidden`). This build used the existing
unaltered official archive, verified against VLC's pinned SHA-512 and the new
SHA-256 lock, through `--download-cache`. Do not use an unidentified mirror. If
the origin is blocked locally, obtain the verified source companion/cache rather
than changing hashes or bypassing certificates. The companion includes that
exact archive. Lua is necessary to preserve the unchanged worker's `load-scripts`
and `ytdl` options even though the worker sets both to `no`.

Output: `prefix/bin/libmpv-2.dll`, `CANDIDATE.json`, generated cross file, exact
command list, configure/build logs and elapsed time (including failures after
initialization). No source patches are applied. Vulkan **headers** are needed for
libplacebo's disabled-backend stub/public headers; no Vulkan loader is linked.
FFmpeg uses native software decoders for H.264/HEVC/MPEG4/MJPEG/rawvideo, MOV/MP4
and Matroska demuxers, and no networking, hardware acceleration, external codec
libraries or external x86 assembler. Assembly is disabled for this experiment;
performance must be measured, not assumed equivalent to the approved broad build.
Audio/subtitle playback is not a purpose of this reduced runtime.

## Verify and compare without modifying workers

```sh
python3 ci/runtime/verify.py NEW_ROOT/prefix/bin/libmpv-2.dll \
  APPROVED_RUNTIME/libmpv-2.dll UNCHANGED_WORKERS /tmp/new-verification.json
python3 -m unittest discover -s test/runtime -v
```

The verifier checks normal imports, exported API symbols and each worker's imported
API requirements. It does not certify OS API-set support or dynamic-load closure.

For Wine, the test-only `worker_bridge.c` creates real Win32 anonymous pipes for
the **unchanged** worker; Unix-inherited stdin is not a compatible substitute for
its `PeekNamedPipe` protocol. It uses a kill-on-close Job Object, bounded reads,
child cleanup and records loaded modules after a delivered image. Compile with
the locked toolchain, `-std=c11 -O2 -Wall -Wextra -Werror -municode -lpsapi`.
`api_probe.c` tests DLL loading and client API 2.6, without decoding.

`test/runtime/matrix.py --help` lists arguments. On native Windows, use Python 3,
compiled bridge and new isolated baseline/candidate folders containing **identical
worker EXEs** and their respective DLLs. Run the existing synthetic fixture scripts
from `prototypes/stage_2_3` / `prototypes/stage_2_4_6C` first. Then:

```text
python test/runtime/matrix.py --candidate C:\TEST\candidate --baseline C:\TEST\baseline --bridge C:\TEST\worker-bridge.exe --fixtures C:\TEST\fixtures --new-output C:\TEST\results-new
```

On Linux, supply `--wine PATH_TO_WINE64 --wine-env JSON_WITH_TEST_WINE_ENV` and
Linux paths; the harness translates media/worker paths to Wine's Z: mapping.
Do not use a private movie in committed reports. The outer 90-second deadline and
Win32 Job Object bound teardown; a successful case requires complete image payloads
and worker exit 0. BMPs are inspectable test evidence, not shipped thumbnails.
Measurements are image handoff timings, not actual Qt hover/display latency.

## Corresponding-source material

```sh
python3 ci/runtime/source-kit.py NEW_ROOT/downloads /tmp/new-source-materials.tar --fetch-missing --build-record NEW_ROOT
```

This collects hash-checked complete source archives (including LLVM/MinGW runtime
sources), lock files and build/verification scripts. Compiler binary and host-tool
wheels are fetched separately when building, not bundled in the source tar. Keep
logs, actual commands, configuration and source/DLL checksums with it. Source
access/hosting, notices, toolchain-source correspondence and license-version review
must be completed before distribution; the script does not issue legal clearance.

A future release needs equivalent source access beside its binary, preserved
per-component notices, any applicable LGPL replacement/relinking obligations and
review of the complete VLC package. No Release or public ZIP is created here.
