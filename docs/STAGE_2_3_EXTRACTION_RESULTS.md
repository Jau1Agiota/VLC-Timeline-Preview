# Stage 2.3 extraction prototype results

Date: 2026-10-20. Target: VLC 3.0.24 Windows x64. This is a standalone research prototype; **no timeline integration** is included.

## Baseline and implementation

Branch: `prototype/stage-2.3-libvlc-extraction`, created from fetched baseline `966bcdd4b9` (merge of Stage 2.2 architecture PR #3). The validated baseline build/Windows playback success is the repository owner's report, not a build performed in this session.

New source and tools are isolated under [prototypes/stage_2_3](../prototypes/stage_2_3/README.md): Windows C helper, standalone incremental build script, synthetic fixture generator, PowerShell matrix runner, visible-timestamp scorer and scorer tests. Existing VLC application/build/Qt sources and playback behavior are unchanged. No top-level build-system integration is needed to compile one standalone executable against the existing baseline import library; no full VLC rebuild is requested.

The helper uses the verified 3.0.24 public APIs: `libvlc_new`, `libvlc_media_new_path`, `libvlc_media_add_option`, `libvlc_media_player_new_from_media`, `libvlc_video_set_callbacks`, `libvlc_video_set_format_callbacks`, player play/stop/release and media/instance release. Its private instance disables audio, subtitles, OSD, title display and fast seeking. Memory callbacks select private vmem output/software decoding in this release. No foreground instance, playlist, HWND, input or player handle is accepted or accessed.

It requests per-media `:start-time` then saves the **first displayed candidate** as a bounded BMP using three aligned callback slots and an owned image copy. It never treats time events, player clocks or fixed sleeps as frame-PTS evidence. A helper-side watchdog terminates only this process if initialization/capture/output/cleanup stalls; the external matrix runner also bounds argument/file validation and process startup. Output file creation refuses overwrite. See the README for exact bounds, error codes and expected versus unverified behavior.

## Actual checks performed in this session

| Check | Actual result | What it establishes |
| --- | --- | --- |
| Baseline fetch and architecture merge | Baseline contains `docs/STAGE_2_2_ARCHITECTURE.md` via merge `966bcdd4b9`; working tree initially clean | Correct branch ancestry and research input |
| Compiler/runtime availability | No `x86_64-w64-mingw32-gcc`, Wine or PowerShell found; host GCC and FFmpeg are available | Windows helper and PowerShell execution unavailable through installed tools |
| Docker storage | Daemon remains `vfs`, filesystem 32 GiB; approximately 19 GiB free before this stage | Earlier image-plus-writable-layer exhaustion remains diagnosed; no unchanged full-image pull/run was repeated |
| Fixture generator | Completed default 30-second generation of eight visibly timecoded H.264 MP4/MKV files, plus audio-only and corrupt fixtures | Real generated test inputs, not a helper accuracy test |
| Fixture probe checks | Six CFR files: 900 frames each; two VFR files: 420 frames each. MP4 VFR gaps 33.3/66.7/100 ms; MKV gaps 33/67/100 ms. B-frame variants contain B pictures; long-GOP keyframes verified at indices 0/300/600 | Required ordinary/B-frame/long-GOP/VFR cases actually generated; MKV timebase introduces rounding |
| Visible-text readability | A fixture frame independently decoded with host FFmpeg and scaled to 320×180 visibly reads `PTS 00:00:14.767` | Burned label is readable at preview size; this is **not a helper-produced frame** |
| Scorer unit tests | Seven tests passed | Empty, unmeasured, player-clock-only, inaccurate, failed/timed-out and nonfinite evidence cannot silently pass; 250 ms inclusive boundary checked |
| Generator overwrite protection | Second invocation on same output failed with `FileExistsError` | Existing fixture output was preserved |

Fixture generation used installed `ffmpeg version 7.1.5-0+deb13u1`, not the VLC contrib decoder. No new codec pack or media was downloaded. Generated files and machine logs were placed outside the checkout (`/tmp/vlc-stage23-fixtures`, associated `/tmp` logs). They are reproducible inputs, not portable deliverables or committed generated assets. The generator records exact output hashes, frame metadata and FFmpeg version in its manifest; different encoders/tool versions can produce different bytes.

## Actual extraction measurements

**None.** The Windows helper was not cross-compiled, linked, launched or measured in this cloud instance. No helper-produced image was inspected. Requested/observed timestamp pairs, latency, accuracy success rate, visible-window/audio behavior, decoder cleanup and timeout behavior are all **unrun**, not passed or failed. There is no evidence yet that the proposed 250 ms budget is achievable by this candidate-selection rule.

The absence of a Windows compiler/runtime and the previously demonstrated 32 GiB vfs container-creation blocker prevent the validated build route here. They do not prove a defect in this helper or LibVLC. The user's working WSL2/Docker Desktop environment can compile it incrementally using the existing validated import library and plugins. No baseline rebuild is technically needed.

## Planned local measurements — not results

Follow [the exact build and Windows procedure](../prototypes/stage_2_3/README.md). Build only `thumbnail_helper.c`; copy the resulting executable into a **separate copy** of the validated portable package. Generate the default fixtures, then use `run_matrix.ps1` to collect 168 isolated candidate attempts (8 video files × 7 timestamps × 3 repetitions).

The CSV records requested milliseconds, callback/wall latency, exit code, image filename and explicit unreviewed observation fields. Populate observed milliseconds **from the burned PTS visible in each actual helper BMP**. The scorer compares those measurements to the request with a 250 ms budget; capture success alone cannot pass. Retain every candidate, stderr/JSON record, manifest and raw/reviewed CSV. Separately record no-window/no-audio and foreground continuity observations, failures, timeouts and remaining helper processes.

Expected failure tests include audio-only/corrupt/missing inputs, past-EOF time, invalid arguments, unwritable/existing output, deadlines and external cancellation. Expected behavior is a bounded failed extraction with no foreground interaction; exact runtime codes/latencies must be recorded instead of assumed. Incorrect frames are **inaccurate output**, not successful tests.

## Recommendation

**Revise/validate the prototype before proceeding with LibVLC; do not start timeline integration.** The independent capture path is implemented, but its first-display startup-selection rule remains experimentally unverified. Run the supplied matrix on native Windows and attach visible-image evidence. If reliable association within 250 ms is not demonstrated, investigate a dedicated FFmpeg backend that selects decoded frames by their presentation timestamps and decodes forward from a prior keyframe. Do not “fix” failed evidence by assuming TimeChanged/get_time or a delay supplies frame PTS.

Unresolved risks include build/compiler diagnostics, format-callback/stop races and blocked slots, startup seek accuracy, short/past-end clips, long-GOP latency, VFR/container offsets, decoder memory/resource contention, SAR/HDR/color fidelity, DLL/plugin discovery, Windows path edge cases and actual cleanup/window/audio behavior. The bounded callback output is not a bound on full-resolution decoder memory. This prototype adds neither a cache nor a UI; stop after these experiments.

## Stage 2.3.1 — MinGW integer-format compilation fix

The repository owner reported that the validated VideoLAN compiler rejects literal `%llu` in the startup timestamp and JSON output under `-Werror=format`. Both sites now use `<inttypes.h>`'s `PRIu64` with explicitly matching `uint64_t` arguments. This delegates the format to the target headers: inspected MinGW-w64 12.0.0 headers select `I64u` for native MSVCRT formatting and `llu` for ANSI/UCRT formatting. Do not hard-code either convention for all stdio modes. Nearby `fprintf` uses `%s`, the usage message has no numeric conversions, and the remaining `%u`/`%d` arguments already match their types. Three-digit milliseconds, all JSON field names/types and 64-bit precision are preserved; build flags and strict warnings are unchanged.

Actual follow-up verification: Debian's signed package repository supplied an isolated MinGW-w64 GCC 14-posix toolchain under `/tmp` (GCC package `14.2.0-19+27+b1`, headers `12.0.0-5`). The complete corrected helper compiled into Windows x64 COFF objects with `-std=c11 -O2 -Wall -Wextra -Werror` in both `__USE_MINGW_ANSI_STDIO=1` and `=0` modes. This newer compiler also accepted the original source in native mode, so the exact reported VideoLAN compiler failure was **not reproduced** here. No claim is made that this toolchain matches the validated Docker compiler.

A host-compiled harness using the actual corrected formatting calls passed six targets (0, 80, 999, 1000, 14750 and 86400000 ms), verifying exact fractional padding, the unchanged JSON schema and `UINT64_MAX` latency output without truncation. These are formatting/compiler checks, not extraction measurements. Linking against the validated baseline DLL/import library, the exact VideoLAN Docker build and Windows execution remain unverified in this follow-up. Rerun the existing standalone `build.sh` command with a new output directory in the validated local container; no full VLC rebuild or documentation-command change is needed. No timeline or playback code was modified.
