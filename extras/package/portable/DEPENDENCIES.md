# Stage F dependency and licensing status

The VLC/worker source and launcher retain GPL-2.0-or-later and repository COPYING.
VLC dependencies retain the existing package notices and separate official
LibVLC helper runtime. Corresponding project source: repository commit recorded
in BUILD_MANIFEST.json at https://github.com/Jau1Agiota/VLC-Timeline-Preview .

The libmpv distribution is **not bundled**. Its pin is unchanged:
shinchiro/mpv-winbuild-cmake release 20261021,
mpv-dev-x86_64-20261021-git-e471496c56.7z; mpv e471496c56f0f49c133bd88b50aed62b845f1909,
FFmpeg N-127893-g9a861e968. URL and archive/DLL SHA-256 are preserved in
thumbnail-mpv/dependencies.json. Provider source/build configuration:
https://github.com/shinchiro/mpv-winbuild-cmake (exact transitive revision
mapping remains unaudited; see pinned release provenance).
Upstream licenses: https://github.com/mpv-player/mpv/blob/master/Copyright
and https://ffmpeg.org/legal.html . These links are information, not a completed
corresponding-source distribution or proof of LGPL-only status.

The previous audit did not establish all static dependency revisions, configure
flags, notices, patches and corresponding-source obligations for that exact DLL.
Therefore Stage F does not claim cleared redistribution, LGPL-only licensing,
or a complete source offer for it. First-launch retrieval is directly from the
identified publisher for the user's private testing; do not redistribute the
resulting populated cache. Archive and extracted DLL are checked before use,
and DLL is checked again on every launch. HTTPS certificate validation stays on.
Only the named DLL is extracted. Setup does not download an extractor, driver,
codec pack or unpinned substitute. Windows inbox tar/PowerShell and system/GPU
runtime dependencies remain prerequisites. Vulkan loader can be an import even
with software decoding; use official GPU drivers, never unidentified DLL sites.

A future offline edition requires a complete license/source audit or an audited,
reproducibly built runtime with fulfillable corresponding-source delivery.
