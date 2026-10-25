# VLC Timeline Preview

An experimental, unofficial modification of **VLC Media Player 3.0.24** for
Windows x64, adding native timeline-hover video previews.

**Not an official VideoLAN release.** VideoLAN and the VLC contributors created
VLC; this project is independently maintained and is not endorsed by VideoLAN.

> **Download status:** A fully offline, self-contained public package is not yet
> available. Stage 2.4.8 is blocked on runtime redistribution and dependency
> completeness. No Stage 2.4.8 ZIP or GitHub Release has been produced.

## Preview

<!-- Screenshot placeholder: windowed orange-glow thumbnail and timestamp. -->
<!-- Screenshot placeholder: fullscreen controller hover preview. -->

Screenshots will be added after acceptance; placeholders are not test evidence.

## Features

- Timeline-hover thumbnails without seeking the foreground playback instance.
- Persistent, isolated libmpv preview worker, bounded caching and asynchronous requests.
- Exact and fast seek modes; exact is the portable launcher's default.
- Fullscreen preview positioning and borderless orange-glow styling.
- Retained images keep their own requested timestamp while a newer image is pending.

A displayed timestamp is not independently verified frame PTS. Exact mode does
not establish frame accuracy for every media type.

## Download and use

There is **no approved offline public download** yet. Do not redistribute a Stage F
folder after its runtime cache has been populated.

For existing private testers, [Stage F instructions](docs/STAGE_2_4_6F_PORTABLE.md)
identify the earlier GitHub Actions artifacts (temporary retention, not permanent
release links). That edition requires internet on first launch. Extract into a
new writable folder and double-click **Start VLC Timeline Preview.exe**, then
open media through VLC's normal interface. No installer, elevation or manual
PowerShell command is needed. It does not satisfy offline-first-launch acceptance.

The intended future self-contained edition will use the same extract-and-launch
experience, without downloading a runtime. It remains gated by the
[Stage 2.4.8 licensing report](docs/STAGE_2_4_8_SELF_CONTAINED.md).

## System requirements and limitations

- Windows x64; Stage F targets Windows 10 1803+ / Windows 11, with inbox Windows
  PowerShell 5.1 and tar. A writable application folder is required.
- The pinned preview DLL imports Vulkan's loader even with software decoding;
  availability on a clean Windows installation has not been established.
- Initial preview support is local MP4/MKV on fixed drives. Network streams,
  removable-drive media, audio-only and non-seekable inputs may have no thumbnails.
- Long-GOP, HEVC, variable-frame-rate and unusual timestamp origins can affect
  latency or accuracy. Positive-origin timestamp discrepancies remain known.
- No physical Windows acceptance is claimed for Stage 2.4.8. Existing playback
  investigation is not resumed by this packaging stage.
- Portable application folders on removable media can stage verified workers in
  local TEMP; not all operating-system caches can be kept inside the application.
- Unsigned executables may trigger Windows trust prompts. Never obtain replacement
  DLLs from unidentified download sites.

## Architecture and development

VLC's Qt timeline sends passive hover requests to a bounded asynchronous
controller. Separate persistent libmpv worker processes decode previews; VLC's
foreground player remains responsible for normal audio and video playback. A
separate official LibVLC helper runtime remains available for the existing helper.
The Windows bootstrap resolves its script and dependencies relative to itself.

See [Windows CI](docs/WINDOWS_CI.md),
[portable implementation and CI evidence](docs/STAGE_2_4_6F_PORTABLE.md),
[exact-seeking investigation](docs/STAGE_2_4_6C_ACCURACY.md), and
[Windows acceptance checklist](docs/STAGE_2_4_8_SELF_CONTAINED.md#windows-acceptance--pending).
Builds use pinned source, toolchain and dependency hashes. Do not dispatch an older
workflow expecting it to produce a Stage 2.4.8 offline package.

Development progressed from VLC source/build verification, through an isolated
LibVLC extraction prototype, native hover detection and asynchronous UI integration,
to fullscreen diagnostics, performance experiments, persistent libmpv extraction,
exact seeking, positioning, styling and Stage F portable startup. The `docs/`
reports preserve the measured results and unresolved questions. No upstream
submission or endorsement is implied by this history.

Contributions should be small, preserve playback behavior and include relevant
verification. Open a draft PR in this repository; keep source licenses and notices,
identify borrowed material, and distinguish automated tests from physical Windows
results. Do not include private movies, populated runtime caches or build archives.

## Credits and licensing

Thanks to VideoLAN and VLC contributors; mpv, FFmpeg and their dependency authors;
MPC-QT for the interaction-model reference; the identified Windows runtime build
provider; and this project's contributors and Windows testers.

Original attribution remains in [AUTHORS](AUTHORS), [THANKS](THANKS) and source
headers. VLC contains components with different licenses; see [COPYING](COPYING),
[COPYING.LIB](COPYING.LIB) and individual notices. Public libmpv API headers use ISC;
that does not license the runtime DLL. The selected binary includes GPL/version-3
FFmpeg configuration, and its full corresponding-source and notice obligations
remain unresolved. This repository's license files alone do not clear redistribution
of an assembled Windows package. See the [licensing report](docs/STAGE_2_4_8_SELF_CONTAINED.md).

Runtime follow-up: [Stage 2.4.8A findings](docs/runtime-audit/RUNTIME_RESOLUTION.md)
record verified SDK imports and the remaining source/dependency gates. A
software-only replacement runtime is a proposal, not an available offline build.
