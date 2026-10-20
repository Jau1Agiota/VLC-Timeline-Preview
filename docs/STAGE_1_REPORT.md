# Stage 1 — VLC Timeline Preview

Investigation date: 2026-10-20. Scope: source investigation and Windows x64 build preparation only. No thumbnail UI or playback changes are implemented.

Stage 1 historical findings are retained below. See **Stage 1.1 follow-up** at the end for current source-verification and build results.

## Source identity and history

| Item | Observed value |
| --- | --- |
| Requested repository | https://github.com/Jau1Agiota/VLC-Timeline-Preview |
| Remote default branch | `master` |
| Original local branch | `work` (platform checkout) |
| Original HEAD / remote master | `e30973a45e8c4f075cf5a6094f500cd3100665f5` |
| Original declared version | `AC_INIT(vlc, 3.0.15)` in configure.ac |
| Original commit | demux: asf: fix regression with broadcasts; committed 2021-06-07 |
| Development branch created | `feature/stage-1-vlc-3.0.24` |
| Upstream release tag object | `cbb63ead663f05413847e8712cbe0eb6833083ff` |
| Release commit (peeled tag) | `6de05adcbaf2e8b85fe86aad4169393098628119` |
| Release declared version | `AC_INIT(vlc, 3.0.24)` |

The initial checkout was clean, non-shallow, and had no local tags. Origin advertised only `master` and no tags. The original HEAD is an exact ancestor of the release commit: `git merge-base` returns the original HEAD, with 3,904 commits between it and the release. This establishes shared upstream history; it does not establish that GitHub formally marks this repository as a fork. There is no evidence that the original master is VLC 3.0.24.

The official VideoLAN GitHub mirror https://github.com/videolan/vlc advertised `3.0.24` and `3.0.24-rc1`, both peeling to the release commit above. Its moving `3.0.x` branch was `843607a066be54af41c4ab69a1de538e687229a6` at inspection; do not build that moving tip as the release. The fetched final tag is annotated by Thomas Guillem, dated 2026-09-22, and identifies VLC 3.0.24 Vetinari. `git verify-tag 3.0.24` reports **no signature found**. This is mirror-based identity verification, not cryptographic publisher authentication.

The mirror advertises the 3.0 release sequence from 3.0.0 through 3.0.24, including 3.0.7.1, 3.0.9.1/.2, 3.0.11.1, 3.0.12.1, 3.0.17.2/.3/.4, release candidates and packaging suffixes. Re-enumerate exact names with `git ls-remote --tags https://github.com/videolan/vlc.git 'refs/tags/3.0.*'`; only the selected final tag was imported locally.

### Official release verification still required

Authoritative locations are https://code.videolan.org/videolan/vlc and https://download.videolan.org/pub/videolan/vlc/3.0.24/. Direct HTTPS checks of both returned proxy CONNECT 403; the official wiki was also blocked. Consequently the release directory, exact source archive filename, published checksums/signatures, and agreement with the primary Git server could not be checked. Do not treat an assumed `vlc-3.0.24.tar.xz` URL or an unverified checksum as established evidence. Resolve access, compare primary tag object and peeled SHA, then authenticate any source archive using the release's published verification material before using it. Building the pinned mirror commit is a separate, explicitly identified source route.

## Safe branch strategy

The dedicated feature branch was created directly from the fetched release tag. Existing `work` and origin/master history remain intact; no reset, default-branch update, upstream write, or push was performed. The documentation commit is a child of the release commit. Its SHA is reported separately in the completion message to avoid a self-referential hash in this file.

Future implementation should continue on this branch (or a child) after baseline validation. Do not merge old master indiscriminately: it is already an ancestor. Audit any future local patches individually. Push only to the user's repository when explicitly requested, never to VideoLAN; use no force push.

## Build investigation and outcome

The release's own `extras/ci/gitlab-ci.yml`, `extras/package/win32/build.sh`, `configure.sh`, `package.mak`, `extras/tools/bootstrap`, and contrib recipes were inspected. They establish a Linux-hosted MinGW-w64 cross-build, Qt GUI and Windows packaging workflow. See [WINDOWS_BUILD.md](WINDOWS_BUILD.md) for the pinned CI image and commands. WSL is an optional Linux host on a Windows workstation, not a dependency of this Linux cloud machine.

Current machine: Debian 13 x86_64; five visible CPUs but four-CPU cgroup quota; 32 GiB memory limit; approximately 29 GiB initial free disk; Docker daemon works but uses space-intensive `vfs` storage. No MinGW compiler, autoconf, or Wine was found. Resources alone do not prove a full contrib build fits available disk.

Attempted `docker pull registry.videolan.org/vlc-debian-win64-3.0:20260820114402`; it failed with `Get "https://registry.videolan.org/v2/": Forbidden`. This blocks the release-matched toolchain. Direct release/wiki checks also failed. No VLC compilation was attempted after these prerequisite failures; no executable or portable package was produced. No native Windows test host is attached. Installing an arbitrary newer compiler is not a verified substitute for the release CI environment.

