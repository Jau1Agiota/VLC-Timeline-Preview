# Stage 2.4.6B results and acceptance gates

Implementation in progress. No Windows GUI or real-movie testing has been
performed in this stage. The user authorized an experimental integration after
A.1 removed visible gaps, while remaining slower than MPC-QT.

Verified locally: the new worker compiles AND links with pinned MinGW GCC 6.4.0,
-Wall -Wextra -Werror and the checksum-pinned libmpv SDK import library. This is not
execution proof. Existing LibVLC controller regressions passed on Linux Qt5.
New integration regressions and a fresh CI package are pending final verification.

The new SW-render/raw-pipe path has unavoidable CPU scaling/copies and differs
from A.1's direct OpenGL display. A.1's synthetic timings must not be reported as
measurements of this integration. No real-movie comparison or accuracy gate has
passed. Native Windows synthetic worker/LibVLC comparisons, when available, will
be recorded separately from manual VLC popup/audio/fullscreen/device acceptance.

Known risks: software decoding of long-form HEVC; 1 GiB helper process cap;
serialized in-flight seeks can delay the newest pending request; coarse keyframe
accuracy; HDR/color management differences; Windows DLL dependencies; cold-start
6.5 s controller deadline; separate controllers can restart the helper on a
windowed/fullscreen handover. Missing/failing runtime uses timestamp-only with a
clear error; select libvlc and restart to restore the original backend.

User acceptance requires the exact same RTX 3060/1080p10-bit HEVC movie and a
human review of image/request labels. See STAGE_2_4_6B_INTEGRATION.md checklist.
