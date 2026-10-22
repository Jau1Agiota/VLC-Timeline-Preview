# Stage 2.3.1 — near-end candidate recovery

## Scope and decision

Branch: `investigate/stage-2.3.1-near-end`, based on prototype commit `73d6c8f455a64fb2a83482fc58ee226c387619a7`. VLC source remains 3.0.24 (release commit `6de05adcbaf2e8b85fe86aad4169393098628119`). Changes affect only the isolated helper, prototype tests and documentation. Neither the Stage 1 branch nor production VLC code is changed.

Add a narrowly gated, observable tail retry. This recovers **candidates**, not verified final frames. In the six focused Wine tests, the recovered images visibly read **00:00:29.833**, 117 ms before the 29,950 ms request; these are penultimate frames. Do not describe this as a guarantee of final-frame delivery or a completed accuracy gate. Native Windows confirmation and full visible-PTS review remain required before any timeline integration.

## Baseline and source investigation

The user's native Windows `C:\VLC-Stage23\results-run1` contains 168 attempts, 162 candidates and six `ended_without_video` failures: VFR MP4/MKV at 29,950 ms, three repetitions each, without external timeouts. That directory is unavailable in this cloud environment and was not modified. These native results and timing evidence are user-provided, not newly measured here:

| Property | MP4 | MKV |
| --- | ---: | ---: |
| Container duration, ms | 30000 | 30021 |
| Last frame PTS, ms | 29900 | 29921 |
| Estimated endpoint, ms | 29933.333 | 29954 |

The MKV failure is inside the final reported interval. Container duration alone cannot explain both failures.

The checked-out 3.0.24 source establishes these mechanisms:

- `src/input/input.c`, `StartTitle`: startup `:start-time` queues a seek. The helper selects accurate seeking with `--no-input-fast-seek`.
- `modules/demux/mp4/mp4.c`, `Seek`, approximately lines 1608–1650: accurate seeking sets `ES_OUT_SET_NEXT_DISPLAY_TIME` to the requested date.
- `modules/demux/mkv/matroska_segment.cpp`, `Seek`, approximately lines 895–909: accurate seeking sets `sys.i_start_pts` and the same next-display threshold.
- `src/input/es_out.c`, approximately lines 2079–2103 and 2628: preroll tracks the next-display threshold; packet duration participates in preroll handling.
- **`src/input/decoder.c`, `DecoderUpdatePreroll`, line 768, and `DecoderPlayVideo`, line 1008:** preroll-marked packets set the decoder boundary to `INT64_MAX`; an unmarked dated packet can lower it using DTS/PTS. Pictures before that decoder boundary are dropped, without considering picture duration. This boundary is **not simply copied from the requested time**. If the tail remains marked preroll, no picture need survive; but the two last-frame PTS values alone do not prove that occurred. In particular, `es_out.c` considers input block duration, which need not equal ffprobe's reported decoded-frame duration. `modules/demux/mkv/util.cpp`, `send_Block`, lines 374–377, assigns block length from demux duration only when permitted by the track. A final interval reported by ffprobe does not prove its VLC block passed preroll.
- `src/input/input.c`, `MainLoop`, waits for output emptiness before end; `src/input/decoder.c`, `input_DecoderIsEmpty`, calls `vout_IsEmpty`. **`src/video_output/video_output.c`, `vout_IsEmpty`, line 337, checks the decoder FIFO**, not every picture held for scheduled display. EndReached is therefore not a public guarantee that every final display callback has run. The helper stops immediately on that event, potentially canceling held output. This also limits tail recovery.
- `lib/media_player.c` maps input END_S to `libvlc_MediaPlayerEndReached`. `libvlc_video_set_callbacks` in `include/vlc/libvlc_media_player.h` still exposes no frame PTS.

