# Stage 2.4.8D private local prototype dependencies

The local assembler copies the user's supplied original libmpv DLL, never a
replacement build. Required SHA-256:
`04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`.
The DLL is placed beside the unchanged workers under thumbnail-mpv/. Startup
verifies it against this fixed pin, not merely a user-editable dependency version.

Original provenance remains shinchiro/mpv-winbuild-cmake release 20261021,
`mpv-dev-x86_64-20261021-git-e471496c56.7z`; archive SHA-256
`efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633`.
mpv revision e471496c56f0f49c133bd88b50aed62b845f1909 and FFmpeg
N-127893-g9a861e968. These are provenance records, not download instructions.
No runtime retrieval/extraction/installer is part of offline startup or assembly.

A loadable Windows x64 vulkan-1.dll is a system prerequisite. Compatible official
PC/graphics drivers commonly provide it, but not every Windows installation does.
The loader check does not enumerate GPUs or require a Vulkan-capable GPU. This
prototype does not bundle a loader, modify Windows or install/download drivers.
Missing dependencies produce a diagnostic; never obtain unidentified loose DLLs.
Windows x64/PowerShell 5.1/WinForms and a writable application folder are required.

VLC and its separate official helper runtime, plugins, notices, and existing
worker executables are copied intact from the input Stage F package after file
hash verification. Preserve COPYING.txt, AUTHORS.txt, THANKS.txt, helper notices
and per-file attribution. The old Stage F build manifest is retained as
BASE_STAGE_F_BUILD_MANIFEST.json; the new OFFLINE_PROTOTYPE_MANIFEST.json describes
this private layout, its complete payload hashes and its local-only status.

Existing original-runtime corresponding-source/static-dependency/notice gaps
remain recorded in docs/original-runtime/REVIEW.md in the repository. GPL itself
is not a redistribution prohibition, but this private prototype does not declare
those distribution obligations fulfilled. Do not upload/publish the assembled ZIP
or populated folder. Public packaging guards remain unchanged. No replacement
runtime or new source/licensing investigation is part of Stage D.
