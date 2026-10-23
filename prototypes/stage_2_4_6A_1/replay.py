#!/usr/bin/env python3
"""GPL-2.0-or-later. Fixed-paced replay; visual mode never streams per-frame logs."""
import argparse
import hashlib
import json
import os
from pathlib import Path
import queue
import subprocess
import threading
import time
from benchmark import stats, process_cpu_ms

FIELDS=('display_handoff_ms','queue_ms','command_ack_ms','restart_ms','render_ms',
        'render_call_ms','readback_ms','swap_ms','bmp_ms','stream_log_ms','ui_message_age_ms','render_total_ms','readback_total_ms','seek_pipeline_ms','render_to_handoff_ms')


def summarize(rows,planned):
    for row in rows:
        if 'queue_ms' in row:row['seek_pipeline_ms']=row['restart_ms']-row['queue_ms']
        if 'render_ms' in row:row['render_to_handoff_ms']=row['display_handoff_ms']-row['render_ms']
    by_id={r['id']:r for r in rows}
    for item in planned:
        if item['id'] in by_id:by_id[item['id']]['workload']=item['workload']
    summary={}
    for group in ('first_hover','warm_random','repeat_same','rapid_latest'):
        selected=[r for r in rows if r.get('workload')==group]
        summary[group]={field:stats([r[field] for r in selected if field in r]) if any(field in r for r in selected) else None for field in FIELDS} if selected else None
    summary['planned_requests']=len(planned)
    summary['presented_requests']=len(rows)
    summary['missing_ids']=[p['id'] for p in planned if p['id'] not in by_id]
    summary['missing_required_ids']=[p['id'] for p in planned if p['workload']!='rapid_intermediate' and
        (p['id'] not in by_id or by_id[p['id']].get('latest_at_handoff',p['id'])!=p['id'])]
    summary['intermediate_presentations']=sum(r.get('latest_at_handoff',r['id'])!=r['id'] for r in rows)
    summary['frame_timestamp_verified']=False
    summary['physical_display_verified']=False
    return summary


