# Windows x64 baseline build

Stage 1 baseline instructions below are retained for reference. See **Stage 1.1 verification and retry** for the latest results and a WSL2 reproduction path.

## Status and authoritative inputs

Current Stage 1.1 status: official source verification and the official CI image pull passed. The build-container launch failed with Docker exit 125, `no space left on device`, on this 32 GiB cloud filesystem using Docker vfs. Compilation, packaging and Windows runtime verification remain unrun. This is **not a successfully executed build recipe**. Do not label any artifact verified until the checks below pass.

Use release commit `6de05adcbaf2e8b85fe86aad4169393098628119` (annotated tag `3.0.24`), not the fork's original master. Source evidence is the files at that commit:

- `INSTALL` points Windows builders to https://wiki.videolan.org/Win32Compile/ (now accessible, but explicitly deprecated; prefer the release CI/scripts).
- `extras/ci/gitlab-ci.yml` selects `registry.videolan.org/vlc-debian-win64-3.0:20260820114402`, `HOST_ARCH=x86_64`, and `TRIPLET=x86_64-w64-mingw32`.
- `extras/package/win32/build.sh` builds host tools, target contribs, bootstraps/configures VLC, and compiles Qt-enabled desktop VLC by default.
- `extras/package/win32/package.mak` provides `package-win32-zip` and `package-win32-7zip` portable packaging targets.

The CI image tag is pinned by the release, but a tag can move. After pulling, record its RepoDigest and use that immutable digest for repeated builds. Stage 1.1 verified image manifest digest: `sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7`. Docker image size is 8,547,765,093 bytes. Compiler versions remain uninspected because the container could not start.

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

Record the digest externally in the build record, then substitute it for the image tag below. The initial Stage 1 pull failed with registry `Forbidden`; the Stage 1.1 retry succeeded. The downloaded image was subsequently removed to recover disk after container creation exhausted storage. No alternate image was substituted.

## Baseline compilation and portable packaging

From the checkout, run the following in the pinned image. The bind mount must resolve to this checkout on the Docker daemon host. Use an appropriate UID/GID or adjust output ownership without changing source permissions globally.

```sh
cd /workspace/VLC-Timeline-Preview
docker run --rm --cpus=4 \
  -v "$PWD:/vlc" -w /vlc \
  registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7 \
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

## Original access requirements (see Stage 1.1 results below)

The draft requests `download.videolan.org`, `code.videolan.org`, `wiki.videolan.org`, `images.videolan.org`, `get.videolan.org`, `registry.videolan.org`, and `artifacts.videolan.org`. Registry authentication/redirect domains and dependency download domains must be added only as observed; this is not a complete contrib allowlist. Review/save and publish the environment settings, then retry the failed checks. Saving the draft does not change current access. No project secret was identified as required, and working GitHub reads did not require a personal token.

## Stage 1.1 verification and retry

### Reproduce official source verification

Run these in a fresh scratch directory, not over repository files. Commands require curl, GnuPG, Git and sha256sum. HTTPS verification remains enabled. This source verification sequence passed in the cloud instance.

```sh
set -eu
mkdir -p "$HOME/vlc-source-verification"
cd "$HOME/vlc-source-verification"
release=https://download.videolan.org/pub/videolan/vlc/3.0.24
for suffix in '' .sha256 .asc; do
    curl --fail --location --output "vlc-3.0.24.tar.xz${suffix}" \
      "$release/vlc-3.0.24.tar.xz${suffix}"
done
sha256sum --check vlc-3.0.24.tar.xz.sha256
curl --fail --location --output release-key.asc \
  https://download.videolan.org/pub/videolan/keys/1D77C0AE835B911E.asc
