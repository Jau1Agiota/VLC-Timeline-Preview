# Stage 2.4.5: thumbnail latency

## Status and scope

Based on merged master `cc257988d8`, including the Windows-tested Stage 2.4.4
source `e879ce656c218eddec13926f03b5f0615d14dc61`. Native Windows visual and
performance acceptance for Stage 2.4.5 is pending. No older build, ZIP or Release
is replaced. The 300 ms visible-preview target is aspirational.

The user's separate Windows executions for their unavailable Punisher MP4 at
30,000 ms reported capture 562 ms, helper total 703 ms, and PowerShell wall time
approximately 800 ms; observed hovering was 1–2 seconds. These are **not one
synchronized trace**, nor measurements of the synthetic CI fixtures.

## Architecture and instrumentation

Before: 150 ms trailing debounce, a shared 32-entry/8 MiB LRU, one new private
helper process/LibVLC/player per cache miss. After: 35 ms trailing debounce and
one shared 512-entry/256 MiB LRU ceiling across windowed/fullscreen timelines.
A maximum-size ARGB frame costs 230,400 bytes; 512 such frames account for
112.5 MiB of image storage, below the independent byte ceiling. Key/container
metadata and temporary image copies are additional. No prefetch queue exists.
Same-bucket hover events do not restart extraction, and cache hits present
synchronously without debounce. One process is globally active, one latest
request is pending, and superseded active work is killed and reaped before replacement.
A private worker now retains **only the LibVLC instance** between requests.
Each request creates and releases its media/player/decoder and drains callbacks
before sending JSON. Accurate seeking and the original file-cache/hardware
settings remain the default: Phase A did not justify fast seek or zero caching.
No decoding or player state is reused across timestamps.

The worker takes the media and output paths as separate Unicode argv fields;
stdin contains only `request_id target_ms timeout_ms` (bounded to 127 bytes).
There is one request in flight and no worker-side request queue. JSON IDs and
controller generations both reject stale results. Idle workers survive ordinary
mouse leave/cache hits, but active leave cancels by killing the process.
Media/source changes, crashes, bad output or timeouts restart from a fresh
instance. The GUI kills idle workers after 30 s, the helper independently ends
after 120 s or 64 requests, and final instance release has a 5 s watchdog.
Shutdown retains the existing bounded private-process teardown; no new GUI wait
is introduced. A cold process still pays initialization; only subsequent
uncached buckets in that worker amortize it. Fullscreen/windowed handover keeps
one global private process, restarting it when the owning controller changes.
Existing job-object containment (512 MiB), 5 s helper budget, 6.5 s controller
deadline, source-identity checks and failure backoff remain in place.

The packaged helper is compiled from
`prototypes/stage_2_3/thumbnail_helper.c` against the official 3.0.24 SDK import
library, then bundled with its **separate complete official runtime**. It never
uses the active player's handles or seeks the foreground input. The Stage 2.4.4
popup geometry, transient ownership and timestamp rendering are unchanged.

Helper JSON preserves every original field, including
`frame_timestamp_verified:false`, and adds millisecond durations:
`libvlc_init_ms`, `media_player_create_ms`, `play_call_ms`, `decode_wait_ms`,
`stop_release_ms`, `bmp_output_ms`, `entry_to_result_ms`. Creation/play/stop sums
include the existing bounded near-end retry when used. `play_call_ms` measures
the API call, not completion; `decode_wait_ms` includes demux/decode/scheduling.
`capture_latency_ms` remains the original helper-start-to-callback measure.
Windows GetTickCount64 resolution can quantize short phases; zero is not proof
that a phase has no cost. The parent records process wall time; subtracting
entry-to-result only estimates combined loader/launch/exit/transport overhead,
not isolated process startup.

`VLC_TIMELINE_HOVER_DEBUG=1` enables controller performance records through VLC's
existing debug logger: miss/hit, generation/bucket, debounce, request-to-launch,
request-to-present, helper total, supersession and cancellation. Logging is quiet
by default. Request-to-present ends after the synchronous tooltip update
callback; it does **not** measure Windows compositor/paint completion. No frame
PTS is inferred from player time, callback arrival or successful BMP output.

## Benchmark methodology

`prototypes/stage_2_3/benchmark.py` runs identical files/timestamps against five
explicit profiles, always in a fresh helper process:

| Profile | Private LibVLC configuration |
| --- | --- |
| baseline | Existing accurate seek, default file caching/hardware selection |
| low-cache | Accurate seek, `--file-caching=0` |
| fast-seek | Zero file caching plus `--input-fast-seek` |
| hw-none | Accurate/zero file caching, `--avcodec-hw=none` |
| hw-auto | Accurate/zero file caching, `--avcodec-hw=any` |

These options exist in this checkout: `src/libvlc-module.c` declares
`input-fast-seek` (default false) and `file-caching` (default 1,000 ms);
`modules/codec/avcodec/avcodec.c` declares `avcodec-hw` (default `any`). A cache
setting is not a measured delay. The hardware profile requests a path; it does
not prove which decoder was selected. LibVLC's own
`include/vlc/libvlc_media_player.h` warns that memory-video callbacks can disable
hardware acceleration or require GPU-to-CPU copies. No NVIDIA/RTX performance
claim can follow from Wine on a GitHub-hosted CPU runner.

