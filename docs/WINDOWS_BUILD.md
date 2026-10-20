# Windows x64 baseline build

## Status and authoritative inputs

This is a source-derived procedure, **not a successfully executed build recipe**. Toolchain acquisition was attempted and blocked; compilation and Windows runtime verification remain unrun. Do not label any artifact verified until the checks below pass.

Use release commit `6de05adcbaf2e8b85fe86aad4169393098628119` (annotated tag `3.0.24`), not the fork's original master. Source evidence is the files at that commit:

- `INSTALL` points Windows builders to https://wiki.videolan.org/Win32Compile/ (currently inaccessible here).
- `extras/ci/gitlab-ci.yml` selects `registry.videolan.org/vlc-debian-win64-3.0:20260820114402`, `HOST_ARCH=x86_64`, and `TRIPLET=x86_64-w64-mingw32`.
- `extras/package/win32/build.sh` builds host tools, target contribs, bootstraps/configures VLC, and compiles Qt-enabled desktop VLC by default.
- `extras/package/win32/package.mak` provides `package-win32-zip` and `package-win32-7zip` portable packaging targets.

The CI image tag is pinned by the release, but a tag can move. After pulling, record its RepoDigest and use that immutable digest for repeated builds. The image contents and compiler versions could not be inspected here. Do not invent a digest or compiler version.

## Host prerequisites

Prefer the release's Linux CI image with a functioning Docker runtime. Windows developers may use WSL2 with a Linux distribution and Docker integration, or a separate Linux VM; keep source/build files on the Linux filesystem rather than `/mnt/c`. WSL is not required when building on Linux. Native MSVC is not the documented route in these release scripts. Keep toolchain, CRT and contrib ABI consistent; do not mix MSVC, UCRT and default MinGW binaries.

Allow sufficient CPU, memory and disk for the image, host tools, source tarballs, contrib object trees, VLC and packaging. This machine has four effective CPUs and 32 GiB RAM; only about 29 GiB initially free disk and Docker vfs merit monitoring. Prefer four build CPUs. `build.sh` computes jobs with `getconf _NPROCESSORS_ONLN`; Docker `--cpus` alone may not change that count. Select four available CPUs with `--cpuset-cpus` on the actual host or run on a four-visible-CPU worker. Do not patch upstream build scripts merely to adjust parallelism.

The non-container alternative needs a compatible MinGW-w64 GCC/G++/binutils/windres/widl toolchain, POSIX shell, Git, make, pkg-config, autoconf, automake, libtool, m4, gettext, flex/bison, patch, curl/wget, tar/xz/zstd, CMake, Meson/Ninja, Python, NASM, compression utilities and target development headers. This is a prerequisite inventory, not a tested apt package lock. The release tools bootstrap specifies minimum autoconf 2.71, automake 1.15, bison 3.0, CMake 3.18, libtool 2.4, Meson 0.60, NASM 2.15 and Ninja 1.13; bootstrap can build selected missing tools. Prefer the official image over guessing compatible distro versions.

## Source and toolchain preparation

Use the existing isolated cloud checkout; no worktree is needed. Inspect `git status --short` before switching branches. Preserve any user changes. For a clean checkout without the release tag:

```sh
cd /workspace/VLC-Timeline-Preview
git fetch --no-tags https://github.com/videolan/vlc.git refs/tags/3.0.24:refs/tags/3.0.24
test "$(git rev-parse '3.0.24^{commit}')" = 6de05adcbaf2e8b85fe86aad4169393098628119
# Only when the feature branch does not already exist:
git switch -c feature/stage-1-vlc-3.0.24 3.0.24
```

The branch already exists in this prepared checkout. Do not recreate it or reset it. Before release distribution, compare the primary VideoLAN tag and authenticate the source release as described in STAGE_1_REPORT.md. The mirror tag is unsigned.

On a host with registry access:

```sh
docker pull registry.videolan.org/vlc-debian-win64-3.0:20260820114402
docker image inspect --format '{{json .RepoDigests}}' registry.videolan.org/vlc-debian-win64-3.0:20260820114402
```

Record the digest externally in the build record, then substitute it for the image tag below. The exact attempted pull above failed here with registry `Forbidden`; no alternate image was substituted.

## Baseline compilation and portable packaging

From the checkout, run the following in the pinned image. The bind mount must resolve to this checkout on the Docker daemon host. Use an appropriate UID/GID or adjust output ownership without changing source permissions globally.

