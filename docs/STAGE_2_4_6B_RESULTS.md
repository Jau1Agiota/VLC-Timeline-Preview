# Stage 2.4.6B experimental integration results

The portable Windows x64 build and native synthetic tests passed. Physical
Windows Qt popup/audio/fullscreen/HEVC acceptance has NOT been performed.
Keep LibVLC as the default; libmpv remains an explicit opt-in experiment.

## Provenance and delivery

- Base: `feature/stage-2.4.5-thumbnail-performance`,
  `92102fca537ef36bb11c018146766bc2a656d8cf`.
- A.1 reference: `e9d79b96ae4af3691693c894d8b4e44b11a999d8`; only verified
  runtime setup and API concepts carried over, no standalone UI branch merge.
- Compiled VLC/worker source: `5cfebdb9718b3477e17c5ffcb6d14a2876974350`.
- Build driver: `6536f38c35fd49980b1d6b32aa334155328f2c48`.
- Build: [39987670316](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39987670316).
  Build/package/readback jobs passed; that run's first native comparison failed
  because the harness reused a BMP path across LibVLC CREATE_NEW writes (exit 4).
  The helper algorithm was untouched. The harness now consumes/preserves each
  candidate and removes only its own working BMP before the next request.
- Final native retest and independent image analysis:
  [39991618918](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39991618918),
  driver `29390a0f07469a8069f083509f05e436e2559c48`, all relevant jobs passed.
  It downloads the original verified ZIP; VLC is NOT rebuilt.
- [Portable artifact 12516713005](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39987670316/artifacts/12516713005).
  Inner ZIP: `VLC-3.0.24-stage246B-ci1-win64-5cfebdb9718b-39987670316-1.zip`,
  512,230,494 bytes; SHA-256
  `b3ffec9b91ccc6cd2180b05876deef6dec496644a432a80b75a3f26ddcf9dc00`.
- [Raw native results and 80 BMPs](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39991618918/artifacts/12516879654).
- [Raw image OCR observations](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39991618918/artifacts/12518835567).
- [Build logs/resources](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39987670316/artifacts/12517117477).
  Artifacts expire after 14 days. Complete Checks summaries are also preserved in
  `prototypes/stage_2_4_6B/evidence/ci-39991618918.json`.

## What actually passed

Strict worker compile AND final link with pinned MinGW GCC 6.4.0 locally and in
the official VideoLAN Docker build; full modified VLC/Qt plugin clean build with
Qt 5.6.3 and matching verified prebuilt contrib. PE32+, diagnostic/performance/
libmpv integration markers, separate official VLC helper runtime, complete ZIP
CRC/file hashes/manifest and independent Actions download readback passed.

Linux Qt5 real event-loop/subprocess tests use a fake decoder and cover existing
seek mapping, cache limits, both popup contexts, leave/drag/deactivation/media
invalidation, deadlines/crashes, cleanup and default LibVLC scheduling. New tests
cover persistent libmpv PID reuse, fragmented binary delivery, latest pending
requests, intermediate-image labels, newer cache-hit ordering, hidden results,
missing/unknown backend rejection and continuous hovering without empty images.
A 20-request rapid fake-hover burst produces fewer than six worker requests and
ends at the latest requested bucket. This is scheduling evidence, not a movie
benchmark or Windows GUI test. Follow-up rollback/burst tests passed locally;
the packaged application code is unchanged by those later tests.

Six framing/parser/harness unit tests pass. CI guard/packaging suite: 43 tests,
42 pass plus one intentional stage-specific skip. Bash syntax, ShellCheck 0.10.0
and actionlint 1.7.7 pass. The cleanup allowlist, 35 GiB disk gate and fail-closed
contrib guard are unchanged. This successful runner passed the gate; feasibility
on every future runner image is not established.

On native Windows Server 2025: 56 libmpv candidates (eight persistent processes,
seven distant requests each), 24 LibVLC comparison candidates (eight persistent
instances, three matching requests each), bounded corrupt/audio-only failures,
forced active-child termination and helper reaping passed. Exact invalid-input
exit codes were not retained in the report; tests require nonzero exit without a
candidate. Fresh packaged VLC launched/exited successfully using dummy interface,
video and audio outputs on a fixture. This does NOT exercise the Windows Qt popup,
physical audio, concurrent foreground playback isolation or monitor z-order.

## Measured synthetic performance

