#!/usr/bin/env python3
"""GPL-2.0-or-later. Native Windows frame-handoff benchmark, not scanout proof."""
import argparse
import json
import math
import os
from pathlib import Path
import platform
import queue
import statistics
import subprocess
import threading
import time


def stats(values):
    values=sorted(values)
    return dict(n=len(values),median_ms=statistics.median(values),p95_ms=values[math.ceil(.95*len(values))-1])


def process_cpu_ms(child):
    # GetProcessTimes includes every thread; its granularity can make tiny samples zero.
    import ctypes
    from ctypes import wintypes
    kernel=ctypes.WinDLL('kernel32',use_last_error=True)
    function=kernel.GetProcessTimes
    function.argtypes=[wintypes.HANDLE]+[ctypes.POINTER(wintypes.FILETIME)]*4
    function.restype=wintypes.BOOL
    values=[wintypes.FILETIME() for _ in range(4)]
    if not function(int(child._handle),*[ctypes.byref(v) for v in values]):
        raise ctypes.WinError(ctypes.get_last_error())
    return sum((v.dwHighDateTime<<32)+v.dwLowDateTime for v in values[2:])/10000


def run(executable,media,output,renderer,hardware,seek,samples,runtime):
    output=Path(output).resolve()
    if output.exists():raise ValueError('output must be new')
    output.mkdir()
    env=dict(os.environ,PATH=str(Path(runtime).resolve())+os.pathsep+os.environ['PATH'])
    # Start in the executable directory, not an arbitrary DLL search location.
    engine=output/'engine'
    started=time.perf_counter()
    child=subprocess.Popen([str(Path(executable).resolve()),str(Path(media).resolve()),str(engine),renderer,hardware,seek],
                           cwd=Path(executable).resolve().parent,stdin=subprocess.PIPE,stdout=subprocess.PIPE,
                           stderr=(output/'stderr.txt').open('wb'),env=env)
    replies=queue.Queue(maxsize=1024)
    records=[]
    def receive():
        for raw in iter(child.stdout.readline,b''):
            try:replies.put(json.loads(raw),timeout=1)
            except (ValueError,queue.Full):child.kill();break
        replies.put({'type':'eof'})
    reader=threading.Thread(target=receive,daemon=True);reader.start()
    def until(kind,timeout=20):
        end=time.monotonic()+timeout
        while time.monotonic()<end:
            record=replies.get(timeout=max(.01,end-time.monotonic()));records.append(record)
            if record['type'] in ('error','eof'):raise ValueError('engine failure: '+str(record))
            if record['type']==kind:return record
        raise TimeoutError(kind)
    summary={}
    try:
        opened=until('ready',30)
        summary['process_start_to_ready_ms']=(time.perf_counter()-started)*1000
        summary['opening']=opened
        duration=opened['duration_ms']
        # Spread over nearly the full file; deterministic back-and-forth, not adjacency.
        fractions=(.03,.85,.22,.65,.07,.95,.40,.78,.12,.55)
        for index in range(samples):
            sent=time.perf_counter()
            child.stdin.write(f'{duration*fractions[index%len(fractions)]:.3f}\n'.encode());child.stdin.flush()
            row=until('displayed');row['parent_response_ms']=(time.perf_counter()-sent)*1000;row['workload']='first_hover' if index==0 else 'warm_random'
        rapid=[]
        for cluster in range(6):
            before=time.perf_counter()
            for i in range(12):
                target=duration*fractions[(i+cluster)%len(fractions)]
                sent=time.perf_counter()
                child.stdin.write(f'{target:.3f}\n'.encode());child.stdin.flush();time.sleep(.005)
            final_id=samples+(cluster+1)*12
            end=time.monotonic()+10
            while time.monotonic()<end:
                row=until('displayed',10)
                row['workload']='rapid_intermediate'
                if row['id']==final_id:
                    row['workload']='rapid_latest';row['parent_response_ms']=(time.perf_counter()-sent)*1000;rapid.append(row);break
                if row['id']>final_id:raise ValueError('invalid request ordering')
            else:raise TimeoutError('latest rapid target')
            row['cluster_wall_ms']=(time.perf_counter()-before)*1000
        displayed=[r for r in records if r['type']=='displayed']
        for name in ('first_hover','warm_random','rapid_latest'):
            group=[r for r in displayed if r.get('workload')==name]
            summary[name]={field:stats([r[field] for r in group]) for field in
                           ('display_handoff_ms','parent_response_ms','command_ack_ms','render_ms','cpu_ms')}
        summary['memory']={'peak_private_bytes':max(r['private_bytes'] for r in displayed),
                           'peak_working_set_bytes':max(r['working_set_bytes'] for r in displayed)}
        summary['playback_position_deltas_ms']=[abs(r['playback_position_ms']-r['requested_ms']) for r in displayed
                                               if r['playback_position_ms']>=0]
        summary['hwdec_current']=sorted(set(r['hwdec_current'] for r in displayed))
        summary['frame_timestamp_verified']=False
        summary['physical_display_verified']=False
        summary['session_process_cpu_ms']=process_cpu_ms(child)
        summary['session_wall_ms']=(time.perf_counter()-started)*1000
        summary['session_cpu_equivalent_cores']=summary['session_process_cpu_ms']/summary['session_wall_ms']
        child.stdin.write(b'quit\n');child.stdin.flush();child.wait(timeout=8)
        if child.returncode:raise ValueError('engine teardown failed')
    finally:
        if child.poll() is None:child.kill();child.wait(timeout=5)
        reader.join(timeout=2)
        child.stdin.close();child.stdout.close()
        data=dict(environment=dict(platform=platform.platform(),processor=platform.processor(),logical_cpus=os.cpu_count()),
                  renderer=renderer,hardware_request=hardware,seek=seek,media=str(Path(media).resolve()),
                  summary=summary,records=records)
        (output/'benchmark.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
    print('::notice title=libmpv runtime::'+json.dumps(dict(environment=data['environment'],media=Path(media).name,runtime=[r for r in records if r['type'] in ('mpv-version','ffmpeg-version','mpv-configuration','opengl_renderer')]),separators=(',',':')))
    print('::notice title=libmpv benchmark::'+json.dumps(dict(media=Path(media).name,renderer=renderer,seek=seek,summary=summary),separators=(',',':')))
    return data


if __name__=='__main__':
    parser=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','media','output','runtime'):parser.add_argument('--'+name,required=True)
    parser.add_argument('--renderer',choices=('gl','sw'),default='gl')
    parser.add_argument('--hardware',choices=('no','auto-copy'),default='no')
    parser.add_argument('--seek',choices=('keyframes','exact'),default='keyframes')
    parser.add_argument('--samples',type=int,default=30)
    args=parser.parse_args()
    if not 10<=args.samples<=100:parser.error('samples must be 10..100')
    run(args.exe,args.media,args.output,args.renderer,args.hardware,args.seek,args.samples,args.runtime)
