# 03 — License, provenance and contributor review

Audit date: 2026-10-25. Read-only research; no application builds, runtime
experiments, production edits or VideoLAN submissions. Baseline Stage F:
`31c583d5f83f18e06d9853d6e59adca26c1f68e9` (PR #20).
Upstream inspected directly from `https://code.videolan.org/videolan/vlc.git`:
`master` at `2556a9abdfc491d77d0cd96388f80153b4ae3910`.

**Confirmed** means directly supported by inspected source, public API metadata,
or explicitly identified existing evidence. **Inferred** means an architectural
judgment, not a maintainer decision. **Unverified** means an open question or
experiment not performed. Issue/MR states are observations on the audit date,
not permanent statements. Source links below are revision-pinned; issue links
remain live.

## What is known about source origin

| Material | Finding and evidence | Classification |
|---|---|---|
| Existing slider/tooltip | Original GPL-2.0-or-later headers remain: [modules/gui/qt/util/input_slider.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/input_slider.cpp#L1) credits Clément Stenac, Jean-Baptiste Kempf and Ludovic Fauvet; [modules/gui/qt/util/timetooltip.cpp](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/modules/gui/qt/util/timetooltip.cpp#L1) credits VideoLAN and Ludovic Fauvet. | **Confirmed**; preserve these notices and history. |
| Project additions | Controller/geometry/workers/portable scripts declare GPL-2.0-or-later; `seekhover.hpp` credits 2026 VLC Timeline Preview contributors. Git history separates integration, exact mode, geometry, visuals and packaging. | **Confirmed** declarations; complete rights/derivation verification remains **Unverified**. |
| Core target | Native preparser and its tests carry LGPL-2.1-or-later headers; Qt UI files carry GPL-2.0-or-later. See [src/preparser/internal.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/src/preparser/internal.c#L1) and [test/src/preparser/thumbnail.c](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/test/src/preparser/thumbnail.c#L1). | **Confirmed**; root COPYING alone is not each file's license. |
| MPC-QT reference | Project [prototypes/stage_2_4_6B/THIRD_PARTY.md](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/prototypes/stage_2_4_6B/THIRD_PARTY.md) declares no MPC-QT implementation copied. Pinned reference is `d2d0d15de6e9113df19a4d6e539897de2227a16d`. | Declaration **Confirmed**; absence of all adaptation **Unverified**. |
| libmpv API | Public client/render headers at `e471496c56f0f49c133bd88b50aed62b845f1909` carry ISC notices; worker consumes them from the pinned SDK. | **Confirmed** provenance record; ISC headers do not relicense the DLL. |
| Tests/media | Synthetic fixture scripts and [prototypes/stage_2_4_6C/FONT_NOTICES.txt](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/prototypes/stage_2_4_6C/FONT_NOTICES.txt) document DejaVu/Bitstream font provenance. Private user movies/recordings are not proposed upstream fixtures. | **Confirmed** records; each asset/redistribution still requires human checking. |

**Confirmed:** the MPC-QT [LICENSE](https://github.com/mpc-qt/mpc-qt/blob/d2d0d15de6e9113df19a4d6e539897de2227a16d/LICENSE)
is GPL version 2 text. Its [mpvwidget.cpp](https://github.com/mpc-qt/mpc-qt/blob/d2d0d15de6e9113df19a4d6e539897de2227a16d/src/mpvwidget.cpp)
explicitly says portions came from mpv examples and were reworked. That is a
provenance warning for copying that file, not evidence our workers copied it.
Do not assume an “or later” grant from the license text alone, or assume example
code has the same grant as public headers.

**Confirmed, limited comparison:** normalized nonblank-line comparisons of our
B/C workers against those two pinned reference C++ files found maximum consecutive
identical blocks of one/two lines, not a substantial identical implementation
block. The workers use Win32/SW rendering while MPC-QT uses Qt/OpenGL. This is
supporting evidence for independent implementation, **not** a plagiarism audit,
a legal clearance or proof that no snippets/examples were adapted elsewhere.
A/A.1 are historical Win32/OpenGL prototypes, declared independent; their example
and shader lineage must also be checked if any of that code is proposed for reuse.

## Three different obligations

### 1. Submitting source upstream

**Inferred:** a native implementation avoids libmpv binary obligations for the new
feature but still needs correct contributor rights and per-file licensing. Obtain
human confirmation of original authorship and any adapted snippets. Keep upstream
copyright headers; identify genuinely new authors/changes as maintainers require.
Commits currently include generic `John Doe` identity and collective attribution;
the contributor should confirm the intended identity and authority to submit.
This is not a verified rule requiring a particular form of legal name.

**Confirmed:** existing project additions are declared GPL-2.0-or-later, while core
preparser files are LGPL-2.1-or-later. **Inferred:** do not simply copy GPL-only
project additions or old GPL Qt code into LGPL core files and declare them LGPL.
Use a fresh implementation under the accepted module license, or obtain appropriate
permission from all relevant rightsholders. Ideas, test requirements and measured
behavior are distinct from copying implementation text. License-version and
linkage questions require human legal/maintainer review, not automatic relicensing.

**Unverified:** an applicable CLA/DCO/sign-off requirement beyond inspected source
policy; see [04](04_VIDEOLAN_CONTRIBUTION_PATH.md). No rights assignment, written
source offer or contributor consent is invented by this audit.

### 2. Existing downloadable binaries

**Confirmed:** Stage F's packaging recipe and metadata exclude `libmpv-2.dll`
and its import library; the packager additionally rejects a bundled libmpv DLL. First-launch setup retrieves the
identified publisher's SDK for private testing; a populated runtime folder is
not the original distributable ZIP. Source submission readiness does not itself
clear historical binary distribution obligations, including VLC/contrib, official
LibVLC runtime notices and any redistributed test assets.

Pinned SDK provenance is recorded in [prototypes/stage_2_4_6B/dependencies.json](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/prototypes/stage_2_4_6B/dependencies.json)
and [docs/STAGE_2_4_6F_PORTABLE.md](https://github.com/Jau1Agiota/VLC-Timeline-Preview/blob/31c583d5f83f18e06d9853d6e59adca26c1f68e9/docs/STAGE_2_4_6F_PORTABLE.md#L91):

- Publisher: `shinchiro/mpv-winbuild-cmake`, release `20261021`.
- Archive: `mpv-dev-x86_64-20261021-git-e471496c56.7z`.
- Archive SHA-256: `efc719eff3dc5954b4a5efc800632808bcec01cc9034940dd14e5eb7fe9ea633`.
- DLL SHA-256: `04e4d27dc385dc18ddd93fbe69c75e635b864f3c6ec53a9122df54c2f7842f4a`.
- Provider workflow `39518569492`, recipe commit `1f8d2e76255abc48f0456d9be99b4cab9c6566e6`.

**Confirmed from the pinned recipe / Stage F provenance record:**
[mpv recipe](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/packages/mpv.cmake)
uses static preference; [FFmpeg recipe](https://github.com/shinchiro/mpv-winbuild-cmake/blob/1f8d2e76255abc48f0456d9be99b4cab9c6566e6/packages/ffmpeg.cmake)
enables GPL and version 3. [Pinned mpv Copyright](https://github.com/mpv-player/mpv/blob/e471496c56f0f49c133bd88b50aed62b845f1909/Copyright)
explains default GPL and dependency effects. It is not an audited LGPL-only build.

**Unverified:** complete matching source for every static dependency, configuration,
patch, notice and applicable delivery/source-access obligation for that exact DLL.
A repository URL, hash or subprocess boundary alone does not fulfill GPL binary
obligations. Private download is not permission to publicly redistribute the
assembled runtime. This audit does not certify all existing VLC binary packages'
license compliance or change/publish any binary.

### 3. Eventual VideoLAN distribution

**Inferred:** retaining libmpv would add dependency provenance, license-version,
corresponding-source, security-update, platform packaging and distribution review.
GPL-2.0-or-later frontend code is not categorically incompatible with GPLv3
components, but that does not settle LGPL core use, mixed dependencies or each
platform's packaging requirements. Do not claim “all libmpv is LGPL” or “separate
process means licenses do not matter.”

**Inferred:** using existing VLC native decoders and preparser removes this new
second-engine dependency; it does not waive VLC's existing licensing obligations.
Stage F first-launch runtime fetching and independent product packaging should
not be proposed as VideoLAN's distribution strategy.

## Human review required before any official proposal

**Unverified / action items:** attest the source lineage of reusable additions and
any examples; confirm per-file licenses and author identity; keep ISC/font notices
where needed; exclude private copyrighted footage; confirm licensing of any new
core implementation; separately audit any future binary redistribution. No
production notices were edited in this stage.

**Confirmed — contribution responsibility, not a license grant:**
[doc/VideoLAN_AI_Policy.md](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/doc/VideoLAN_AI_Policy.md#L14) requires a human to understand and vouch for
submitted work, disclose coding assistance in the MR, and prohibits AI agents
from submitting to VideoLAN even with approval. It does not adjudicate copyright
ownership. All these reports and future code require that human review; this
stage only publishes documentation to the user's own repository.