def run(args):
    lock=json.loads(Path(__file__).with_name('dependencies.json').read_text())
    if hashlib.sha256((Path(args.runtime)/'libmpv-2.dll').read_bytes()).hexdigest()!=lock['dll_sha256']:
        raise ValueError('runtime checksum mismatch')
    output=Path(args.output).resolve()
    if output.exists():raise ValueError('output must be new')
    output.mkdir();exe=Path(args.exe).resolve()
    env=dict(os.environ,PATH=str(Path(args.runtime).resolve())+os.pathsep+os.environ['PATH'])
    planned=[];messages=[];records=[];errors=[];opened=None;child=None;reader=None
    stderr=(output/'stderr.txt').open('wb')
    started=time.perf_counter();ready_wall=None;summary={};session=None;external_cpu=None;external_wall=None
    try:
        command=[str(exe),str(Path(args.media).resolve()),str(output/'engine'),args.renderer,args.hardware,args.seek]
        if not args.legacy:command.append(args.mode)
        child=subprocess.Popen(command,cwd=exe.parent,env=env,
                               stdin=subprocess.PIPE,stdout=subprocess.PIPE,stderr=stderr)
        replies=queue.Queue(maxsize=1024)
        def read():
            for raw in iter(child.stdout.readline,b''):
                try:replies.put(json.loads(raw),timeout=1)
                except (ValueError,queue.Full):child.kill();return
            replies.put(None)
        reader=threading.Thread(target=read,daemon=True);reader.start()
        end=time.monotonic()+30
        while time.monotonic()<end:
            row=replies.get(timeout=max(.01,end-time.monotonic()))
            if row is None:raise ValueError('engine ended before ready')
            messages.append(row)
            if row['type']=='error':raise ValueError(str(row))
            if row['type']=='ready':opened=row;break
        if not opened:raise TimeoutError('initial image')
        ready_wall=(time.perf_counter()-started)*1000
        fractions=(.03,.85,.22,.65,.07,.95,.40,.78,.12,.55)
        def send(target,workload):
            planned.append(dict(id=len(planned)+1,requested_ms=target,workload=workload,
                                parent_sent_ms=(time.perf_counter()-started)*1000))
            child.stdin.write(f'{target:.3f}\n'.encode());child.stdin.flush()
        for i in range(args.samples):
            send(opened['duration_ms']*fractions[i%len(fractions)],'first_hover' if i==0 else 'warm_random')
            time.sleep(args.interval)
        for _ in range(3):
            send(planned[-1]['requested_ms'],'repeat_same');time.sleep(args.interval)
        for cluster in range(args.bursts):
            for i in range(12):
                send(opened['duration_ms']*fractions[(cluster+i)%len(fractions)],
                     'rapid_latest' if i==11 else 'rapid_intermediate')
                time.sleep(.005)
            time.sleep(args.interval)
        time.sleep(args.interval)
        external_cpu=process_cpu_ms(child)
        external_wall=(time.perf_counter()-started)*1000
        child.stdin.write(b'quit\n');child.stdin.flush();child.wait(timeout=8)
        if child.returncode:raise ValueError(f'engine exit {child.returncode}')
    except (ValueError,OSError,TimeoutError,queue.Empty,subprocess.TimeoutExpired) as exc:
        errors.append(str(exc))
    finally:
        if child:
            if child.poll() is None:child.kill();child.wait(timeout=5)
            if reader:reader.join(timeout=2)
            child.stdin.close();child.stdout.close()
            while 'replies' in locals() and not replies.empty():
                row=replies.get_nowait()
                if row:messages.append(row)
        stderr.close()
        journal=output/'engine/events.jsonl'
        if journal.is_file():
            for line in journal.read_text(encoding='utf-8',errors='replace').splitlines():
                try:records.append(json.loads(line))
                except json.JSONDecodeError:errors.append('truncated/invalid journal record')
        frames=[r for r in records if r['type']=='displayed']
        sessions=[r for r in records if r['type']=='session'];session=sessions[-1] if sessions else None
        summary=summarize(frames,planned)
        if summary['missing_required_ids']:errors.append('required paced/latest targets did not produce a candidate')
        if args.mode=='visual' and any(r['readback_ms'] or r['bmp_saved'] or r['stream_log_ms'] for r in frames):
            errors.append('normal visual path performed capture/logging')
        if args.mode=='visual' and any(r.get('type')=='displayed' for r in messages):errors.append('visual streamed per-frame stdout')
        data=dict(media=str(Path(args.media).resolve()),mode=args.mode,legacy=args.legacy,seek=args.seek,renderer=args.renderer,
                  external_cpu_before_quit_ms=external_cpu,external_wall_before_quit_ms=external_wall,
                  discarded_records=[r for r in records if r['type']=='discarded'],
                  hardware_request=args.hardware,opening=opened,process_start_to_ready_ms=ready_wall,
                  records=frames,session=session,summary=summary,planned=planned,errors=errors,
                  metadata=[r for r in records if r['type'] not in ('displayed','discarded','session')],
                  note='paced replay; accepted-input to presentation handoff, not scanout or human hover latency')
        (output/'benchmark.json').write_text(json.dumps(data,indent=2),encoding='utf-8')
        print('::notice title=Visual replay::'+json.dumps(dict(mode=args.mode,renderer=args.renderer,
              legacy=args.legacy,summary=summary,session=session,external_cpu_before_quit_ms=external_cpu,external_wall_before_quit_ms=external_wall,errors=errors),separators=(',',':')))
    if errors:raise ValueError('; '.join(errors))
    return data


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    for name in ('exe','runtime','media','output'):p.add_argument('--'+name,required=True)
    p.add_argument('--legacy',action='store_true',help='unchanged Stage 2.4.6A executable; requires capture mode')
    p.add_argument('--mode',choices=('visual','timing','capture'),default='visual')
    p.add_argument('--renderer',choices=('gl','sw'),default='gl')
    p.add_argument('--hardware',choices=('no','auto-copy'),default='no')
    p.add_argument('--seek',choices=('keyframes','exact'),default='keyframes')
    p.add_argument('--interval',type=float,default=1.0)
    p.add_argument('--samples',type=int,default=12)
    p.add_argument('--bursts',type=int,default=6)
    a=p.parse_args()
    if a.legacy and a.mode!='capture':p.error('legacy mode requires --mode capture')
    if not .1<=a.interval<=10 or not 2<=a.samples<=100 or not 1<=a.bursts<=10:p.error('bounded interval/samples/bursts required')
    run(a)