mkdir -p gnupg
chmod 700 gnupg
gpg --homedir "$PWD/gnupg" --import release-key.asc
key_fingerprint=$(gpg --homedir "$PWD/gnupg" --with-colons --fingerprint 1D77C0AE835B911E | awk -F: '$1 == "fpr" { print $10; exit }')
test "$key_fingerprint" = A341FD7684DBC1FFD69BC25A1D77C0AE835B911E
gpg --homedir "$PWD/gnupg" --verify vlc-3.0.24.tar.xz.asc vlc-3.0.24.tar.xz
git ls-remote https://code.videolan.org/videolan/vlc.git 'refs/tags/3.0.24*'
```

Expected archive SHA-256: `e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf`. Expected final tag object `cbb63ead663f05413847e8712cbe0eb6833083ff`, peeled commit `6de05adcbaf2e8b85fe86aad4169393098628119`. GnuPG verifies the signature but reports undefined owner-trust in a fresh keyring; the key's identity is sourced from VideoLAN HTTPS, not a preexisting trust graph. See STAGE_1_REPORT.md for the Git/archive comparison.

### WSL2 alternative on Windows x64

This is a reproduction path for an external workstation, **not a claim of a tested WSL build**. It retains the release CI toolchain and avoids unsupported compiler substitutions. It requires working access to VideoLAN and dependency sites; WSL does not solve an upstream outage or access restriction by itself.

1. In an elevated Windows PowerShell, install WSL2 using Microsoft's supported command and reboot if requested:

   ```powershell
   wsl --install -d Ubuntu-24.04
   wsl --set-version Ubuntu-24.04 2
   ```

2. Install Docker Desktop from its official distribution, select Linux containers / the WSL2 backend, and enable integration for Ubuntu-24.04. Allocate at least four CPUs, sufficient RAM, and ample Linux virtual-disk capacity; 60 GiB free is a planning allowance, not a measured minimum. In the Ubuntu shell:

   ```sh
   sudo apt-get update
   sudo apt-get install -y ca-certificates curl git gnupg
   docker info
   mkdir -p "$HOME/src"
   cd "$HOME/src"
   git clone --branch feature/stage-1-vlc-3.0.24 --single-branch \
     https://github.com/Jau1Agiota/VLC-Timeline-Preview.git
   cd VLC-Timeline-Preview
   git merge-base --is-ancestor aace0183b472bfe87685538195636dc5939c5ad6 HEAD
   git fetch --no-tags https://code.videolan.org/videolan/vlc.git refs/tags/3.0.24:refs/tags/3.0.24
   test "$(git rev-parse '3.0.24^{commit}')" = 6de05adcbaf2e8b85fe86aad4169393098628119
   git diff --exit-code 3.0.24 -- . ':!docs'
   git status --short
   ```

   Use this fresh clone for a clean build. In an existing cloud task, use the existing isolated checkout instead; do not create a worktree or erase local changes. Keep WSL builds under `$HOME/src`, not a Windows-mounted path. Run the source verification block above separately.

3. Pull the official release CI image, record its digest, and run the **Baseline compilation and portable packaging** commands above with the recorded immutable image reference. The container supplies MinGW, host tools and Wine; a Windows compiler installation is unnecessary. Building directly in WSL without the container is a separate unvalidated toolchain path. The release wrapper has a WSL fallback for Wine, but the CI container avoids depending on that host detection.

4. For faster contrib preparation, use the exact release-compatible URL below only with the release-matched image:

   ```sh
   export VLC_PREBUILT_CONTRIBS_URL=https://artifacts.videolan.org/vlc-3.0/win64/vlc-contrib-x86_64-w64-mingw32-910cc9cada6080dd777fbafb2ebdd0c24f702c30.tar.zst
   ```

   Pass this variable into Docker with `-e VLC_PREBUILT_CONTRIBS_URL` and invoke `./extras/package/win32/build.sh -a x86_64 -p` inside it. The contrib SHA is derived by the release's `extras/ci/get-contrib-sha.sh win64`. The cloud download completed (355,116,326 bytes); its observed SHA-256 is `49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c`. Compare future downloads with that hash and preserve the artifact for repeated builds. The stock prebuilt rule does not authenticate a detached signature or compare a published pinned hash: use only the official HTTPS artifact, never a mutable `latest` HTTP URL or an untrusted mirror. An observed hash records bytes, not independent publisher authentication. Source contrib builds retain recipe checksums and remain the stronger route when independent dependency verification is required.

5. After successful build and packaging, copy the portable ZIP/7z to Windows, extract into a new directory and perform every artifact and playback check above. Do not rely on system-installed VLC DLLs or plugins. Do not publish an archive whose build or integrity checks failed.

If a corporate proxy is involved, install its administrator-provided CA through supported host/container trust configuration. Do not use `curl -k`, `wget --no-check-certificate`, or disable Git TLS verification. In the cloud container, the platform's existing CA bundle may be mounted read-only and proxy variable names passed with Docker `-e`; never persist proxy credentials in these documents.

### Exact cloud build attempt and failure

The following launch was attempted from the existing clean source checkout (only Stage 1 documentation differed from the release). Standard output/error was captured outside the checkout in `/tmp/vlc-stage11/baseline.log`, and the command's exit code was recorded without a masking pipeline:

```sh
cd /workspace/VLC-Timeline-Preview
docker run --rm --name vlc-stage11-baseline --network host --cpuset-cpus=0-3 --memory=16g \
  -e HTTP_PROXY -e HTTPS_PROXY -e NO_PROXY -e http_proxy -e https_proxy -e no_proxy \
  -v /etc/ssl/certs/ca-certificates.crt:/etc/ssl/certs/ca-certificates.crt:ro \
  -v "$PWD:/vlc" -w /vlc \
  registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7 \
  bash -lc 'set -e
    test -f extras/package/win32/build.sh
    x86_64-w64-mingw32-gcc --version
    x86_64-w64-mingw32-g++ --version
    x86_64-w64-mingw32-gcc -dumpmachine
    getconf _NPROCESSORS_ONLN
    ./extras/package/win32/build.sh -a x86_64
    make -C win64 package-win32-zip
    make -C win64 package-win32-7zip
    sha256sum win64/vlc-*.zip win64/vlc-*.7z'
