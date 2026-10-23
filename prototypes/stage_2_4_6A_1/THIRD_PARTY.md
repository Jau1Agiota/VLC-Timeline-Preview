# Third-party provenance and distribution boundary

MPC-QT source reviewed at d2d0d15de6e9113df19a4d6e539897de2227a16d:
`src/widgets/videopreview.cpp`, `src/mpvwidget.cpp`, and its GPL version 2 LICENSE.
The reference constructs a separate paused MpvObject, disables audio/subtitles,
sets hr-seek=no, uses absolute+keyframes and an OpenGL widget. No implementation
code was copied. This program is an independent GPL-2.0-or-later implementation
of that concept, using the public API. It is not an MPC-QT fork or affiliation.

Public client.h/render.h/render_gl.h at mpv commit
`e471496c56f0f49c133bd88b50aed62b845f1909` carry ISC notices, including the mpv
developers' copyright and warranty disclaimer. Build headers are obtained intact
in the pinned SDK; their license does not relicense the mpv implementation.

The reputable, identified provider is shinchiro/mpv-winbuild-cmake, release
20261021, generic x86_64 development asset (not the x86_64-v3 variant). GitHub's
release API supplied the SHA-256, and downloaded bytes plus the extracted DLL
were independently hashed. See dependencies.json for exact names/hashes.
Nightly source version is pinned by commit; runtime mpv-version, ffmpeg-version
and mpv-configuration are recorded rather than substituting the latest stable
tag. Direct mpv.io installation-page access returned HTTP 403 in this environment;
we do not claim that page was verified here. The provider's public build recipe
and long-standing named distribution are inspectable on GitHub.

mpv defaults to GPLv2+, with LGPLv2.1+ possible only for an appropriately configured
build excluding GPL-only files. FFmpeg's license also depends on enabled codecs
and libraries (GPL options can make the combined DLL GPL). This selected binary
must not be assumed LGPL. Its archive has headers, import library and DLL, but
no complete corresponding source bundle or license/notice bundle. Our ZIP
therefore DOES NOT redistribute the DLL or its import library. The private-use
setup script downloads the exact upstream SDK and verifies hashes on the user's
machine. Do not redistribute that assembled folder without auditing the actual
build configuration, notices and complete corresponding source for mpv, FFmpeg
and all statically linked dependencies. Upstream URLs alone are not a claim that
we have fulfilled GPL binary-redistribution obligations. No written source offer
is invented. A distributable all-in-one VLC/libmpv package is a separate gate.

The experiment ZIP includes our own complete C source, build script and GPL text.
OpenGL/Win32 APIs are system interfaces. No Qt, codec pack, VLC DLLs or untrusted
DLL mirrors are required by the program. A future LGPL-only build and its full
source/provenance could change packaging options, but is not asserted here.
