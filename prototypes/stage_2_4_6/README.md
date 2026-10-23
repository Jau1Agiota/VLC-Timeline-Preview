# Standalone libmpv seek experiment (Stage 2.4.6A)

This ZIP contains our executable and complete experiment source, **not libmpv**.
It does not use or modify VLC. Obtain the pinned runtime for private testing:

```powershell
.\Get-Runtime.ps1 -Destination C:\VLC-246A-Runtime
.\Run-Preview.ps1 -Media 'C:\Videos\movie.mp4' -Results C:\VLC-246A-Test1 -Runtime C:\VLC-246A-Runtime
```

Requires Windows x64, official 7-Zip on PATH or in Program Files, and a suitable
OpenGL driver (the RTX 3060 driver normally supplies it). Destination/results
must be new. Dependencies are not installed globally. Hover the strip under the
image, or enter a timestamp in milliseconds in the console. Enter `quit` to exit.
The image uses a 320×180 canvas with aspect-preserving letterboxing, not a lower
resolution decoding pass. Audio/subtitles/config/scripts/network helpers are off.
The player remains paused and initialized; the default is **keyframe** seeking,
which can intentionally be seconds away from the request on long-GOP media.

For CPU/software-renderer measurements, pass `-Renderer sw`; that mode is not
MPC-QT's OpenGL path. `-Hardware auto-copy` requests hardware decode with copying;
check `hwdec_current` in raw results rather than assuming acceleration. The
default `-Hardware no` decodes on CPU with OpenGL scaling. `-Seek exact` is an
explicit accurate-seek comparison, not the default fast-keyframe experiment.

Timing benchmark (Python 3 on PATH, new output directory):

```powershell
py .\benchmark.py --exe .\mpv-preview.exe --runtime C:\VLC-246A-Runtime --media 'C:\Videos\movie.mp4' --output C:\VLC-246A-Bench1 --samples 30 --renderer gl
```

Outputs raw `events.jsonl`, `benchmark.json`, and at most 128 displayed BMPs.
The sampler spans 3%..95% of the file with back-and-forth seeks, then six rapid
12-request bursts. Only one seek and one latest pending request exist; there is
no cache, pre-index or debounce. Older candidates cannot overwrite newer input.

`command_ack_ms` is not a decoded frame. A candidate must also have a seek event,
playback-restart event and a non-redraw/non-repeat render frame. `display_handoff_ms`
ends after SwapBuffers (OpenGL) or StretchDIBits/GdiFlush (software); physical
monitor scanout is **not verified**. `video_pts_property_ms` is a property sampled
while paused after handoff, not independent proof of the BMP's PTS. Read burned-in
PTS in known test footage, record observed milliseconds separately, and report
absolute error. No accuracy gate is inferred from exit code or command reply.
CPU time is process CPU milliseconds (all its threads), memory is process working
set/private bytes. Keyframe error is a deliberate speed/accuracy tradeoff.

Build in the existing MinGW container, or MSYS2 UCRT64 with GCC, against the
verified SDK fetched above:

```sh
CC=x86_64-w64-mingw32-gcc bash prototypes/stage_2_4_6/build.sh VERIFIED_SDK NEW_OUTPUT
cc -std=c11 -Wall -Wextra -Werror prototypes/stage_2_4_6/test_policy.c -o /tmp/preview-policy
/tmp/preview-policy
```

The SDK archive/DLL hashes are pinned in `dependencies.json`; `build.sh` refuses
other DLLs and existing outputs. MSYS2 UCRT64 uses `CC=gcc`. Strict warnings remain
errors. To launch a locally built binary, keep `libmpv-2.dll` on PATH (or copy it
adjacent only into your new private test folder); never overwrite older builds.
The DLL imports Windows system/UCRT libraries and `vulkan-1.dll`. An up-to-date
NVIDIA driver normally supplies the Vulkan loader; a missing loader is a genuine
runtime blocker, not a reason to download unidentified DLLs.

No Windows GPU/visual/performance acceptance is claimed. See repository
`docs/STAGE_2_4_6A_LIBMPV.md` for actual CI results, exact provider source,
licensing limits and integration recommendation. Keep Stage 2.4.5 as rollback;
this experiment has no hook into VLC at all.
