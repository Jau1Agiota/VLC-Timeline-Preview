#!/usr/bin/env python3
"""GPL-2.0-or-later. Isolated native seek comparison, image evidence separate from clocks."""
import argparse
from concurrent.futures import ThreadPoolExecutor
import json
from pathlib import Path
import sys
import time
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/'stage_2_4_6B'))
from test_worker import Worker, bmp, stats, compare_libvlc


def main():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--fast',type=Path,required=True)
    p.add_argument('--exact',type=Path,required=True)
    p.add_argument('--libvlc',type=Path,required=True)
    p.add_argument('--fixtures',type=Path,required=True)
    p.add_argument('--output',type=Path,required=True)
    a=p.parse_args();a.output.mkdir()
    media=[f'{s}.{e}' for s in ('ordinary','bframes','long_gop','vfr') for e in ('mp4','mkv')]
    media+=['hevc/hevc10.mp4','hevc/hevc10.mkv','scene_cuts.mp4','offset.mp4']
    rows=[];failures=[];libvlc=[]
    def record(mode, name, workload, result, intent=None, latest=None, received=None, emitted=None):
        h,pixels=result
        if mode=='exact' and (h.get('seek_strategy')!='exact' or
                'actual_image_timestamp_ms' not in h or h['actual_image_timestamp_ms'] is not None or
                h.get('candidate_state')!='provisional'):
            raise RuntimeError('invalid exact provenance')
        h.update(mode=mode,media=name,workload=workload,actual_image_timestamp_ms=None,
                 candidate_state='provisional',intent_sequence=intent,latest_intent=latest,
                 intent_to_candidate_ms=(received-emitted)*1000 if received is not None else None)
        image=f'{mode}-{len(rows):05d}.bmp';bmp(a.output/image,pixels);h['bmp']=image
        if name=='scene_cuts.mp4':
            sample=pixels[(130*320+160)*4:][:3]
            h['scene_id']='white' if min(sample)>220 else 'blue'
            h['requested_scene_id']='white' if (h['requested_ms']//1000)%2==0 else 'blue'
        rows.append(h)
    tasks=[(name,mode) for index,name in enumerate(media)
           for mode in (('fast','exact') if index%2==0 else ('exact','fast'))]
    for name,mode in tasks:
        program=a.fast if mode=='fast' else a.exact
        worker=Worker(program.resolve(),(a.fixtures/name).resolve())
        try:
            targets=[1000,25000,5000,18000,29950,50,23973]
            if name=='scene_cuts.mp4':targets+=[965,999,1001,1035,1965,1999,2001,2035]
            for index,target in enumerate(targets):
                record(mode,name,'first' if index==0 else 'boundary' if index>=7 else 'warm',worker.request(target))
            # One active request and only the newest pending intent, 20 distant
            # hover intents at a fixed cadence. This is a scheduler benchmark,
            # not Qt painting or physical perceived responsiveness.
            with ThreadPoolExecutor(max_workers=1) as pool:
                active=None;active_seq=-1;emitted={};latest=-1
                def extract(target):
                    result=worker.request(target);return result,time.perf_counter()
                for seq in range(20):
                    fire=time.perf_counter();latest=seq;emitted[seq]=fire
                    if active is not None and active.done():
                        result,end=active.result();record(mode,name,'rapid',result,active_seq,latest,end,emitted[active_seq]);active=None
                    if active is None:
                        active_seq=latest
                        active=pool.submit(extract,(1000,25000,5000,18000)[latest%4])
                    time.sleep(max(0,fire+.005-time.perf_counter()))
                while active is not None:
                    result,end=active.result(timeout=18)
                    record(mode,name,'rapid',result,active_seq,latest,end,emitted[active_seq])
                    if active_seq==latest:break
                    active_seq=latest;active=pool.submit(extract,(1000,25000,5000,18000)[latest%4])
        except Exception as error:
            failures.append(dict(mode=mode,media=name,error=str(error)))
        finally:
            try:worker.close()
            except Exception as error:failures.append(dict(mode=mode,media=name,cleanup_error=str(error)))
        if worker.p.returncode!=0:failures.append(dict(mode=mode,media=name,exit_code=worker.p.returncode))
    for name in media:
        try:
            records=compare_libvlc([str(a.libvlc.resolve())],(a.fixtures/name).resolve(),
                                  (a.output/('libvlc-'+name.replace('/','-')+'.bmp')).resolve())
            for row in records:
                row.update(media=name,mode='libvlc',workload='first' if row['request_id']==1 else 'warm')
                libvlc.append(row)
        except Exception as error:failures.append(dict(mode='libvlc',media=name,error=str(error)))
    summary={}
    for mode in ('fast','exact'):
        selected=[r for r in rows if r['mode']==mode]
        summary[mode]={}
        for workload in ('first','warm','boundary','rapid'):
            cases=[r for r in selected if r['workload']==workload]
            summary[mode][workload]=stats([r['received_ms'] for r in cases]) if cases else None
        finals=[r['intent_to_candidate_ms'] for r in selected if r['workload']=='rapid' and r['intent_sequence']==19]
        summary[mode]['rapid_final']=stats(finals) if finals else None
        for label in ('hevc','ordinary','long_gop','vfr'):
            cases=[r for r in selected if label in r['media'] and r['workload']=='warm']
            summary[mode][label]=stats([r['received_ms'] for r in cases]) if cases else None
        summary[mode]['max_sampled_private_bytes']=max((r.get('private_bytes',0) for r in selected),default=0)
        summary[mode]['cpu_total_ms_by_media']={name:max([r.get('cpu_total_ms',-1) for r in selected if r['media']==name],default=-1) for name in media}
    output=dict(scope='Native Windows synthetic candidate receipt, not physical display or real movie',
                rows=rows,libvlc=libvlc,summary=summary,failures=failures,
                actual_frame_pts_available=False,physical_display_verified=False,gpu_usage_measured=False)
    (a.output/'results.json').write_text(json.dumps(output,indent=2))
    # Keep complete notices below GitHub's per-message truncation boundary.
    for mode in summary:print('::notice title=Stage 2.4.6C '+mode+' performance::'+json.dumps(summary[mode]))
    print('::notice title=Stage 2.4.6C failures::'+json.dumps(failures))
    return bool(failures)


if __name__=='__main__':raise SystemExit(main())
