#!/usr/bin/env python3
"""GPL-2.0-or-later. Full-resolution synthetic HEVC10 truth fixture, not a movie surrogate."""
import argparse
import hashlib
import json
import shutil
import subprocess
from pathlib import Path


def generate(output,font,duration):
    output=Path(output)
    if output.exists():raise ValueError('output must be new')
    if not 12<=duration<=600 or not Path(font).is_file():raise ValueError('font and duration 12..600 required')
    output.mkdir();shutil.copyfile(font,output/'font.ttf')
    version=subprocess.check_output(['ffmpeg','-version'],text=True).splitlines()[0]
    filt=(r"drawbox=x=0:y=0:w=iw:h=220:color=black:t=fill,"
          r"drawtext=fontfile=font.ttf:text='PTS %{pts\:hms}':fontcolor=white:fontsize=120:x=30:y=20,format=yuv420p10le")
    subprocess.run(['ffmpeg','-nostdin','-v','error','-n','-f','lavfi','-i',
                    f'testsrc2=size=1920x1080:rate=24:duration={duration}',
                    '-vf',filt,'-c:v','libx265','-preset','ultrafast','-crf','24','-g','240','-keyint_min','240',
                    '-x265-params','pools=2:frame-threads=2:scenecut=0:log-level=error','-tag:v','hvc1',
                    '-movflags','+faststart','hevc10.mp4'],cwd=output,check=True,timeout=600)
    subprocess.run(['ffmpeg','-nostdin','-v','error','-n','-i','hevc10.mp4','-c','copy','hevc10.mkv'],cwd=output,check=True,timeout=60)
    probe=subprocess.check_output(['ffprobe','-v','error','-select_streams','v:0','-show_frames',
        '-show_entries','frame=best_effort_timestamp_time,pict_type,key_frame','-of','json',str(output/'hevc10.mp4')],text=True,timeout=180)
    (output/'hevc10.mp4.frames.json').write_text(probe)
    manifest=dict(ffmpeg=version,duration_s=duration,synthetic=True,not_hdr=True,
        font_sha256=hashlib.sha256((output/'font.ttf').read_bytes()).hexdigest(),
        files={p.name:hashlib.sha256(p.read_bytes()).hexdigest() for p in output.glob('hevc10.*')})
    (output/'manifest.json').write_text(json.dumps(manifest,indent=2))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('output');p.add_argument('--font',required=True)
    p.add_argument('--duration',type=int,default=180);a=p.parse_args();generate(a.output,a.font,a.duration)
