# 04 — Official contribution path and proposed workflow

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

## Confirmed official rules

1. **Confirmed:** [README.md](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/README.md#L97) directs contributions to **Merge Requests on
   VideoLAN GitLab**, not the GitHub mirror. It says CI and discussions should be
   resolved before merge. No MR, issue, comment, email or review reply was submitted
   to VideoLAN during this audit.
2. **Confirmed:** canonical project API (project 435) and Git remote HEAD identify
   `master`. Its inspected source is 4.0.0-dev / Qt 6. **Inferred:** use that
   development branch, subject to maintainer agreement; the 3.0.24 fork is
   proof-of-concept evidence, not the default basis for a new feature.
3. **Confirmed:** [doc/VideoLAN_AI_Policy.md](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/doc/VideoLAN_AI_Policy.md#L14) states:
   > Do not let an AI agent submit anything to VideoLAN for you, even with your approval.
   This explicitly includes issues, MRs, comments, review replies and proposals.
   A future design discussion must therefore be authored and submitted by the human.
4. **Confirmed:** the same policy requires the contributor to read, test,
   understand and vouch for every submitted line, including reports and descriptions.
   Coding AI assistance must be disclosed via `AI::Assisted` **or** description
   mention. It says not to put this information in commit messages or credit AI
   tools as co-authors. Keep submissions precise and concise.
5. **Confirmed:** [doc/standalone/buildsystem.md](https://code.videolan.org/videolan/vlc/-/blob/2556a9abdfc491d77d0cd96388f80153b4ae3910/doc/standalone/buildsystem.md#L222) describes registering
   Meson tests through `vlc_tests`, including module dependencies. Current test
   names must come from actual build definitions, not the stale thumbnail example.

## Rules not established by this audit

**Unverified:** a complete current formal coding-style guide, mandatory sign-off,
CLA/DCO, commit subject length or compulsory author-name format. No universal
requirement was established from the inspected repository. This is not proof
that no additional rules apply.

Requests to `https://wiki.videolan.org/Sending_Patches_VLC/` and the tested
`docs.videolan.me` pages failed through this environment's proxy with
`CONNECT tunnel failed, response 403`. The contents/existence of those attempted
documentation endpoints were not established. Do not silently substitute remembered
mailing-list-era instructions as current policy. The human should verify the
linked [Developer's Corner](https://wiki.videolan.org/Developers_Corner) and
[Hacker Guide](https://wiki.videolan.org/Hacker_Guide), plus account/submission
requirements, before an official proposal. Public GitLab MR notes endpoints for
!7822/!7493 returned HTTP 401; their complete review discussions were unavailable.
No credentials were created or borrowed to bypass access checks.

**Inferred from source/history, not a formal mandate:** use module-prefixed,
focused commits (`qt:`, `preparser:`, `test:`), describe why and validation, follow
the touched C/C++/QML files' style, retain notices, and separate functional work
from packaging/cosmetics. Upstream Meson selects C17/C++17 and Qt 6; our old Windows
image/compiler is not evidence of current-master compatibility. Confirm the
reviewers' expectations for both Autotools and Meson registration when new files
or tests affect both existing build systems.

## Existing work to read before discussing a design

**Confirmed:** [#29393](https://code.videolan.org/videolan/vlc/-/issues/29393)
contains the core/multiple-source direction, POC references and a discussion of
Qt-driven generation that did not fit. [medialibrary #493](https://code.videolan.org/videolan/medialibrary/-/issues/493)
records related resource/default-policy concerns. [#1251](https://code.videolan.org/videolan/vlc/-/issues/1251)
is an open request. [!7493](https://code.videolan.org/videolan/vlc/-/merge_requests/7493)
is an open macOS implementation proposal; [!7822](https://code.videolan.org/videolan/vlc/-/merge_requests/7822)
is closed and unmerged. These are existing records, not invitations or approval
for our implementation. The issue text's “cannot fit” observation describes that
prior proposal; no maintainer has reviewed ours.

**Inferred:** a short human-led design discussion is advisable before coding an
upstream series, because a GUI-private thumbnail service could duplicate the
ongoing core preview design. Ask how on-demand local extraction can serve that
provider model, whether decoder reuse belongs in preparser/core, and the intended
accuracy, CPU/memory and platform scope. Do not claim native VLC currently matches
MPC-QT or the prototype's perceived responsiveness. Do not send these five reports
unreviewed as an official proposal.

## Proposed human-owned development sequence — not executed

1. **Inferred:** review and correct this audit, verify rights/attribution and current
   contribution rules. Recheck issue/MR state and upstream HEAD; do not reuse stale
   request-ID APIs from older proposals.
2. **Inferred:** approve a separate native-only benchmark experiment on the pinned
   master. The smallest experiment is in [02](02_NATIVE_VLC_FEASIBILITY.md), without
   GUI integration or rewriting our libmpv implementation. Retain raw data and
   clearly distinguish decoded image, callback receipt and physical presentation.
3. **Inferred:** once the human can explain the results, submit a concise design
   discussion themselves, referencing existing work and disclosing relevant AI
   assistance. No agent should post it, including after a later approval.
4. **Inferred:** develop a new upstream branch from the agreed current revision.
   Keep our Stage F branch/artifacts as references, not a mass cherry-pick. Select
   existing native APIs first; discuss a core persistence extension only with
   evidence of a latency bottleneck.
5. **Inferred:** submit small independently reviewable patches: contract/tests,
   optional native provider changes, Qt bridge, QML UI and lifecycle tests. Discuss
   cross-platform UI/thumbnail source behavior with the relevant owners; macOS
   Cocoa frontend work is distinct from Qt on Windows/Linux.
6. **Inferred:** the human runs relevant upstream CI/local tests, reviews every
   line and handles review replies themselves. A timeline preview requires real
   Windows/Linux GUI and fullscreen/DPI checks; other platform support and format
   limits should be explicit. Resolve CI/discussions before requesting merge.

**Unverified:** maintainers' preferred provider API, feature scope/defaults,
performance thresholds, reviewer availability and acceptance. No sign-off is added
on anyone's behalf and no branch/MR/release was created on VideoLAN.