Generate the existing burned-in-PTS fixtures, then benchmark the original profiles and persistent architecture (new output paths):

```powershell
py prototypes\stage_2_3\generate_fixtures.py C:\VLC-Stage245\fixtures --font C:\Windows\Fonts\arial.ttf
py prototypes\stage_2_3\benchmark.py --helper C:\VLC-Stage245\portable\thumbnail-preview\thumbnail-helper.exe --fixtures C:\VLC-Stage245\fixtures --output C:\VLC-Stage245\timings-run1.json --samples 6 --all-fixtures --profiles baseline --persistent
```

Adjust `portable` to the extracted package root. Requires existing Python,
FFmpeg/libx264/AAC/drawtext and ffprobe on PATH; no codec-pack download is needed.
For the profile comparison, omit `--profiles baseline --persistent` and use a
new output JSON. Use `--profiles baseline --persistent` to compare fresh processes
with the chosen persistent accurate configuration. Worker samples separately
label the first request of each process and subsequent requests; only the latter
amortize initialization. The worker benchmark's final EOF also tests clean
instance release. Targets cycle through 1,000/4,750/8,000 ms. Full mode covers ordinary, B-frame,
300-frame-GOP and VFR MP4/MKV. Files and targets are identical between profiles.
Profiles run sequentially, so filesystem/plugin/OS caches are **not controlled**;
the first sample is identifiable but is not a repeatable cold-disk experiment.
Median and nearest-rank p95 use the recorded sample count. Repeat after reboot
for a separately labelled cold first run. On the target PC also benchmark the
user's real clip and manually record request-to-visible latency.

Full-mode records SHA-256 of each BMP before deleting it. Byte agreement with
the baseline is a regression comparison, **not** independent timestamp truth.
Visually read the burned-in PTS and retain the existing Stage 2.3 scoring/review
workflow before any accuracy claim. Fast-seek gains cannot justify an unreviewed
accuracy reduction.

## Measurements and decision

Helper-only experiment: [Actions run 39843296241](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39843296241),
source `9c8602e9ef700262450578fffbfc531270452267`. All 60 candidates completed. The build/benchmark step succeeded; the existing
separate final resource step failed. No VLC application was rebuilt in this run.
Each profile has 12 samples (ordinary MP4/MKV, six each):

| Private helper profile | Capture median / p95 ms | Total median / p95 ms | Initialization median ms |
| --- | --- | --- | --- |
| Accurate baseline | 528 / 567 | 616 / 664 | 395 |
| Accurate, zero file cache | 522 / 608 | 560 / 641 | 394 |
| Fast seek, zero cache | 542.5 / 606 | 573.5 / 648 | 381.5 |
| CPU requested, zero cache | 526.5 / 587 | 558.5 / 619 | 392 |
| Hardware auto requested, zero cache | 529 / 655 | 562.5 / 689 | 389.5 |

Environment: GitHub Ubuntu runner, pinned image/GCC 6.4.0 and official Windows
runtime under Wine, four-CPU quota / 12 GiB container limit. This first version
did not capture the CPU model. Process wall medians were approximately 5.0–5.1 s
under Wine (first baseline p95 12.7 s), far above in-helper times. They include
Wine/loader/prefix/exit transport effects and are **not native Windows startup
measurements**. Phase B initializes the Wine prefix separately and measures
persistent responses rather than waiting for a fresh Wine client to exit.
The machine-readable summary is committed in
`prototypes/stage_2_3/evidence/stage245-phase-a-summary.json`; raw records remain
in evidence artifact `vlc-ci1-evidence-39843296241-1` (ID 12453479622).

Initialization dominates the measured helper cost (395 ms median vs 118 ms
median decode wait). File-cache reduction saved mainly shutdown time, while
capture scarcely changed. This supports retaining the instance rather than
changing seeking/buffering defaults. No NVIDIA acceleration is verified.

Local offscreen Qt controller tests with a fake decoder: 12 uncached requests,
median 41 ms / p95 44 ms; 12 cached requests, median/p95 0 ms at millisecond
resolution. This tests debounce/process scheduling and cache mechanics on the
cloud host, **not real video or Windows visual performance**.

The selected persistent-instance worker avoids those repeated initialization
costs without reusing seek/decoder state. Cancellation uses process termination,
not cooperative seeking or an unbounded queued cancel acknowledgement. The
standalone one-shot CLI is preserved for comparison and rollback. The first Phase B run
[39846291221](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39846291221)
completed the helper benchmark before cancellation to include the later GUI
idle-protocol guard. Its helper source is byte-identical to the final source.
Hardware: AMD EPYC 9V45, four logical CPUs, Linux/Azure, Wine, 12 GiB container
limit. The separate Wine-prefix setup removed the huge prefix/exit artifacts
from the initial experiment.

