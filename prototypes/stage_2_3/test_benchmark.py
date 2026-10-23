# GPL-2.0-or-later. Timing aggregation/protocol tests; no decode claims.
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
import benchmark


class BenchmarkTests(unittest.TestCase):
    def test_median_nearest_rank_p95(self):
        self.assertEqual(benchmark.stats(range(1, 21)),
                         dict(n=20, median_ms=10.5, p95_ms=19))

    def test_small_sample_and_empty(self):
        self.assertEqual(benchmark.stats([3]), dict(n=1, median_ms=3, p95_ms=3))
        with self.assertRaises(ValueError):
            benchmark.stats([])

    def run_fake(self, root, extra=''):
        helper = root / 'fake.py'
        helper.write_text('''#!/usr/bin/env python3
import json,sys,struct
from pathlib import Path
image=bytearray(58);image[:2]=b'BM';struct.pack_into('<ii',image,18,1,1)
Path(sys.argv[3]).write_bytes(image)
r=dict(status='candidate_captured',frame_timestamp_verified=False,requested_ms=int(sys.argv[2]),width=1,height=1)
for k in ('capture_latency_ms','total_latency_ms','libvlc_init_ms','media_player_create_ms',
          'play_call_ms','decode_wait_ms','stop_release_ms','bmp_output_ms'):r[k]=1
''' + extra + '\nprint(json.dumps(r))\n')
        helper.chmod(0o700)
        output = root / 'output.json'
        with contextlib.redirect_stdout(io.StringIO()):
            benchmark.benchmark(helper, root, output, 3, profiles=('baseline','low-cache'))
        return json.loads(output.read_text())

    def test_real_process_rows_and_candidate_comparison(self):
        with tempfile.TemporaryDirectory() as tmp:
            data = self.run_fake(Path(tmp))
            self.assertEqual(len(data['rows']),12)
            self.assertEqual(data['summary']['low-cache']['total_latency_ms']['n'],6)
            self.assertEqual(data['candidate_comparison']['low-cache']['byte_identical_to_baseline'],6)
            self.assertFalse(data['frame_timestamp_verified'])

    def test_reject_timestamp_verified_claim(self):
        with tempfile.TemporaryDirectory() as tmp:
            with self.assertRaises(ValueError):
                self.run_fake(Path(tmp), "r['frame_timestamp_verified']=True")

    def test_persistent_response_sequence_and_eof_cleanup(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);helper=root/'worker.py'
            helper.write_text("#!/usr/bin/env python3\nimport json,sys,struct\nfrom pathlib import Path\nfor line in sys.stdin:\n    rid,target,timeout=map(int,line.split())\n    image=bytearray(58);image[:2]=b'BM';struct.pack_into('<ii',image,18,1,1)\n    Path(sys.argv[3]).write_bytes(image)\n    r=dict(request_id=rid,requested_ms=target,frame_timestamp_verified=False,exit_code=0,\n           capture_latency_ms=1,total_latency_ms=1,libvlc_init_ms=1 if rid==1 else 0)\n    print(json.dumps(r),flush=True)\n")
            helper.chmod(0o700)
            rows=benchmark.persistent_samples(helper,root/'media.mp4',root/'frame.bmp',3,'baseline',False)
            self.assertEqual([r['mode'] for r in rows],['worker_cold','worker_warm','worker_warm'])
            self.assertEqual([r['helper']['request_id'] for r in rows],[1,2,3])
            self.assertFalse((root/'frame.bmp').exists())

    def test_refuse_existing_output(self):
        with tempfile.TemporaryDirectory() as tmp:
            output=Path(tmp)/'old.json';output.write_text('preserve')
            with self.assertRaises(ValueError):
                benchmark.benchmark('missing',tmp,output,3)
            self.assertEqual(output.read_text(),'preserve')


if __name__ == '__main__':
    unittest.main()
