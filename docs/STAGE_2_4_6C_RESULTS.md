# Stage 2.4.6C measured results

This is an experiment for user comparison, **not an accuracy/performance acceptance
pass or a replacement for Stage B**. No physical Windows Qt/fullscreen/RTX3060
review, real-movie timing or PotPlayer comparison was performed here.

## Package and provenance

- Base: PR16, `8161fa1a0e88efc0a2911255164ca22503c3d8f6`, preserved.
- Application/worker source: `febfd332f4ffbc94981f0b1cac298dc941de057b`.
- Clean build driver: `61f934c6e4c81571c10812365f0a303c71c3f3e9`.
- [Clean build and package](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40014553690).
  The build job passed in49m29s, including pinned GCC6.4.0/Qt5.6.3 cross-compilation,
  package checks, Wine smoke, upload and independent download/readback verification.
- [Download the package artifact](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40014553690/artifacts/12529659132).
  GitHub login may be required; retention is14days, not a permanent release.
- Inner ZIP: `VLC-3.0.24-stage246C-ci1-win64-febfd332f4ff-40014553690-1.zip`,
  512440570bytes. Its SHA-256:

```text
ca26c24e57559bb76571299231e575f67415d3bb743875905dfcf50bfd4a6cdd
```

The GitHub artifact wrapper has a different digest; verify the INNER ZIP against
PACKAGE_MANIFEST.json/SHA256SUMS. The package includes both workers, modified Qt
plugin and the separate official VLC helper runtime. The checksum-verified libmpv
DLL remains a private Setup-Runtime.ps1 download, not redistributed in this ZIP.
Later commits change tests/reporting/docs only; this source/driver provenance stays
valid. Previous portable packages, source worktrees and untracked win64 remain intact.

## Native performance comparison

[Expanded Windows benchmark](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40021018636),
Windows2025 GitHub-hosted runner, unchanged pinned runtime, software decoding.
Counterbalanced fast/exact order,12files,72warm requests per mode. Candidate receipt
includes IPC; it is NOT Qt painting, monitor scanout or real-movie smoothness.
First request includes worker/media initialization; DLL/OS caches are not controlled.
Rapid tests emit20distant intents at5ms intervals with one active/one newest pending.

| Measurement (ms, median / p95) | Stage B fast | Exact |
|---|---:|---:|
| First candidate (12files) |55.93 /694.19|63.60 /127.18|
| Warm candidate (72each) |9.92 /26.54|20.50 /272.17|
| Warm Main10 HEVC (12each) |26.18 /28.99|196.32 /320.50|
| Warm long-GOP H.264 (12each) |10.48 /11.59|39.48 /65.45|
| Warm VFR (12each) |10.43 /11.60|26.44 /29.44|
| Rapid final intent→candidate (12each) |15.34 /43.42|32.44 /639.32|

Both modes delivered candidates and cleaned up without reported native extraction
failures. Fast returned205candidates, exact165; rapid throughput differs by design,
so use the identical warm-target sets for direct latency comparisons. There were46
isolated LibVLC comparison/probe images. LibVLC start-time probes are an accuracy
reference, **not a test of the actual Qt slider click**.

Exact precision costs decoding work. Its HEVC latency and rapid-final tail are
materially worse; preserving Stage B responsiveness is **not demonstrated**. This
is why exact is selectable, fast remains unchanged/default for libmpv, and no
hybrid/double-update refinement was added.

Peak sampled private memory:113.93MiB fast,127.29MiB exact. HEVC first→last samples:
fast100.53→113.93MiB (MP4), exact120.04→127.29MiB. Allocator warm-up/growth occurred;
this short run is not a long-duration leak/stability test. Cumulative HEVC worker
CPU was about0.86–0.91s fast versus6.33–6.42s exact per file workload. These are CPU
time totals, not utilization percentages; request counts differ. GPU utilization
was not measured, hardware decoding was disabled, and no RTX3060 result is claimed.
Full native headers/bitmaps are in the run's `stage246C-native-results-*` artifact.

## Image accuracy and remaining failures