| Phase B workload | n | Median ms | p95 ms |
| --- | ---: | ---: | ---: |
| One-shot capture | 48 | 543 | 600 |
| One-shot helper total | 48 | 612.5 | 693 |
| One-shot process wall | 48 | 708.5 | 788.8 |
| Worker first request, response wall | 8 | 678.4 | 728.7 |
| Worker warm capture | 40 | 116 | 172 |
| Worker warm helper total | 40 | 157.5 | 246 |
| Worker warm response wall | 40 | 158.1 | 246.7 |

All eight cold and 40 warm worker BMPs were byte-identical to their corresponding
one-shot candidates across ordinary/B-frame/long-GOP/VFR MP4/MKV. Initialization
was 0 ms for all warm requests. These are unverified-PTS candidates, not an
accuracy-gate pass. Warm samples exclude the first 1,000 ms request in each
worker, so aggregate groups have different target weights; do not treat this
as a paired percentage-speedup estimate. Every hash comparison itself is paired
by file and sample index. Raw summaries are in
`prototypes/stage_2_3/evidence/stage245-persistent-summary.json`.

Real Windows first-visible performance, cached painting, rapid-movement latency,
repeated foreground seeks and two-video switching latency remain unmeasured.
The Qt suite tests those scheduling/lifecycle transitions with a fake decoder;
it does not substitute for physical-device performance tests. Cold process
startup remains above the aspirational target in CI. No prewarming is performed.
Superseding an active request kills the worker and loses its warm initialization;
rapid movement can therefore end with a cold restart. This deliberately preserves
latest-request priority and avoids unverified cooperative decoder cancellation.
Only completed-request reuse earns the measured warm benefit.

## Validation and delivery

Available local results: pinned MinGW-w64 GCC 6.4.0 compile **and final link** of
the instrumented helper against official 3.0.24 `libvlc.lib`; 21 prototype unit
tests; Qt offscreen controller/tooltip regression suite. 42 CI safety/packaging/finalization tests; Bash syntax, ShellCheck 0.10.0 and
actionlint 1.7.7. No failures remain in the final local runs. The unchanged
Stage 2.4.3 ZIP still hashes to its original SHA-256. Full Windows cross-build,
packaging and independent readback are running under
[39847212055](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/39847212055),
application source `d10df7f0a54326f122f54b2a2961a3e1b0ee1f66`.

The pinned Wine 8 `wineserver64 -k` was separately reproduced locally returning
silent exit 1 with no server, while `-w` returned 0. Finalization now accepts that
specific silent status only if a subsequent 5-second-bounded `-w` confirms
shutdown. Nonempty diagnostics, other statuses and wait timeouts still fail
teardown separately without masking the original build failure. Three focused
regressions cover these cases. A helper-only real-run check of the corrected
reporting driver is running separately; the already-running full-build driver
predates this reporting-only correction.

CI retains manual dispatch, read-only repository permissions, exact image/contrib
and SDK pins, the unchanged 35 GiB gate and cleanup allowlist. Stage 2.4.5 has a
helper-only Phase A mode (no VLC rebuild/package) and a full-build mode. Both use
fresh checkouts, private Wine state and run-specific paths. ZIP delivery remains
an Actions artifact with manifest/member hashes, SHA256SUMS, ZIP CRC and separate
Actions download/readback verification; no Release publishing exists.

## Windows device acceptance and rollback

Extract to a new directory. Preserve Stage 2.4.4 as the rollback package. Launch:

```powershell
$env:VLC_TIMELINE_HOVER_DEBUG='1'
& .\vlc.exe -vvv --file-logging --logfile="$PWD\stage245-hover.log" 'C:\Videos\sample.mp4'
```

Test windowed and fullscreen thumbnails/timestamps; left/right and negative
monitor edges; seeking, dragging, keyboard controls; toolbar auto-hide;
pause/resume and uninterrupted audio/video. Record first hover, repeated
uncached buckets, cache hits, rapid movement, repeated seeks and switching two
videos. Test helper rename/failure, timeout, close during extraction and verify
no orphaned helpers. Record target PC hardware, media hashes, n, median/p95,
separate cold/warm conditions and burned-in PTS review. Log files contain paths
and media metadata; review before sharing. Cached images retain the bucket's
timestamp label; callback PTS is still unavailable.

Rollback: close Stage 2.4.5 and launch the preserved Stage 2.4.4 portable directory.
No user preferences, foreground decoding settings or existing packages are
rewritten by this stage. Reverting this branch's controller/helper changes and
building the previously pinned Stage 2.4.4 source restores its build behavior.

Retain optimized BMPs for visual PTS review with the existing matrix/scorer:

```powershell
.\prototypes\stage_2_3\run_matrix.ps1 -Helper C:\VLC-Stage245\portable\thumbnail-preview\thumbnail-helper.exe -Fixtures C:\VLC-Stage245\fixtures -Results C:\VLC-Stage245\accuracy-lowcache-run1 -Profile low-cache
```

Use a different results directory for `-Profile baseline`. The baseline default
retains the old helper argument list. The PowerShell profile extension is
reviewed source; native PowerShell execution remains pending Windows testing.
