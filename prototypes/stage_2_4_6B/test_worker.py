#!/usr/bin/env python3
"""Native Windows worker protocol/latency smoke; never asserts frame PTS or GUI display.
GPL-2.0-or-later. Outputs must be new. BMPs are test evidence only.
"""
import argparse
import hashlib
import json
import math
from pathlib import Path
import queue
import shutil
import struct
import subprocess
import threading
import time


def exact(stream, length):
    parts = bytearray()
    while len(parts) < length:
        chunk = stream.read(length-len(parts))
        if not chunk:
            raise RuntimeError('worker EOF before payload completed')
        parts.extend(chunk)
    return bytes(parts)


class Worker:
    def __init__(self, program, media):
        self.start = time.perf_counter()
        self.p = subprocess.Popen([str(program), str(media)], stdin=subprocess.PIPE,
                                  stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
                                  creationflags=getattr(subprocess, 'CREATE_NO_WINDOW', 0))
        self.frames = queue.Queue(maxsize=2)
        def receive():
            try:
                while True:
                    line = self.p.stdout.readline(1026)
                    if not line or len(line) > 1025 or not line.endswith(b'\n'):
                        raise RuntimeError('missing/bounded header')
                    h = json.loads(line)
                    if h.get('protocol') != 1 or h.get('bytes') != 230400 or h.get('frame_timestamp_verified') is not False:
                        raise RuntimeError('invalid protocol')
                    self.frames.put((h, exact(self.p.stdout, 230400)), timeout=1)
            except Exception as error:
                try:
                    self.frames.put(error, timeout=1)
                except queue.Full:
                    pass
        self.reader = threading.Thread(target=receive, daemon=True)
        self.reader.start()
        self.id = 0

    def request(self, target):
        self.id += 1
        start = time.perf_counter()
        self.p.stdin.write(f'{self.id} {target} 5000\n'.encode())
        self.p.stdin.flush()
        result = self.frames.get(timeout=18)
        if isinstance(result, Exception):
            raise result
        header, pixels = result
        if header['request_id'] != self.id or header['requested_ms'] != target:
            raise RuntimeError('mismatched candidate')
        header.update(received_ms=(time.perf_counter()-start)*1000,
                      open_elapsed_ms=(time.perf_counter()-self.start)*1000,
                      physical_display_verified=False)
        return header, pixels

    def close(self):
        self.p.stdin.close()
        try:
            self.p.wait(timeout=7)
        except subprocess.TimeoutExpired:
            self.p.kill()
            self.p.wait(timeout=3)
            raise RuntimeError('worker cleanup exceeded deadline')
        self.reader.join(timeout=1)
        self.p.stdout.close()


def bmp(path, pixels):
    # Negative height preserves SW-render top-down row order.
    Path(path).write_bytes(struct.pack('<2sIHHI', b'BM', 54+len(pixels), 0, 0, 54)+
                          struct.pack('<IiiHHIIiiII', 40, 320, -180, 1, 32, 0, len(pixels), 0, 0, 0, 0)+pixels)