[Corrected image-analysis run](https://github.com/Jau1Agiota/VLC-Timeline-Preview/actions/runs/40022314293)
passed native extraction, headless Windows VLC playback and the analyzer. It reused
the SAME ZIP; VLC was not rebuilt. Its raw native results and image-evidence
artifacts contain418images/records:206fast,166exact,46LibVLC. All418 produced
plausible OCR matching known fixture frame times. This is **provisional automated
image evidence, not human verification or a runtime frame-PTS API**.

| Burned-in PTS absolute error | Fast | Exact | LibVLC proxy |
|---|---:|---:|---:|
| Median / p95 (ms, all images) |1000 /8000|0 /5000|66.5 /200|
| Within100ms |14/206|149/166|35/46|

Exact's148non-offset images all fell within100ms. Per-fixture exact p95 was17–27ms
ordinary H.264,27ms B-frames/long-GOP,33ms Main10 HEVC,50ms VFR. Examples from the
HEVC MP4: request23973→burn24000 (27ms),29950→29958 (8ms),50→83 (33ms).
The ordinary fast keyframe control had1s errors;10s-GOP H.264/HEVC reached9950ms.
These are actual decoded pixels/OCR, not player-clock comparisons.

**Nonzero-origin media remain inaccurate.** The offset fixture adds5s to container
PTS without changing visible content. Exact request25000 produced burn20000;
18000→13000;29950→24967.17of18offset candidates exceeded100ms. The documented
exact mode does not normalize this origin into VLC-relative content time. No
constant offset workaround was added. Do not treat exact mode as validated for
nonzero-origin/edit-list media; test origin semantics before extending support.

**Same-scene VLC acceptance remains unresolved.** Around cuts, exact's burned-in
PTS was within1–3ms for many adjacent requests (970→967,999→1000,1001→1000), yet
RGB comparison with isolated LibVLC start-time capture matched only4of13probes
(fast8of13). For example, exact at1000 selected the blue1s scene while the LibVLC
first-callback probe returned white; at1999 exact selected the white2s scene while
the proxy returned blue. The proxy's burned-in frames can precede the target,
and its capture is NOT VLC's settled Qt click. Making exact emulate those early
callbacks would not establish correct foreground behavior. The mismatch is recorded,
not hidden as a100ms pass. Actual paused-click/snapshot review is required.

Pinned mpv's precision loop permits approximately5ms early tolerance, and EOF
can retain its final frame. Thus even a tiny numerical error can straddle a cut.
No universal identical-frame guarantee is made. Public render/client APIs do not
provide reliable per-surface media PTS; all runtime candidates explicitly remain
provisional with null actual-image PTS. No completion event confirms accuracy.

The first analysis runs failed because the scene analyzer required320x180 whereas
LibVLC emits aspect-preserving images bounded by that size. The analyzer now
accepts bounded32bpp dimensions, samples a proportional position and retains
strict size/offset/payload checks. Added regressions cover those dimensions,
orientation, invalid width/height/bit depth/offset and truncated payload. GitHub's
ten-notice-per-step limit also hid the initial final summary; summaries now appear
first and per-media data is grouped. Neither correction changed the shipped app
or workers. Prior failed runs remain visible; the clean build job itself passed.

## Evidence, limitations and recommendation

[Committed API/Checks measurements](../prototypes/stage_2_4_6C/evidence/ci-measurements.json)
record exact provenance, artifact digests, job outcomes, raw numeric summary notices
and focused image observations. Full request headers/BMPs/results.json/accuracy.json
are in the linked Actions artifacts (14day retention). Cloud artifact readback
returned403 from the artifact service, so this checkout contains API/Checks
excerpts, not an invented copy of the full raw artifacts. Runner-side independent
package download and checksum verification passed.

Host Qt/fake-decoder lifecycle and exact mapping tests,43CI guard tests (one skip),
GCC6.4.0 compile/link, Bash syntax, ShellCheck, actionlint and two Python analyzer
tests passed. Package checks confirm the modified Qt plugin and exact worker;
headless playback passed on Windows. They do NOT prove physical GUI launch,
continuous nonblank previews, fullscreen behavior, audible playback isolation on
this device or real-film smoothness. Those are pending user tests.

**Recommendation:** retain Stage B as the default performance baseline. Offer this
build only for controlled fast/exact comparison on zero-origin local media. Do not
merge/promote it as an accepted accurate engine yet: HEVC latency, time-origin
alignment and settled VLC-click scene matching are unresolved. Hybrid remains
unimplemented because two seeks/updates may increase CPU and cause visible image
changes; no evidence here establishes a better hybrid policy. No background
indexing, lower-quality stage, playback-engine changes or new decoder was added.

Follow [setup, selection and Windows acceptance instructions](STAGE_2_4_6C_ACCURACY.md).
Test the user's original movie at46:23 and1:10:52 in fast/exact modes, compare
paused same-pixel clicks, record actual scenes plus first/warm/rapid latency, and
check fullscreen, media switching, leave/reenter, dragging and shutdown. Keep
runtime and extraction folders separate from prior builds. StageB/LibVLC remain
selectable fallbacks. Stop for user acceptance; no PR is merged automatically.
