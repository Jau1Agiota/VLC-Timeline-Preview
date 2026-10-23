The worker is independently implemented GPL-2.0-or-later (repository COPYING).
Public libmpv client/render headers are ISC licensed. No MPC-QT code was copied.
Reference: MPC-QT d2d0d15de6e9113df19a4d6e539897de2227a16d (GPL-2.0).
A.1 reference: e9d79b96ae4af3691693c894d8b4e44b11a999d8; only the pinned
runtime setup is carried over, not the standalone UI or diagnostic capture path.

Runtime: identified shinchiro/mpv-winbuild-cmake release 20261021, mpv
v0.41.0-1227-ge471496c5, FFmpeg N-127893-g9a861e968. Exact URLs and hashes:
dependencies.json. This is a third-party mpv distribution, not a VideoLAN binary.
Imports include Windows/UCRT and vulkan-1.dll (normally provided by the GPU driver).
This integration uses software decoding and SW rendering, not hardware decoding.

The complete corresponding source, build options and notices for the runtime's
mpv/FFmpeg/static dependencies have NOT been audited. Do not describe it as LGPL
or assume redistribution is cleared. The VLC package excludes libmpv-2.dll and
its import library. Setup-Runtime.ps1 fetches the pinned SDK for private testing
and checks archive and DLL SHA-256. Do not redistribute that assembled runtime
until corresponding-source/license obligations have been resolved. The existing
separate official LibVLC runtime, attribution and licenses remain in the package.
