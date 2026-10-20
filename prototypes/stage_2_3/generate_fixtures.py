#!/usr/bin/env python3
"""GPL-2.0-or-later. Generate synthetic, visibly timecoded MP4/MKV fixtures.
Uses an existing FFmpeg with libx264, AAC, drawtext and a user-supplied font.
Does not download binaries, fonts, codecs or media. Refuses existing output.
"""
import argparse
import hashlib
import json
from pathlib import Path
import subprocess


def run(command):
    subprocess.run(command, check=True)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("output", type=Path)
    parser.add_argument("--font", required=True, type=Path)
    parser.add_argument("--duration", type=int, default=30)
    args = parser.parse_args()
    if not args.font.is_file() or not 3 <= args.duration <= 300:
        parser.error("supply a readable font and duration between 3 and 300 seconds")
    args.output.mkdir(parents=True, exist_ok=False)
    # Use a local simple filename so ffmpeg filter escaping never depends on
    # an arbitrary Windows/WSL font path. No user files are overwritten.
    (args.output / "font.ttf").write_bytes(args.font.read_bytes())
    version = subprocess.check_output(["ffmpeg", "-version"], text=True).splitlines()[0]
    records = []
    variants = [("ordinary", 0, 30, None), ("bframes", 3, 60, None),
                ("long_gop", 3, 300, None),
                ("vfr", 3, 90, r"select=not(mod(n\,3))+not(mod(n\,5))")]
    for name, bframes, gop, selection in variants:
        # Burn original presentation time AFTER selection. No setpts reset:
        # selected VFR frames keep their actual uneven presentation times.
        filters = [] if selection is None else [selection]
        filters += ["drawbox=x=0:y=0:w=iw:h=100:color=black:t=fill",
                    r"drawtext=fontfile=font.ttf:text='PTS %{pts\:hms}':"
                    "fontcolor=white:fontsize=40:x=16:y=12",
                    f"drawtext=fontfile=font.ttf:text='{name}':"
                    "fontcolor=yellow:fontsize=28:x=16:y=60"]
        mp4 = args.output / f"{name}.mp4"
        command = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-n",
                   "-f", "lavfi", "-i", f"testsrc2=size=640x360:rate=30:duration={args.duration}",
                   "-f", "lavfi", "-i", f"sine=frequency=660:sample_rate=48000:duration={args.duration}",
                   "-vf", ",".join(filters), "-fps_mode", "vfr", "-c:v", "libx264",
                   "-threads", "2", "-preset", "fast", "-crf", "18", "-pix_fmt", "yuv420p",
                   "-bf", str(bframes), "-g", str(gop), "-keyint_min", str(gop),
                   "-sc_threshold", "0", "-c:a", "aac", "-b:a", "96k",
                   "-movflags", "+faststart", mp4.name]
        subprocess.run(command, cwd=args.output, check=True)
        mkv = args.output / f"{name}.mkv"
        run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-n",
             "-i", str(mp4), "-map", "0", "-c", "copy", str(mkv)])
        for path in (mp4, mkv):
            probe = subprocess.check_output([
                "ffprobe", "-v", "error", "-select_streams", "v:0", "-show_frames",
                "-show_entries", "frame=best_effort_timestamp_time,pict_type,key_frame",
                "-of", "json", str(path)], text=True)
            frames = json.loads(probe)["frames"]
            (args.output / f"{path.name}.frames.json").write_text(probe)
            times = [float(frame["best_effort_timestamp_time"]) for frame in frames]
            gaps = {round(b - a, 4) for a, b in zip(times, times[1:])}
            if not times or (bframes and not any(f["pict_type"] == "B" for f in frames)):
                raise RuntimeError(f"Fixture invariant failed: {path}")
            if name == "vfr" and len(gaps) < 2:
                raise RuntimeError("VFR fixture did not preserve varying presentation intervals")
            key_indices = [i for i, frame in enumerate(frames) if frame["key_frame"]]
            if name == "long_gop" and key_indices != list(range(0, len(frames), 300)):
                raise RuntimeError("Long-GOP fixture did not preserve 300-frame keyframe spacing")
            records.append({"keyframe_indices": key_indices, "file": path.name, "sha256": hashlib.sha256(path.read_bytes()).hexdigest(),
                            "frames": len(frames), "first_pts_s": times[0], "last_pts_s": times[-1],
                            "presentation_gaps_s": sorted(gaps),
                            "b_frames": sum(f["pict_type"] == "B" for f in frames)})
    # Also provide an audio-only MP4 with the same audible test tone.
    audio = args.output / "audio_only.mp4"
    run(["ffmpeg", "-hide_banner", "-loglevel", "error", "-nostdin", "-n",
         "-f", "lavfi", "-i", "sine=frequency=660:duration=3", "-c:a", "aac", str(audio)])
    (args.output / "corrupt.mp4").write_bytes(b"Stage 2.3 intentionally invalid media\n")
    (args.output / "manifest.json").write_text(json.dumps({
        "ffmpeg": version, "duration_s": args.duration, "fixtures": records,
        "note": "Burned PTS is the source image truth; container remux can offset its timestamps."}, indent=2) + "\n")
    print(json.dumps(records, indent=2))


if __name__ == "__main__":
    main()
