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
request is pending, and superseded work is killed and reaped before replacement.
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

Generate the existing burned-in-PTS fixtures, then benchmark (new output paths):

```powershell
py prototypes\stage_2_3\generate_fixtures.py C:\VLC-Stage245\fixtures --font C:\Windows\Fonts\arial.ttf
py prototypes\stage_2_3\benchmark.py --helper C:\VLC-Stage245\portable\thumbnail-preview\thumbnail-helper.exe --fixtures C:\VLC-Stage245\fixtures --output C:\VLC-Stage245\timings-run1.json --samples 12 --all-fixtures
```

Adjust `portable` to the extracted package root. Requires existing Python,
FFmpeg/libx264/AAC/drawtext and ffprobe on PATH; no codec-pack download is needed.
Targets cycle through 1,000/4,750/8,000 ms. Full mode covers ordinary, B-frame,
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
source `9c8602e9ef700262450578fffbfc531270452267`. Results pending at this document
revision; no extraction profile has yet been selected from unmeasured estimates.

Local offscreen Qt controller tests with a fake decoder: 12 uncached requests,
median 41 ms / p95 44 ms; 12 cached requests, median/p95 0 ms at millisecond
resolution. This tests debounce/process scheduling and cache mechanics on the
cloud host, **not real video or Windows visual performance**.

A persistent worker could amortize process loading and LibVLC initialization.
It would also require a bounded request protocol, cancellation acknowledgements,
watchdog/restart policy and explicit decoder draining across media switches.
The phase measurements must establish whether that amortization is worth the
additional lifetime/PTS-association risk; no decoder reuse is assumed safe.

## Validation and delivery

Available local results: pinned MinGW-w64 GCC 6.4.0 compile **and final link** of
the instrumented helper against official 3.0.24 `libvlc.lib`; 18 prototype unit
tests; Qt offscreen controller/tooltip regression suite. Final full Windows
cross-build, packaging and independent readback will be recorded here.

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
