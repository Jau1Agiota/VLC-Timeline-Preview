#!/usr/bin/env python3
"""GPL-2.0-or-later. Unmodified Stage 2.4.5 helper, same file/targets as mpv."""
import argparse
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import time
from benchmark import stats


def compare(helper,mpv_benchmark,output):
    reference=json.loads(Path(mpv_benchmark).read_text())
    output=Path(output).resolve()
    if output.exists():raise ValueError('output must be new')
    output.mkdir();image=output/'candidate.bmp'
    helper=Path(helper).resolve()
    child=subprocess.Popen([str(helper),'--worker',reference['media'],str(image),'baseline'],cwd=helper.parent,
                           stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=(output/'stderr.txt').open('wb'))
    responses=queue.Queue(maxsize=2)
    def reader():
        for raw in iter(child.stdout.readline,b''):
            try:responses.put(json.loads(raw),timeout=1)
            except (ValueError,queue.Full):child.kill();return
        responses.put(None)
    thread=threading.Thread(target=reader,daemon=True);thread.start()
    rows=[]
    try:
        expected=[r for r in reference['records'] if r.get('workload') in ('first_hover','warm_random')]
        for index,target in enumerate(expected):
            started=time.perf_counter()
            child.stdin.write(f"{index+1} {round(target['requested_ms'])} 5000\n".encode());child.stdin.flush()
            row=responses.get(timeout=8)
            if not row or row.get('exit_code')!=0 or row.get('request_id')!=index+1 or not image.is_file():
                raise ValueError('Stage 2.4.5 comparison failure')
            row['response_wall_ms']=(time.perf_counter()-started)*1000
            row['mpv_requested_ms']=target['requested_ms']
            row['input_rounding_ms']=round(target['requested_ms'])-target['requested_ms']
            row['workload']='first_hover' if index==0 else 'warm_random'
            row['id']=index+1;row['bmp_saved']=True
            image.rename(output/f'frame-{index+1:06d}.bmp');rows.append(row)
        child.stdin.close();child.wait(timeout=7)
        if child.returncode:raise ValueError('Stage 2.4.5 shutdown failed')
    finally:
        if child.poll() is None:child.kill();child.wait(timeout=5)
        thread.join(timeout=2);child.stdout.close()
        if not child.stdin.closed:child.stdin.close()
        data=dict(media=reference['media'],seek='libvlc-accurate',records=rows,
                  summary={group:{field:stats([r[field] for r in rows if r['workload']==group])
                                  for field in ('response_wall_ms','capture_latency_ms','total_latency_ms')}
                           for group in ('first_hover','warm_random')} if len(rows)==len(expected) else {})
        (output/'comparison.json').write_text(json.dumps(data,indent=2))
    print('::notice title=Stage 2.4.5 same-file comparison::'+json.dumps(dict(media=Path(reference['media']).name,
          note='LibVLC accurate seek vs explicit mpv seek mode; identical file/targets within sub-ms rounding; response not screen latency',summary=data['summary'])))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--helper',required=True);p.add_argument('--mpv-benchmark',required=True);p.add_argument('--output',required=True)
    a=p.parse_args();compare(a.helper,a.mpv_benchmark,a.output)
