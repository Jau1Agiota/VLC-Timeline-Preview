#!/usr/bin/env python3
"""GPL-2.0-or-later. Provisional OCR of image content, separate from player clocks."""
import argparse
import json
import re
from pathlib import Path, PureWindowsPath
import subprocess
import tempfile
from benchmark import stats


def parse_pts(text):
    match=re.search(r'(?<!\d)(\d{2}):(\d{2}):(\d{2})[.](\d{3})(?!\d)',text)
    if not match:return None
    h,m,s,ms=map(int,match.groups())
    if m>=60 or s>=60:return None
    return ((h*60+m)*60+s)*1000+ms


def score(evidence,fixtures,output):
    output=Path(output)
    if output.exists():raise ValueError('output must be new')
    output.mkdir();all_rows=[];summaries=[]
    reports=sorted(Path(evidence).rglob('benchmark.json'))+sorted(Path(evidence).rglob('comparison.json'))
    failures=[]
    for report in reports:
        try:
            score_report(report,evidence,fixtures,all_rows,summaries)
        except (KeyError,ValueError,OSError,subprocess.CalledProcessError,subprocess.TimeoutExpired) as exc:
            failure=dict(report=str(report.relative_to(evidence)),error=str(exc))
            failures.append(failure)
            print('::error title=Image analysis failed::'+json.dumps(failure))
    if not reports:failures.append(dict(error='no raw benchmark reports'))
    (output/'accuracy.json').write_text(json.dumps(dict(rows=all_rows,summary=summaries,failures=failures),indent=2))
    if failures:raise ValueError('image analysis incomplete; see accuracy.json')


def score_report(report,evidence,fixtures,all_rows,summaries):
    source=json.loads(report.read_text());media=PureWindowsPath(source['media']).name
    kind=Path(media).stem
    known=json.loads((Path(fixtures)/(kind+'.mp4.frames.json')).read_text())['frames']
    valid=[float(f['best_effort_timestamp_time'])*1000 for f in known]
    rows=[]
    for record in source['records']:
        if not record.get('bmp_saved'):continue
        image=report.parent/('engine' if report.name=='benchmark.json' else '')/f"frame-{record['id']:06d}.bmp"
        if not image.is_file():continue
        with tempfile.TemporaryDirectory(prefix='burned-pts-') as tmp:
            crop=Path(tmp)/'crop.png'
            subprocess.run(['ffmpeg','-nostdin','-v','error','-i',str(image),'-vf',
                            'crop=iw:30:0:0,scale=1280:120','-frames:v','1',str(crop)],check=True,timeout=30)
            text=subprocess.check_output(['tesseract',str(crop),'stdout','--psm','7',
                                          '-c','tessedit_char_whitelist=0123456789:.'],stderr=subprocess.DEVNULL,text=True,timeout=30).strip()
        observed=parse_pts(text)
        plausible=observed is not None and min(abs(t-observed) for t in valid)<=2
        row=dict(report=str(report.relative_to(evidence)),media=media,seek=source['seek'],id=record['id'],
                 requested_ms=record['requested_ms'],ocr_text=text,observed_burned_ms=observed,
                 observation_method='automated burned-in PTS OCR; human spot review pending',
                 plausible_source_frame=plausible,frame_timestamp_verified=False,
                 absolute_error_ms=abs(observed-record['requested_ms']) if plausible else None)
        rows.append(row);all_rows.append(row)
    errors=[r['absolute_error_ms'] for r in rows if r['absolute_error_ms'] is not None]
    summary=dict(report=str(report.relative_to(evidence)),media=media,seek=source['seek'],
                 mode=source.get('mode','unknown'),legacy=source.get('legacy',False),images=len(rows),ocr_plausible=len(errors),
                 provisional_absolute_error=stats(errors) if errors else None,human_review_pending=True)
    summaries.append(summary)
    print('::notice title=Burned-image OCR (provisional)::'+json.dumps(summary))


if __name__=='__main__':
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('evidence');p.add_argument('fixtures');p.add_argument('output')
    a=p.parse_args();score(Path(a.evidence),a.fixtures,a.output)
