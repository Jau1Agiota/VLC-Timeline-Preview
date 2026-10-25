VLC Timeline Preview - Stage 2.4.6F portable Windows x64

Extract this entire folder to a writable location. Double-click
"Start VLC Timeline Preview.exe", then open a video through VLC's normal menu.
Do not run vlc.exe directly: it does not configure the preview backend.

First launch requires internet and downloads a checksum-pinned third-party
libmpv runtime from GitHub. Click OK and wait. Subsequent launches reuse it.
This is NOT a fully offline-portable ZIP. No administrator privileges, installer,
manual PowerShell commands or system-wide DLL installation are required.
Windows 10 1803+ / Windows 11 x64, inbox Windows PowerShell 5.1 and tar.exe are
required. GPU driver's Vulkan loader may be required by the pinned runtime even
though previews use software decoding. Errors appear in a dialog.

Keep all files together. Move/copy only when VLC is closed; do not unplug USB
storage while running. Path spaces are supported; read-only media is not.
Portable-data contains runtime, configuration, cache and temporary preview data.
The launcher sets these locations only for its VLC child, uses --no-one-instance,
and does not change the existing installed VLC. Windows/GPU/Qt may still use OS
caches. Windows security software can block unsigned experimental executables.
This ZIP and native launcher are unsigned. Do not bypass organizational policy.

Default: existing libmpv exact mode. Existing fast mode and diagnostics remain
available for developer testing through portable/Start-Portable.ps1 parameters
-SeekMode fast, -Diagnostics, -PlaybackDiagnostics. Ordinary use needs no scripts.
Stage E appearance and D geometry/diagnostics are unchanged. Playback-freeze
investigation remains on hold. Physical Windows acceptance remains pending.

Licensing/provenance: portable/DEPENDENCIES.md, thumbnail-mpv/dependencies.json,
thumbnail-mpv/THIRD_PARTY.md, COPYING.txt and BUILD_MANIFEST.json.
Do not redistribute the downloaded libmpv DLL until its corresponding-source
obligations have been resolved. Distribute the original ZIP, not the populated
portable-data directory. This is an experimental build, not a signed release.

Removable-storage detail: the unchanged preview controller requires helper and
media paths on fixed local drives (thumbnailpreview.cpp localPath). When the
application is on USB/removable storage, startup stages a checksum-verified
helper/DLL copy in the user's local fixed-drive TEMP, retains it for that VLC
process and removes it on exit (forced termination can leave that private temp
folder). The download cache stays in portable-data. Videos on removable/UNC
storage remain ineligible for previews, as in Stage E; ordinary playback works.
VLC's native portable-directory support (src/win32/dirs.c) stores its own profile
and Qt INI files under portable/; child APPDATA isolation supplements this.
Do not share a writable extracted folder between mutually untrusted users.

Vulkan prerequisite: the original preview runtime needs a Windows x64 loadable
vulkan-1.dll. Official compatible graphics drivers commonly provide it, but not
on every Windows installation. Startup checks loader availability without querying
GPU capability or installing/downloading drivers. If unavailable, follow the
reported Windows error and use official PC/graphics-vendor support; never obtain
loose DLLs from unidentified sites. Stage 2.4.8C is preparatory: an offline ZIP has
not been approved, and this existing edition still requires first-launch internet.
