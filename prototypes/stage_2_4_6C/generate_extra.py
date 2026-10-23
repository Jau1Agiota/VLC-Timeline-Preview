#!/usr/bin/env python3
"""GPL-2.0-or-later. Add deterministic 1080p Main10/cut/start-offset fixtures to a NEW fixture bundle."""
import argparse
import json
from pathlib import Path
import subprocess
import sys


def run(args):
    subprocess.run(args, check=True, timeout=240)


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('fixtures',type=Path)
    p.add_argument('--font',type=Path,required=True)
    a=p.parse_args()
    run([sys.executable,str(Path(__file__).with_name('generate_hevc_fixture.py')),
         str(a.fixtures/'hevc'),'--duration','30','--font',str(a.font)])
    # Reuse font filename escaping from the existing generator's owned bundle.
    filters=(r"drawbox=x=0:y=100:w=iw:h=ih-100:color=white:t=fill:enable='lt(mod(t\,2)\,1)',"
             r"drawbox=x=0:y=0:w=iw:h=100:color=black:t=fill,"
             r"drawtext=fontfile=font.ttf:text='PTS %{pts\:hms}':fontcolor=white:fontsize=40:x=16:y=12")
    command=['ffmpeg','-nostdin','-v','error','-n','-f','lavfi','-i','color=c=blue:s=640x360:r=30:d=30',
             '-vf',filters,'-c:v','libx264','-threads','2','-g','120','-bf','3','-pix_fmt','yuv420p','scene_cuts.mp4']
    subprocess.run(command,cwd=a.fixtures,check=True,timeout=120)
    # Positive stream PTS offset with unchanged visible burn-in times.
    run(['ffmpeg','-nostdin','-v','error','-n','-i',str(a.fixtures/'ordinary.mp4'),'-map','0:v:0',
         '-c','copy','-output_ts_offset','5',str(a.fixtures/'offset.mp4')])
    (a.fixtures/'stage246C.json').write_text(json.dumps(dict(
        hevc='30s 1920x1080 Main10 SDR, 10s GOP, visible PTS',
        scene_cuts='30s 640x360 alternating white/blue each second, 4s GOP',
        offset='ordinary.mp4 video, +5s container offset; burn-in unchanged'),indent=2))


if __name__=='__main__':main()