The original helper waits for first display, error or EOF; only display copies an image. If EOF wins without any display callback, stop/release occurs and exit 3 is correct. It neither uses a fixed delay nor verifies callback PTS. Preroll retention and EOF/display ordering provide candidate explanations consistent with both observed failures; no instrumented native decoder trace proves which internal path dominated each original attempt. Codec/vout errors logged without a LibVLC EncounteredError event remain possible contributors.

A first experiment retried just 50 ms earlier (29,900 ms). **All six still failed under Wine.** That experiment is not the delivered algorithm. Merely subtracting an estimated frame duration does not ensure display delivery at EOF.

## Delivered policy

Normal successful extraction still copies the first displayed candidate and stops. Only an initial **EOF without any captured image**, without EncounteredError or timeout, can retry. Additional guards require a discovered video track, known positive container length of at least 250 ms, and a request no later than that length and within its final **100 ms**. Audio-only, unknown-length, past-end, non-EOF, explicit-error and timed-out attempts cannot enter recovery. The policy's boundary and failure exclusions have executable C tests.

Recovery releases the first media/player, then opens one new private media/player on the same private LibVLC instance at **container length minus 250 ms**. It retains the last delivered candidate until EOF, instead of completing on first display. Image replacement is serialized; callbacks perform no player control or disk IO. Stop/release precedes state reset and image access. Buffers remain bounded at 320×180, three aligned slots per negotiated format plus one owned image. The original process watchdog is not reset: both attempts, decoder startup and cleanup share the original overall budget.

The 100 ms eligibility window covers the two supplied endpoint gaps (50/71 ms) with a small explicit margin; it is a prototype boundary, not a stream-duration theorem. The 250 ms tail uses the architecture's proposed error budget as a bounded seek lookback and gives more than one candidate a chance to reach display. **Neither constant bounds actual returned PTS**: keyframe behavior, preroll and callback scheduling can still return a wrong candidate or none. This is why fallback is exposed and every image still requires visible review. Expanding these thresholds for other files needs new evidence.

CLI and original JSON fields are preserved; `requested_ms` always remains the original request and `frame_timestamp_verified` remains false. Added fields are `fallback_used`, `attempts`, `effective_start_ms`, `initial_length_ms` (0 if unknown), and `initial_extraction_status`. Recovered success has status **`fallback_candidate_captured`**, rather than ordinary `candidate_captured`. Failure remains nonzero; output is never overwritten. A retry with no display, an error or a timeout cannot be turned into success. This cannot detect every silent decode failure: a fallback candidate must not be interpreted as proof that the initial failure was harmless.

## Actual cloud verification

The exact GCC 6.4.0 MinGW toolchain was extracted from the verified layer of the pinned official image:

```text
registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7
compiler layer sha256: 72ffc864bced46cb695698eb90efcc92999c3ee4ae50138d5af094858fd4ed29
```

Both original and modified helpers compiled and **linked** with `-std=c11 -O2 -Wall -Wextra -Werror -municode -D__USE_MINGW_ANSI_STDIO=1` against the real official Windows 3.0.24 SDK `sdk/lib/libvlc.lib`. PE inspection confirms x86-64 and LibVLC imports. No VLC rebuild was performed. The user's custom baseline import library/build directory is unavailable here; this is not a claim to have linked against that local artifact.

Runtime/SDK: `https://download.videolan.org/pub/videolan/vlc/3.0.24/win64/vlc-3.0.24-win64.7z`, SHA256 `1ed59c09152e78aff84663fa76efab26057716576db74332858810e5ef54ae1f`. Published checksum and detached signature verified; release key fingerprint `A341FD7684DBC1FFD69BC25A1D77C0AE835B911E`, obtained via VideoLAN HTTPS (no independent web-of-trust claim).