```sh
cd /workspace/VLC-Timeline-Preview
docker run --rm --cpus=4 \
  -v "$PWD:/vlc" -w /vlc \
  registry.videolan.org/vlc-debian-win64-3.0:20260820114402 \
  bash -lc 'set -e
    x86_64-w64-mingw32-gcc --version
    x86_64-w64-mingw32-g++ --version
    x86_64-w64-mingw32-gcc -dumpmachine
    ./extras/package/win32/build.sh -a x86_64
    make -C win64 package-win32-zip
    make -C win64 package-win32-7zip
    sha256sum win64/vlc-*.zip win64/vlc-*.7z'
```

This uses the default debug development build and builds contribs from source. For a release-optimized portable build use `./extras/package/win32/build.sh -r -a x86_64` followed by the portable targets; note that `-r` also selects the aggregate installer target and therefore needs NSIS prerequisites. Keep the default baseline first. Do not use `-z` (disables the GUI), `-u` (changes CRT), or `-w` (Windows Store) for this desktop baseline. Do not configure the project's crash reports to use VideoLAN's production endpoint.

`build.sh` expects to be invoked at the repository root and generates `extras/tools`, `contrib/contrib-win64`, `contrib/x86_64-w64-mingw32`, and `win64`. Reuse valid outputs for the same toolchain and flags. Do not use `git clean -xfd` or rewrite tracked files to recover from failure. Preserve build logs and the underlying exit status (use `set -o pipefail` if adding a logging pipeline).

### Dependencies and prebuilt contribs

Contrib recipes pin dependencies and include checksums/patches. The release defaults include Qt 5.6.3 and FFmpeg 8.1.2; do not substitute contemporary Qt or FFmpeg arbitrarily. Qt6 support in the source does not mean this baseline should silently switch to Qt6.

Source builds need access to every host selected by `extras/tools` and contrib recipes, beyond the primary VideoLAN domains. Run `make list` and `make fetch` in the bootstrapped contrib directory to discover the selected closure and failures. Keep checksum, package signature and TLS checks enabled. Archive dependency tarballs, recipe commit, tool versions, flags and logs for reproducibility; bit-for-bit reproducibility has not been demonstrated.

The CI uses `extras/ci/get-contrib-sha.sh win64` to derive the contrib identity and this URL pattern:

```sh
cd /workspace/VLC-Timeline-Preview
VLC_CONTRIB_SHA=$(extras/ci/get-contrib-sha.sh win64)
export VLC_PREBUILT_CONTRIBS_URL="https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-${VLC_CONTRIB_SHA}.tar.zst"
# Only after verifying availability, provenance and matching toolchain:
./extras/package/win32/build.sh -a x86_64 -p
```

These commands belong **inside the matched build environment**. A matching prebuilt was not downloaded or verified. Do not assume it exists or bypass verification if it fails. Default to source contribs when trustworthy matching prebuilts are unavailable.

## Validation before calling a build successful

1. Preserve successful compiler and packaging exit codes; inspect logs for disabled Qt or required plugins. Check `win64/config.log` for the correct host and contrib prefix.
2. Inspect the resulting executable with `file` and `x86_64-w64-mingw32-objdump -f/-p`; confirm PE32+ x86-64 and identify DLL dependencies. Test archive integrity with `unzip -t` / `7z t`. Ensure plugins, Qt dependencies, licenses and required runtime DLLs are packaged.
3. Extract onto a clean Windows x64 machine without an installed VLC providing accidental DLLs. Check `vlc.exe --version`, open the GUI, play local audio/video, seek, pause/resume, switch files and exit. Test audio continuity, video rendering, fullscreen, and plugin discovery. Wine can provide an additional smoke test but does not replace Windows graphics/audio testing.
4. Record source/documentation commits, immutable image digest, tool versions, commands, dependency hashes, package hash, Windows version and test results. Keep private test-media paths out of distributable logs.

No steps in this validation list have been executed against a built artifact. No portable binary is delivered.

## Access requirements and remaining blocker

The draft requests `download.videolan.org`, `code.videolan.org`, `wiki.videolan.org`, `images.videolan.org`, `get.videolan.org`, `registry.videolan.org`, and `artifacts.videolan.org`. Registry authentication/redirect domains and dependency download domains must be added only as observed; this is not a complete contrib allowlist. Review/save and publish the environment settings, then retry the failed checks. Saving the draft does not change current access. No project secret was identified as required, and working GitHub reads did not require a personal token.
