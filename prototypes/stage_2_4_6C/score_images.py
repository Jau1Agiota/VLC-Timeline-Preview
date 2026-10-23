#!/usr/bin/env python3
"""GPL-2.0-or-later. Provisional image-content OCR, never a frame-PTS API claim."""
import argparse
import json
import os
from pathlib import Path
import re
import subprocess
import tempfile
import sys
import struct
sys.path.insert(0,str(Path(__file__).resolve().parents[1]/"stage_2_4_6B"))
from test_worker import stats


def parse_pts(text):
    match = re.search(r'(?<!\d)(\d{2}):(\d{2}):(\d{2})\.(\d{3})(?!\d)', text)
    if not match:
        return None
    hours, minutes, seconds, ms = map(int, match.groups())
    if minutes >= 60 or seconds >= 60:
        return None
    return ((hours*60+minutes)*60+seconds)*1000+ms


def scene_id(path):
    data=Path(path).read_bytes()
    if len(data)>320*180*4+54 or len(data)<54 or data[:2]!=b'BM':raise ValueError('bounded BMP required')
    width,height=struct.unpack_from('<ii',data,18)
    offset=struct.unpack_from('<I',data,10)[0]
    if width!=320 or abs(height)!=180 or struct.unpack_from('<H',data,28)[0]!=32:raise ValueError('unexpected scene probe BMP')
    if not 54<=offset<=len(data)-320*180*4:raise ValueError('invalid scene pixel payload')
    y=130 if height<0 else 179-130
    sample=data[offset+(y*320+160)*4:][:3]
    return 'white' if len(sample)==3 and min(sample)>220 else 'blue'


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('evidence', type=Path)
    parser.add_argument('fixtures', type=Path)
    parser.add_argument('output', type=Path)
    args = parser.parse_args()
    args.output.mkdir()  # Never overwrite evidence.
    report = json.loads((args.evidence/'results.json').read_text())
    rows, failures, known = [], [], {}
    environment = dict(os.environ, OMP_THREAD_LIMIT='1')
    for engine, source in [('candidate', report['rows']), ('libvlc', report['libvlc'])]:
        for record in source:
            if not record.get('bmp'):
                continue
            engine = record.get('mode',engine)
            media = record['media']
            image = args.evidence/record['bmp']
            if Path(record['bmp']).name != record['bmp'] or ('..' in Path(media).parts or Path(media).is_absolute()):
                raise ValueError('evidence path must be a basename')
            try:
                stem = 'ordinary' if Path(media).stem == 'offset' else str(Path(media).with_suffix(''))
                if stem not in known:
                    # MKV is a remux of this MP4; visible burn-in predates any
                    # container timestamp shift. Compare to the source MP4 PTS.
                    frames = json.loads(subprocess.check_output([
                        'ffprobe', '-v', 'error', '-select_streams', 'v:0', '-show_frames',
                        '-show_entries', 'frame=best_effort_timestamp_time', '-of', 'json',
                        str(args.fixtures/(stem+'.mp4'))], timeout=30))['frames']
                    known[stem] = [float(f['best_effort_timestamp_time'])*1000 for f in frames]
                with tempfile.TemporaryDirectory(prefix='mpv-burned-pts-') as folder:
                    crop = Path(folder)/'crop.png'
                    subprocess.run(['ffmpeg', '-nostdin', '-v', 'error', '-threads', '1',
                                    '-filter_threads', '1', '-i', str(image), '-vf',
                                    'crop=iw:30:0:0,scale=1280:120', '-frames:v', '1', str(crop)],
                                   check=True, timeout=30)
                    text = subprocess.check_output(['tesseract', str(crop), 'stdout', '--psm', '7',
                            '-c', 'tessedit_char_whitelist=0123456789:.'], text=True,
                            stderr=subprocess.DEVNULL, env=environment, timeout=30).strip()
                observed = parse_pts(text)
                plausible = observed is not None and min(abs(t-observed) for t in known[stem]) <= 2
                rows.append(dict(engine=engine, media=media, image=image.name,
                    requested_ms=record['requested_ms'], observed_burned_ms=observed,
                    workload=record.get('workload'),scene_id=scene_id(image) if media=='scene_cuts.mp4' else None,
                    ocr_text=text, plausible_source_time=plausible,
                    absolute_error_ms=abs(observed-record['requested_ms']) if plausible else None,
                    frame_timestamp_verified=False, human_review_pending=True))
            except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
                failures.append(dict(engine=engine, image=image.name, error=str(error)))
    summary = {}
    for engine in ('fast','exact','libvlc'):
        cases = [r for r in rows if r['engine'] == engine]
        errors = [r['absolute_error_ms'] for r in cases if r['absolute_error_ms'] is not None]
        summary[engine] = dict(images=len(cases), plausible_ocr=len(errors),
            provisional_absolute_error=stats(errors) if errors else None,
            within_100_ms=sum(e <= 100 for e in errors), within_250_ms=sum(e <= 250 for e in errors), human_review_pending=True)
    cut_reference={r['requested_ms']:r['scene_id'] for r in rows if r['engine']=='libvlc' and r['media']=='scene_cuts.mp4'}
    scene_comparison={}
    for engine in ('fast','exact'):
        paired=[r for r in rows if r['engine']==engine and r['media']=='scene_cuts.mp4' and r['requested_ms'] in cut_reference and r['workload']!='rapid']
        scene_comparison[engine]=dict(n=len(paired),matches=sum(r['scene_id']==cut_reference[r['requested_ms']] for r in paired),
            cases=[dict(request_ms=r['requested_ms'],scene=r['scene_id'],libvlc_scene=cut_reference[r['requested_ms']]) for r in paired],
            reference='isolated LibVLC start-time probe, not actual Qt click')
    print('::notice title=Stage 2.4.6C cut alignment::'+json.dumps(scene_comparison))
    by_media={}
    for engine in ('fast','exact','libvlc'):
        for media in sorted({r['media'] for r in rows if r['engine']==engine}):
            cases=[r for r in rows if r['engine']==engine and r['media']==media]
            errors=[r['absolute_error_ms'] for r in cases if r['absolute_error_ms'] is not None]
            by_media[engine+':'+media]=dict(n=len(cases),plausible=len(errors),error=stats(errors) if errors else None)
            print('::notice title=Stage 2.4.6C image cases::'+json.dumps(dict(engine=engine,media=media,
                observations=[dict(requested_ms=r['requested_ms'],observed_ms=r['observed_burned_ms'],error_ms=r['absolute_error_ms']) for r in cases])))
    result = dict(scene_comparison=scene_comparison,by_media=by_media, scope='Synthetic H.264/Main10 HEVC burned-in PTS OCR; human/device review pending',
                  summary=summary, rows=rows, failures=failures, frame_pts_accuracy_gate_passed=False)
    (args.output/'accuracy.json').write_text(json.dumps(result, indent=2))
    print('::notice title=Stage 2.4.6C burned PTS OCR::'+json.dumps(dict(summary=summary, failures=failures)))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