All media are **30-second 640x360 H.264**, ordinary/B-frame/long-GOP/VFR in MP4/MKV.
This is NOT 1080p Main10 HEVC, HDR, the user's movie, or an RTX 3060 benchmark.
Times below are request-to-complete-candidate receipt, not physical display.
First requests include remaining process/library/media initialization but exclude
Qt hover/debounce and do not represent a guaranteed cold operating-system cache.

| Workload | n per backend | libmpv median / p95 ms | LibVLC median / p95 ms |
| --- | ---: | ---: | ---: |
| First request per media | 8 | 67.75 / 691.38 | 223.12 / 1646.39 |
| Matched warm targets, 25 s and 5 s | 16 | 10.50 / 11.48 | 116.04 / 203.74 |

The complete libmpv warm set (48 requests, also 18/29.5/2/23 seconds) is
11.11 / 11.63 ms. LibVLC necessarily includes its BMP-write path; libmpv uses raw
pipe bytes. Neither includes Qt painting. Small n and runner startup variability
limit inference: an earlier identical-package retest had first p95 356.94 ms for
libmpv and 748.99 ms for LibVLC. Do not translate these into movie/UI promises.

SW render/scaling total: median 0.229 / p95 0.396 ms (56 candidates). Blocking raw
pipe write: 0.162 / 0.241 ms (48 prior-request measurements carried in the next
header; last write per media is unavailable). These small-input measurements do
not identify the HEVC bottleneck. Stage durations overlap; do not sum them into
an exclusive decoder-time estimate. GUI `copy_ms` covers QImage deep copy, cache
insertion and the present callback, not just memcpy or actual paint/scanout; no
real Windows GUI value has been measured here.

Maximum **sampled** libmpv private allocation: 35,741,696 bytes (~34.1 MiB), not
peak RSS or total mapped DLL memory. Per-process cumulative CPU at the seventh
candidate ranges 109.375–203.125 ms, including initialization; complete per-media
counters are in the JSON. No GPU utilization was measured. This backend explicitly
uses software decoding/rendering. Real HEVC CPU/GPU/playback resource contention
remains a required device test.

## Image-content accuracy: significant limitation

Independent OCR read visible burned-in timestamps from all 80 saved candidate
images and checked plausibility against source MP4 frame times (MKV is a remux).
These are **provisional automated observations**, with human image review pending.
Neither command acknowledgements nor render events are used as frame PTS proof.

| Backend | Images with plausible OCR | Median / p95 absolute request error | Within 250 ms |
| --- | ---: | ---: | ---: |
| libmpv | 56 / 56 | 1250 / 8000 ms | 2 / 56 |
| LibVLC | 24 / 24 | 33 / 200 ms | 24 / 24 |

The fast libmpv candidates are substantially less accurate in this experiment.
Coarse keyframe seeking is a known contributor, but these aggregate observations
do not establish the cause of every error or prove event-to-frame association.
Inspect the raw images before attributing all errors to GOP spacing. No timestamp-
accuracy gate has passed, even for LibVLC, without human review. The popup labels
libmpv images with their own approximate **request** time alongside current hover
time; it never claims a verified image PTS. Fast receipt is not equivalent to a
correctly timed thumbnail or perceived smoothness.

## Recommendation and stop condition

The opt-in integration is compiled and ready for **device investigation**, not
promotion to the default backend. Keep LibVLC available. Test the exact RTX 3060 /
1080p 10-bit HEVC movie, windowed/fullscreen hovering, audio continuity, dragging,
pause/resume, media switching, failure and shutdown, and inspect image timestamps.
No claim of MPC-QT parity or real-movie improvement is justified yet.

Known limits: first-hover lazy initialization; 1 GiB private helper job cap;
serialized in-flight seeks can delay the newest pending request; software HEVC
cost; coarse keyframes and unverified frame association; Windows DLL dependencies;
6.5 s controller deadline; 24-hour worker lifetime; a controller handover can
restart the helper. The package excludes libmpv until complete runtime/FFmpeg
corresponding-source obligations are resolved; private checksum setup is provided.

After user review, investigate timestamp association/seek accuracy if the observed
coarseness is unacceptable; do not silently substitute exact seeks or redesign the
engine during this stage. Prior builds/source branches remain untouched. No merge
has been performed. See STAGE_2_4_6B_INTEGRATION.md for exact setup and rollback.
