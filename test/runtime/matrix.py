#!/usr/bin/env python3
"""GPL-2.0-or-later. Compare unchanged workers; image handoff is not GUI acceptance."""
import argparse
import hashlib
import json
import math
import os
from pathlib import Path
import re
import statistics
import struct
import subprocess
import time


def parse(raw, targets):
    results = []
    offset = 0
    for ident, target in enumerate(targets, 1):
        end = raw.find(b'\n', offset, offset + 1026)
        if end < 0:
            raise ValueError('missing bounded header')
        header = json.loads(raw[offset:end])
        offset = end + 1
        if (header.get('protocol') != 1 or header.get('bytes') != 230400 or
                header.get('requested_ms') != target or header.get('request_id') != ident or
                header.get('frame_timestamp_verified') is not False):
            raise ValueError('worker protocol mismatch')
        pixels = raw[offset:offset + 230400]
        if len(pixels) != 230400:
            raise ValueError('short frame')
        offset += 230400
        header['pixels_sha256'] = hashlib.sha256(pixels).hexdigest()
        header['image_pts_reviewed'] = False
        results.append((header, pixels))
    if offset != len(raw):
        raise ValueError('trailing output')
    return results


def bmp(path, pixels):
    path.write_bytes(struct.pack('<2sIHHI', b'BM', 54 + len(pixels), 0, 0, 54) +
                     struct.pack('<IiiHHIIiiII', 40, 320, -180, 1, 32, 0, len(pixels), 0, 0, 0, 0) + pixels)


def stats(values):
    values = sorted(values)
    return dict(n=len(values), median_ms=statistics.median(values),
                p95_ms=values[math.ceil(.95 * len(values)) - 1]) if values else None


def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument('--candidate', type=Path, required=True)
    p.add_argument('--baseline', type=Path, required=True)
    p.add_argument('--bridge', type=Path, required=True)
    p.add_argument('--fixtures', type=Path, required=True)
    p.add_argument('--new-output', type=Path, required=True)
    p.add_argument('--wine', type=Path)
    p.add_argument('--wine-env', type=Path)
    a = p.parse_args()
    a.new_output.mkdir(exist_ok=False)
    env = dict(os.environ)
    if a.wine_env:
        env.update(json.loads(a.wine_env.read_text()))
    def win(path):
        return 'Z:' + str(path.resolve()) if a.wine else str(path.resolve())
    media = ['ordinary.mp4', 'ordinary.mkv', 'bframes.mp4', 'bframes.mkv',
             'long_gop.mp4', 'long_gop.mkv', 'vfr.mp4', 'vfr.mkv',
             'hevc/hevc10.mp4', 'hevc/hevc10.mkv', 'offset.mp4', 'scene_cuts.mp4']
    targets = [500, 25000, 1000, 15000, 28000, 5000, 29500, 1000]
    records = []
    for label, folder in [('baseline', a.baseline), ('candidate', a.candidate)]:
        for worker in ['mpv-thumbnail-accurate-worker.exe', 'mpv-thumbnail-worker.exe']:
            for name in media:
                case = a.new_output / f'{label}-{worker[:-4]}-{name.replace("/", "-")}'
                case.mkdir()
                command = ([str(a.wine)] if a.wine else []) + [str(a.bridge), win(folder / worker),
                                                              win(a.fixtures / name), *map(str, targets)]
                start = time.monotonic()
                row = dict(runtime=label, worker=worker, media=name, targets=targets,
                           native_windows=not bool(a.wine), timestamp_accuracy_verified=False,
                           dll_sha256=hashlib.sha256((folder / 'libmpv-2.dll').read_bytes()).hexdigest(),
                           worker_sha256=hashlib.sha256((folder / worker).read_bytes()).hexdigest())
                try:
                    result = subprocess.run(command, capture_output=True, env=env, timeout=90)
                    (case / 'stderr.txt').write_bytes(result.stderr)
                    row.update(exit_code=result.returncode, process_elapsed_ms=(time.monotonic()-start)*1000)
                    if result.returncode:
                        raise RuntimeError('bridge/worker exit ' + str(result.returncode))
                    frames = parse(result.stdout, targets)
                    bridge = re.findall(rb'bridge request=(\d+) image_ms=(\d+)', result.stderr)
                    if len(bridge) != len(targets):
                        raise ValueError('missing image handoff timing')
                    row['loaded_modules'] = sorted(set(x.decode('ascii') for x in re.findall(rb'bridge-module=([^\r\n]+)', result.stderr)))
                    row['frames'] = []
                    for i, (header, pixels) in enumerate(frames):
                        header['bridge_image_ms'] = int(bridge[i][1])
                        row['frames'].append(header)
                        bmp(case / f'{i+1:02d}-{targets[i]}.bmp', pixels)
                    row['success'] = True
                except (RuntimeError, ValueError, subprocess.TimeoutExpired) as error:
                    row.update(success=False, error=str(error))
                records.append(row)
                (a.new_output / 'raw.json').write_text(json.dumps(records, indent=2)+'\n')
    summary = dict(native_windows_tested=not bool(a.wine), cases=len(records),
                   succeeded=sum(r['success'] for r in records), frame_accuracy_verified=False,
                   measurements='bounded native-pipe image handoff; not perceived hover/UI latency', groups={})
    for label in ['baseline', 'candidate']:
        for worker in ['mpv-thumbnail-accurate-worker.exe', 'mpv-thumbnail-worker.exe']:
            frames = [r['frames'] for r in records if r['success'] and r['runtime']==label and r['worker']==worker]
            summary['groups'][label+'-'+worker] = dict(
                first_request=stats([f[0]['bridge_image_ms'] for f in frames]),
                warm_requests=stats([h['bridge_image_ms'] for f in frames for h in f[1:]]))
    (a.new_output / 'summary.json').write_text(json.dumps(summary, indent=2)+'\n')
    print(json.dumps(summary, indent=2))
    return 0 if summary['succeeded'] == len(records) else 1


if __name__ == '__main__':
    raise SystemExit(main())
