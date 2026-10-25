VLC Timeline Preview - Stage 2.4.8E private offline prototype

Extract the entire ZIP into a new writable folder. Double-click
Start VLC Timeline Preview.exe, then open a local MP4 or MKV in VLC.
No first-launch runtime download, installer, administrator privileges or manual
PowerShell command is required. Keep the folder intact and writable.

The original verified libmpv DLL and unchanged preview workers are included by
your LOCAL assembly, with exact seeking by default. Do not use a different DLL.
A loadable Windows x64 vulkan-1.dll remains a system prerequisite. Official,
compatible graphics drivers commonly provide it, but availability is not universal.
Startup does not query GPU capability, install drivers or download dependencies.
Use the startup error message and your official PC/graphics vendor if unavailable;
never fetch loose DLLs. No Vulkan-capable GPU is required by this startup check.

This is a private local test package, NOT cleared for public redistribution.
Do not upload this ZIP or its populated folder. The original source/licensing
review remains open. See portable/DEPENDENCIES.md and the repository Stage D report.
This unofficial VideoLAN modification is not an official VLC/VideoLAN release.

Close before moving/unplugging the folder. Application data is private to this
folder where practical; USB launch may stage verified workers in local TEMP.
Media on removable/network drives remains outside current thumbnail support.
No universal frame-accuracy or frozen-playback fix is claimed.

OPTIONAL WINDOWS INTEGRATION
Double-click Register as Video Player.cmd to register VLC media player for your
Windows user. Defaults do not change automatically: select it in Open with or
Windows Settings > Apps > Default apps. Remove Registration.cmd removes only
this folder registration. Remove it before moving/deleting the folder.
Explorer file requests open separate portable sessions; launching without a
file focuses the most recently launched VLC from this folder when available.
Official VLC is never targeted by the portable launcher.
