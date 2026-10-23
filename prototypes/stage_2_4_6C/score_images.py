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
    by_media={}
    for engine in ('fast','exact','libvlc'):
        for media in sorted({r['media'] for r in rows if r['engine']==engine}):
            cases=[r for r in rows if r['engine']==engine and r['media']==media]
            errors=[r['absolute_error_ms'] for r in cases if r['absolute_error_ms'] is not None]
            by_media[engine+':'+media]=dict(n=len(cases),plausible=len(errors),error=stats(errors) if errors else None)
            print('::notice title=Stage 2.4.6C image cases::'+json.dumps(dict(engine=engine,media=media,
                observations=[dict(requested_ms=r['requested_ms'],observed_ms=r['observed_burned_ms'],error_ms=r['absolute_error_ms']) for r in cases])))
    result = dict(by_media=by_media, scope='Synthetic H.264/Main10 HEVC burned-in PTS OCR; human/device review pending',
                  summary=summary, rows=rows, failures=failures, frame_pts_accuracy_gate_passed=False)
    (args.output/'accuracy.json').write_text(json.dumps(result, indent=2))
    print('::notice title=Stage 2.4.6C burned PTS OCR::'+json.dumps(dict(summary=summary, failures=failures)))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