```

Result: **exit 125**, `docker: Error response from daemon: no space left on device`. The shell inside the container never started: no compiler version was printed, and no VLC configure/compile/package step ran. The failing dependency is the local Docker writable filesystem layer, not VLC source or a compiler error. The image URL/reference above downloaded successfully; this storage error has no failing external URL.

Before pulling, free space was about 30 GiB. After the pull, the vfs-backed daemon had consumed about 21 GiB total filesystem usage, leaving 9.2 GiB. Creating a writable container copy exhausted the 32 GiB filesystem. Docker reported no surviving container; removing only the image downloaded by this task recovered free space to approximately 19 GiB. No other images, containers, caches or repository files were removed. Some allocation from the failed layer remained unreclaimed by image removal; Docker storage maintenance belongs to the platform administrator, not manual edits of daemon internals. The official image is not retained in the current instance.

Do not repeat the same cloud launch until capacity/storage configuration changes. Use a larger cloud filesystem or a supported copy-on-write Docker storage backend on a suitable host. For WSL2, check both `docker info` and free space in Docker Desktop's Linux data disk; free space in the Ubuntu checkout alone is insufficient. The WSL steps above use the same image digest and release; allocate ample capacity before rerunning them.

### Remaining dependency access checks

The original VideoLAN source/build registry domains now work. These additional non-destructive checks failed with HTTP 403 in the current instance:

| Dependency/site | Exact URL checked | Command and error |
| --- | --- | --- |
| Official contrib mirror | `https://downloads.videolan.org/pub/contrib/` | `curl -fIL --max-time 20 URL`, exit 22, HTTP 403 |
| Qt 5.6.3 source | `https://download.qt.io/archive/qt/5.6/5.6.3/submodules/qtbase-opensource-src-5.6.3.tar.xz` | same command, exit 22, HTTP 403 |
| FFmpeg 8.1.2 source | `https://ffmpeg.org/releases/ffmpeg-8.1.2.tar.xz` | same command, exit 22, HTTP 403 |

Saved additive custom-domain requirements for `downloads.videolan.org`, `download.qt.io` and `ffmpeg.org` while preserving existing domains. The first check targets the mirror directory; it does not prove the status of every individual mirrored tarball. Qt and FFmpeg checks target the recipe's exact archives. These are network preflight results, not dependency-build failures: the container storage error occurred first. Review/save and publish the updated environment settings for these additions, then retry affected operations. Other selected contrib destinations remain untested because the build could not reach `make fetch`; do not assume this is a complete allowlist.

The official prebuilt contrib artifact itself is reachable and downloaded, as documented above. It was not extracted, installed or tested, and does not solve the Docker storage failure. No portable ZIP/7z or executable was generated. Native Windows playback and artifact checks remain mandatory after a future successful build.
