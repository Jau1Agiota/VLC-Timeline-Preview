# Stage 1 — VLC Timeline Preview

Investigation date: 2026-10-20. Scope: source investigation and Windows x64 build preparation only. No thumbnail UI or playback changes are implemented.

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