Runtime tests use **Wine 11.0 from the same verified container layer**, official portable VLC 3.0.24, and newly generated 30-second timecoded fixtures (FFmpeg 7.1.5). These are Windows executable compatibility tests on Linux, **not native Windows tests**, and these fixture bytes are not established as identical to the user's local fixtures. No device/audio/window/foreground-playback assertions follow from this headless run. Logs include font/vout/converter **errors**, not just warnings; they are retained. Additional original-helper probes with only `--verbose=2` added reproduced EOF for both files. Logs show seek preroll and EOF, no `end of video preroll` message, and `video output creation failed` / `buffer deadlock prevented`. These errors can themselves prevent delivery without EncounteredError. Their contribution cannot be isolated from preroll/EOF behavior here, and Wine reproduction cannot establish the native root cause.

| Actual test | Outcome |
| --- | --- |
| Original helper, VFR 29,950 ms, 3× per format | Six exit-3 `ended_without_video`, no BMP, no external timeout |
| Rejected 50 ms retry experiment, same six | Six failures again |
| Tail recovery, same six | Six exit-0 fallback candidates; original EOF recorded |
| Visual inspection of those six recovery BMPs | All burned PTS 29,833 ms; absolute error 117 ms; **penultimate**, not final frames |
| Recovery latency, six focused attempts | Capture 2,734–3,315 ms; helper total 2,766–3,355 ms (Wine only) |
| Current helper, VFR 28,800/29,500 ms, 3× per format | 12 ordinary successes, no fallback |
| Ordinary/B-frame/long-GOP MP4/MKV, 14,750 ms | Six ordinary successes, no fallback; PTS unreviewed |
| Audio-only, corrupt, VFR MP4 past EOF (31,000 ms) | Exit 3, no fallback; no BMP |
| Current helper, ordinary MP4, 100 ms deadline, 3× | Exit 124, no external timeout, no BMP |
| Original/modified control image comparison | Ordinary MP4 at 14,750 ms and both VFR formats at 29,500 ms: three byte-identical BMP pairs; not an accuracy claim |
| Host policy and existing accuracy-scorer tests | 13 passed; includes exclusion guards and unreviewed-accuracy rejection |

The final delivery executable also passed a fresh 15-case Wine smoke run (six recovered targets, six ordinary/B-frame/long-GOP controls, and three negative inputs). All six newly inspected target BMPs again read 29,833 ms. Its SHA256 is `19ce2b893adf1f5ca24da123d812c979fe003efa23fa37da6b18add5a6e61103`. The clean full matrix produced **168/168 candidates: 162 ordinary and six fallback**, zero external timeouts, with all 168 image timestamps left unreviewed. Helper total latency was 1,190–3,198 ms, median 1,691.5 ms and nearest-rank P95 1,998 ms (Wine only). The accuracy scorer correctly rejects this unreviewed matrix. Images not explicitly reviewed remain unverified. Reported latency is callback/overall process timing, not frame timestamp evidence.

The first cloud matrix had 12 `output_error` exits because its harness reused the earlier control BMP names. CREATE_NEW correctly refused overwrites. That invalid run is preserved separately in the evidence archive; the reported 168/168 comes from a fresh directory with sequence-prefixed names using the final delivery executable. No errors were silently reclassified or images reused as successful new captures.

Committed records: `prototypes/stage_2_3/evidence/near_end_wine/` contains original, rejected-experiment, focused, final-artifact and clean-matrix measurements plus fixture/environment hashes. The downloadable evidence archive retains raw BMP/JSON/stderr and execution harnesses. Native results-run1 remains untouched.

## Reproduce in the validated local environment

Preserve `results-run1` and the baseline portable directory. From the new branch, use the existing incremental build script; mount existing source/build read-only and select a **new** output directory:

```sh
VLC_SRC="$HOME/src/VLC-Timeline-Preview"
# Existing validated win64/ must remain in place.
mkdir "$HOME/vlc-stage231-near-end"
docker run --rm -v "$VLC_SRC:/vlc:ro" \
  -v "$HOME/vlc-stage231-near-end:/stage231" -w /vlc \
  registry.videolan.org/vlc-debian-win64-3.0@sha256:b8e61c83d248cfb8fc3432d347f07f24de5a2ea6b0060cfe3053243e3674e6e7 \
  bash -lc 'bash prototypes/stage_2_3/build.sh /vlc/win64 /stage231/build'
cp -a "$VLC_SRC/win64/vlc-3.0.24" "$HOME/vlc-stage231-near-end/portable"
cp "$HOME/vlc-stage231-near-end/build/thumbnail-helper.exe" \
  "$HOME/vlc-stage231-near-end/portable/"
PYTHONDONTWRITEBYTECODE=1 python3 -m unittest discover \
  -s "$VLC_SRC/prototypes/stage_2_3" -p 'test_*.py' -v
```

Adjust the existing portable/build paths if yours differ. Copy the new isolated portable directory to `C:\VLC-Stage23\portable-near-end`, and copy this branch's `run_matrix.ps1` beside the original fixtures. Keep the original helper/package for baseline reproduction in another new directory. No source/core rebuild, codec pack or certificate bypass is needed.

Native PowerShell (pending execution; the revised runner has not been executed in PowerShell here):

```powershell
# Reproduce with the original helper into another new directory first.
& 'C:\VLC-Stage23\run_matrix.ps1' -Mode NearEnd `
  -Helper 'C:\VLC-Stage23\portable\thumbnail-helper.exe' `
  -Fixtures 'C:\VLC-Stage23\fixtures' -Results 'C:\VLC-Stage23\results-near-end-original'
# 51 attempts: VFR requested/control/past-end, other formats, negatives, deadline.
& 'C:\VLC-Stage23\run_matrix.ps1' -Mode NearEnd `
  -Helper 'C:\VLC-Stage23\portable-near-end\thumbnail-helper.exe' `
  -Fixtures 'C:\VLC-Stage23\fixtures' -Results 'C:\VLC-Stage23\results-near-end'
# Visually review recovered candidates and targeted controls before the full run.
& 'C:\VLC-Stage23\run_matrix.ps1' -Mode Full `
  -Helper 'C:\VLC-Stage23\portable-near-end\thumbnail-helper.exe' `
  -Fixtures 'C:\VLC-Stage23\fixtures' -Results 'C:\VLC-Stage23\results-run2'
python 'C:\path\to\prototypes\stage_2_3\score_results.py' `
  'C:\VLC-Stage23\results-run2\measurements.csv'
```

Fill each successful image's `observed_ms` from its actual burned PTS and `observation_method=visible_pts`; leave unreviewed rows blank. Use the full video-only matrix for the accuracy gate, since targeted negative/deadline rows intentionally fail. Confirm no audible output, playback window, focus changes, lingering helper processes or foreground playback interruption on the actual Windows desktop. Test output-exists/unwritable paths and external cancellation separately. A 100 ms run may succeed on a fast machine; it must remain bounded either way.

## Remaining limits and recommendation

- Immediate EndReached/stop can still omit the final scheduled display; recovery demonstrably returned the penultimate frame in six focused tests. A fixed sleep would not establish PTS correctness and is not added.
- Container length can be wrong or include audio beyond video. The 100 ms gate and video-track check reduce scope but cannot prove a valid video frame exists near that length.
- There is no callback PTS, reliable error bound or guarantee on arbitrary VFR/GOP/decoder inputs. Silent converter failures and near-end corrupt streams can still produce misleading earlier candidates; fallback metadata and visible review are mandatory.
- Windows cleanup/handle longevity, no-window/no-audio behavior, hardware decoding and user-built-runtime compatibility still need device tests. The process watchdog bounds hangs with abrupt private-process termination, possibly before JSON output.
- **Revise and validate the prototype further; do not start UI integration.** If native tests miss the final frame or exceed 250 ms, investigate timestamp-aware FFmpeg decode-forward selection rather than widening the fallback or trusting player clocks. Full Stage 2.3 accuracy acceptance remains pending.