Required checks: primary release verification, toolchain/container preflight, complete Qt-enabled x64 build, portable package creation, PE/dependency inspection, and native Windows playback smoke tests. Optional initially: installer signing, MSI/NSIS distribution, translations, debug symbols, UCRT variants. None of the unrun checks are reported as passed.

## Architecture and next steps

The Qt integration and isolated extraction proposal are in [THUMBNAIL_ARCHITECTURE.md](THUMBNAIL_ARCHITECTURE.md). First unblock the primary sources and CI registry, record the container digest and dependency provenance, and produce an unmodified baseline. Then prototype isolated extraction on local seekable files and prove playback remains unaffected before adding hover rendering. All original copyright notices, COPYING files, and dependency attribution remain intact. Any later redistribution must preserve applicable GPL/LGPL obligations and corresponding source; review bundled dependency licensing separately.

## Stage 1.1 follow-up — 2026-10-20

Continued on `feature/stage-1-vlc-3.0.24` with a clean working tree; `git merge-base --is-ancestor aace0183b472bfe87685538195636dc5939c5ad6 HEAD` succeeded before changes. No default-branch or playback changes are part of this follow-up.

### Source verification resolved

The approved domains now work in the running environment: the release directory and wiki returned HTTP 200; primary Git reads succeeded. The following primary-source command returned the same annotated tag object and peeled commit already used by this branch:

```sh
git ls-remote https://code.videolan.org/videolan/vlc.git 'refs/tags/3.0.24*'
```

- Final tag object: `cbb63ead663f05413847e8712cbe0eb6833083ff`.
- Final release commit: `6de05adcbaf2e8b85fe86aad4169393098628119`.
- Official source archive: https://download.videolan.org/pub/videolan/vlc/3.0.24/vlc-3.0.24.tar.xz (27,620,304 bytes).
- Published and locally verified SHA-256: `e7cab503d1d7d5849b89d2cf0e1ee60d0ef6d012407791b644b9cfc0cc225fdf`.
- Detached signature: the archive URL plus `.asc`; GnuPG returned `GOODSIG` and `VALIDSIG` for the downloaded archive.
- Signing key: **VideoLAN Release Signing Key 2026**, fingerprint `A341FD7684DBC1FFD69BC25A1D77C0AE835B911E`, obtained from https://download.videolan.org/pub/videolan/keys/1D77C0AE835B911E.asc over verified HTTPS.

GnuPG's owner-trust remains undefined in the isolated keyring; no web-of-trust authentication is claimed. The key's provenance is VideoLAN's HTTPS key directory. The Git tag remains unsigned; archive signature verification is a distinct check, not a claim that the tag is signed. No certificate or checksum validation was bypassed.

Comparing distributed files with the pinned Git tree found 4,099 byte-identical tracked files. Distributed differences were confined to `configure.ac` and translation files under `po/`; 264 tracked paths were absent from the archive. The archive fixes `VERSION_DATE=2026-09-17` instead of computing it through Git. The timeline sources and Qt/FFmpeg contrib recipes matched exactly. CI configuration and the Windows `build.sh` wrapper are not distributed in the archive; retain the pinned Git checkout for this build procedure. The comparison does not claim byte-identical Git and release archive trees.

The accessible Windows wiki explicitly marks itself deprecated and points to current master documentation. Its old package versions and 2016 prebuilt advice must not override the 3.0.24 release's own CI and recipes.

### Build-environment retry outcome

The exact release CI image pull succeeded, with immutable digest `sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7`. Docker reported an image size of 8,547,765,093 bytes. The release-matched official contrib artifact also downloaded (355,116,326 bytes; observed SHA-256 `49f7690b3d09760f842f9f8e9612c28de8cbb1d027bcb3f6aaa25767611eea8c`); it was not installed or tested.

The documented Qt-enabled baseline launch was attempted with that immutable image, four CPUs, 16 GiB container memory, the checkout mounted at `/vlc`, and the existing platform CA bundle mounted read-only. Docker failed **before starting the container**, with exit **125** and `no space left on device`. The 32 GiB filesystem and Docker's vfs full-copy storage implementation could not accommodate the pulled image layers plus a writable container copy. Compiler probes, configuration, compilation and packaging therefore did not run. No application defect or compiler incompatibility was established.

The task's downloaded image was removed to recover disk; no pre-existing Docker resources or repository files were removed. The current instance is not build-ready. See WINDOWS_BUILD.md for the exact attempted command, dependency URL preflight errors, recovery details and a WSL2/Docker Desktop reproduction procedure. The WSL procedure is documented, not executed here. It requires adequate Docker data-disk capacity and network access.

Additional dependency hosts (`downloads.videolan.org`, `download.qt.io`, `ffmpeg.org`) still returned HTTP 403 in preflight checks; their additions were saved in the configuration draft. Full contrib access remains unverified. Source verification is resolved, but storage and remaining dependency access must be addressed before retrying. Do not rerun the same full-copy Docker launch unchanged, disable verification, or substitute an unverified compiler to manufacture a successful result.

Next action: prepare a larger/copy-on-write Linux or WSL2 build host, use the recorded image digest and release commit, complete the baseline build, then run portable-package integrity and native Windows playback tests before proceeding to thumbnail prototyping.