def stats(values):
    values = sorted(values)
    return {'n': len(values), 'median_ms': (values[(len(values)-1)//2]+values[len(values)//2])/2,
            'p95_ms': values[math.ceil(.95*len(values))-1]}


def compare_libvlc(command, media, output):
    """Mirror Controller::launch: consume candidate, preserve it, then remove its
    working BMP before requesting another. CREATE_NEW is a helper safety guard.
    """
    process = subprocess.Popen(command+['--worker', str(media), str(output), 'baseline'],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL)
    answers = queue.Queue(maxsize=4)
    def receive():
        try:
            while True:
                line = process.stdout.readline(4097)
                if not line or len(line) > 4096 or not line.endswith(b'\n'):
                    raise RuntimeError('LibVLC missing/bounded result line')
                answers.put(json.loads(line), timeout=1)
        except Exception as error:
            try:
                answers.put(error, timeout=1)
            except queue.Full:
                pass
    reader = threading.Thread(target=receive, daemon=True)
    reader.start()
    records = []
    try:
        for ident, target in enumerate((1000, 25000, 5000), 1):
            # output belongs only to this new results directory, never a user's file.
            output.unlink(missing_ok=True)
            begin = time.perf_counter()
            process.stdin.write(f'{ident} {target} 5000\n'.encode())
            process.stdin.flush()
            record = answers.get(timeout=18)
            if isinstance(record, Exception):
                raise record
            if record.get('request_id') != ident or record.get('requested_ms') != target or record.get('exit_code') != 0:
                raise RuntimeError(f'LibVLC candidate failed: {record}')
            if not output.is_file() or not 54 < output.stat().st_size <= 320*180*4+54:
                raise RuntimeError('LibVLC missing/bounded BMP')
            record.update(media=media.name, received_ms=(time.perf_counter()-begin)*1000,
                          physical_display_verified=False)
            preserved = output.with_name(output.stem+f'-{ident}.bmp')
            if preserved.exists():
                raise RuntimeError('comparison evidence must be new')
            shutil.copyfile(output, preserved)
            record['bmp'] = preserved.name
            records.append(record)
        process.stdin.close()
        process.wait(timeout=7)
        if process.returncode:
            raise RuntimeError(f'LibVLC exit {process.returncode}')
    finally:
        if process.poll() is None:
            process.kill()
            process.wait(timeout=3)
        if not process.stdin.closed:
            process.stdin.close()
        reader.join(timeout=1)
        process.stdout.close()
    return records


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('worker', type=Path)
    parser.add_argument('fixtures', type=Path)
    parser.add_argument('output', type=Path)
    parser.add_argument('--libvlc', type=Path)
    args = parser.parse_args()
    args.output.mkdir()  # refuse overwrite
    rows, failures, comparison = [], [], []
    for stem in ('ordinary', 'bframes', 'long_gop', 'vfr'):
        for ext in ('mp4', 'mkv'):
            media = (args.fixtures/f'{stem}.{ext}').resolve()
            worker = Worker(args.worker.resolve(), media)
            try:
                # Widely separated targets, including near end; same persistent process.
                for target in (1000, 25000, 5000, 18000, 29500, 2000, 23000):
                    h, pixels = worker.request(target)
                    h['media'] = media.name
                    h['bmp'] = f'{stem}-{ext}-{worker.id}.bmp'
                    bmp(args.output/h['bmp'], pixels)
                    rows.append(h)
            except Exception as error:
                failures.append({'media': media.name, 'error': str(error)})
            finally:
                worker.close()
            if worker.p.returncode != 0:
                failures.append({'media': media.name, 'exit_code': worker.p.returncode})
            if args.libvlc:
                try:
                    comparison.extend(compare_libvlc([str(args.libvlc.resolve())], media,
                        (args.output/f'libvlc-{stem}-{ext}.bmp').resolve()))
                except Exception as error:
                    failures.append({'libvlc': media.name, 'error': str(error)})
    # Invalid input must fail boundedly, not emit a candidate. Audio-only is startup-timeout bounded.
    for name in ('corrupt.mp4', 'audio_only.mp4'):
        worker = Worker(args.worker.resolve(), (args.fixtures/name).resolve())
        try:
            worker.request(1000)
            failures.append({'media': name, 'error': 'unexpected video candidate'})
        except (RuntimeError, queue.Empty, BrokenPipeError):
            pass
        finally:
            worker.close()
        if worker.p.returncode == 0:
            failures.append({'media': name, 'error': 'unexpected success exit'})
    # Closing stdin and forced crash during active request cannot leave this child running.
    worker = Worker(args.worker.resolve(), (args.fixtures/'ordinary.mp4').resolve())
    worker.p.stdin.write(b'1 25000 5000\n'); worker.p.stdin.flush(); worker.p.kill(); worker.close()
    first = [r['received_ms'] for r in rows if r['request_id'] == 1]
    warm = [r['received_ms'] for r in rows if r['request_id'] > 1]
    matched = [r['received_ms'] for r in rows if r['request_id'] in (2, 3)]
    lvfirst = [r['received_ms'] for r in comparison if r.get('request_id') == 1]
    lvwarm = [r['received_ms'] for r in comparison if r.get('request_id') in (2, 3)]
    renders = [r['render_ms'] for r in rows]
    transfers = [r['previous_pipe_write_ms'] for r in rows if r.get('previous_pipe_write_ms', -1) >= 0]
    report = {'scope': 'native Windows synthetic SW worker; not Qt/physical display or real movie',
              'worker_sha256': hashlib.sha256(args.worker.read_bytes()).hexdigest(),
              'rows': rows, 'failures': failures, 'libvlc': comparison,
              'first': stats(first) if first else None, 'warm': stats(warm) if warm else None,
              'matched_warm_mpv': stats(matched) if matched else None,
              'libvlc_first': stats(lvfirst) if lvfirst else None,
              'libvlc_warm': stats(lvwarm) if lvwarm else None,
              'render': stats(renders) if renders else None,
              'previous_pipe_write': stats(transfers) if transfers else None,
              'max_sampled_private_bytes': max((r.get('private_bytes', 0) for r in rows), default=0) or None,
              'cpu_ms_by_media': {r['media']: r.get('cpu_total_ms') for r in rows if r['request_id'] == 7},
              'frame_pts_accuracy_verified': False, 'gpu_usage_measured': False}
    (args.output/'results.json').write_text(json.dumps(report, indent=2))
    print(json.dumps({k: report[k] for k in ('scope', 'first', 'warm', 'failures')}))
    return bool(failures)


if __name__ == '__main__':
    raise SystemExit(main())
