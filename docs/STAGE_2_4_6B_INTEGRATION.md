# Stage 2.4.6B experimental integration

Chosen base (before edits): latest remote feature/stage-2.4.5-thumbnail-performance,
92102fca537ef36bb11c018146766bc2a656d8cf (Stage 2.4.5 plus merged standalone A).
Stage A.1 reference: e9d79b96ae4af3691693c894d8b4e44b11a999d8, PR #15 remains
unmerged when work starts. No whole-branch merge: only its checksum-verified runtime
setup is reused. All prior worktrees/builds remain untouched.

## Architecture and rollback

Default remains the Stage 2.4.5 LibVLC helper. Explicit VLC_THUMBNAIL_BACKEND=libmpv
selects VLC_MPV_HELPER, an absolute path to the new worker beside its verified DLL.
Missing/unknown backend or worker fails closed with the existing timeline-preview
error log and timestamp-only tooltip; no silent decoder substitution. Restart with
libvlc to roll back. Launch uses --no-one-instance to avoid forwarding to an old VLC.
VLC's main player, SeekSlider input handling, tooltip placement and fullscreen
ownership are unchanged. No standalone seekbar is added.

The private persistent paused libmpv SW-render worker opens a local MP4/MKV
independently; audio/subtitles/scripts/config/network resolvers are disabled.
It stays alive across hover leave/re-entry, until media invalidation, timeline
handover, VLC destruction, failure or its 24-hour idle watchdog. First creation
is lazy on the first eligible hover, rather than eager on every media open.
One active request and one replaceable latest pending request; 16 ms launch
throttle (existing LibVLC debounce remains 35 ms). No whole-video preprocessing.
Existing bounded shared LRU is retained, with backend-separated keys.

Frames use public mpv_render_context_* SW API, 320x180 BGR0 with aspect-preserving
letterbox, full-resolution decode, no low-resolution intermediate. An ASCII JSON
header (<=1024 bytes) precedes exactly 230400 raw bytes on the process stdout pipe.
QProcess receives asynchronously; a bounded buffer and one deep QImage copy feed
the existing popup. No BMP/files/shared writable mappings or GPU readback. Pipe
transfer and GUI copy ARE CPU copies; software scaling can be expensive. This is
not the direct-GL zero-readback standalone path and performance parity is unproven.
The worker logs pipe-write time only when diagnostics are enabled; controller
records request-to-present, SW render and Qt-copy timings. Pipe-write may include
backpressure, and controller end-to-end latency includes debounce/startup/transit.

Older in-flight candidates may be displayed with their OWN approximate image-
request label while the hovered timestamp continues independently. Monotonic
presentation generations prevent an older completion replacing a newer cache hit.
No callback supplies frame PTS: keyframe seeks can be seconds away. The tooltip's
`~` image-request label is not verified frame time. Pending seeks retain the last
valid image; leave/drag/media invalidation still hides it immediately.

The GUI never waits for decoding. Existing QProcess containment/kill-on-job-close
remains, with a 1 GiB private-helper ceiling for libmpv (LibVLC remains 512 MiB).
Controller deadline is 6.5 s, worker startup 15 s and seek/pipe/teardown 5 s;
controller's earlier deadline may terminate slow initialization. Process teardown
uses the existing <=1 s destructor wait. No claim of instant interruptible decoding.

## Windows setup and device acceptance

Extract the NEW Actions package; do not overwrite any old portable VLC folder.
In PowerShell from the extracted VLC root:

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Setup-Runtime.ps1 -Destination C:\VLC-246B-Runtime1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libmpv -Runtime C:\VLC-246B-Runtime1 -Media C:\Videos\movie.mkv
# Same media, original backend, separate process:
powershell.exe -NoProfile -ExecutionPolicy Bypass -File .\thumbnail-mpv\Start-Preview.ps1 -Backend libvlc -Media C:\Videos\movie.mkv
```

Setup requires official 7-Zip and HTTPS access to the pinned identified upstream.
Start validates DLL and worker hashes before launching. No libmpv runtime is
redistributed: see thumbnail-mpv/THIRD_PARTY.md. Diagnostic launch adds
`-Diagnostics`; log is a new `vlc246B-*.log` in %TEMP%. Normal mode stays quiet.

On RTX 3060 with the SAME 1080p 10-bit HEVC movie, test both backends separately:
first hover after opening; distant targets (3%,85%,22%,65%,7%,95%); rapid continuous
back-and-forth; stationary hover; pause/resume; click/drag/keyboard seeking; audio
continuity; fullscreen/controller auto-hide; window edges/multiple monitors; switch
files during seeking; audio-only/network/non-seekable inputs; helper missing/crash;
close VLC during an active request. Check no orphan worker and memory after repeats.
Never label old image as current; no blank image while pending after first success.
Use burned-in known PTS footage for accuracy, not time-pos/command acknowledgements.
Record raw request and actual-image observations; median/p95 first/warm/rapid
latencies separately. Physical display, real-movie performance and playback
isolation must be accepted by the user, not inferred from CI process success.

## Build and evidence

Worker: `CC=x86_64-w64-mingw32-gcc bash prototypes/stage_2_4_6B/build.sh VERIFIED_SDK NEW_OUTPUT`.
Strict GCC 6.4.0 cross-build required; VLC uses existing pinned VideoLAN image,
matching prebuilt contrib and separate official 3.0.24 helper runtime. Workflow
pins application source independently of the CI driver. Readback verifies ZIP,
per-file hashes and manifest. No previous artifacts are reused or removed.

Results and remaining gates: see STAGE_2_4_6B_RESULTS.md. Stop for device approval;
no claim of MPC-QT parity or frame-accuracy acceptance.
