# GPL-2.0-or-later. Actual parsers/stats and package preservation/boundary checks.
import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest
import zipfile
import benchmark
import package
from score_burned_pts import parse_pts

ROOT=Path(__file__).resolve().parents[2]


class MetricsTests(unittest.TestCase):
    def test_nearest_rank(self):
        self.assertEqual(benchmark.stats(list(range(1,21))),dict(n=20,median_ms=10.5,p95_ms=19))

    def test_pts_from_actual_text_only(self):
        self.assertEqual(parse_pts('00:00:29.900\n'),29900)
        self.assertEqual(parse_pts('PTS 01:02:03.004'),3723004)
        for text in ('00:00:99.001','00:99:01.001','000029900','requested=29900'):
            self.assertIsNone(parse_pts(text))

    def test_lock_identity(self):
        lock=json.loads((ROOT/'prototypes/stage_2_4_6/dependencies.json').read_text())
        self.assertFalse(lock['runtime_redistributed'])
        self.assertEqual(len(lock['sha256']),64)
        self.assertTrue(lock['url'].startswith('https://github.com/shinchiro/'))


class PackageTests(unittest.TestCase):
    def test_no_runtime_redistribution_and_complete_own_source(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);binary=root/'own.exe';binary.write_bytes(b'test executable')
            delivery=root/'delivery'
            with contextlib.redirect_stdout(io.StringIO()):package.package(binary,delivery,ROOT)
            archive=next(delivery.glob('*.zip'))
            with zipfile.ZipFile(archive) as z:
                self.assertIsNone(z.testzip())
                names=z.namelist()
                for name in ('preview.c','request_policy.h','build.sh','COPYING','dependencies.json','MANIFEST.json'):
                    self.assertIn('libmpv-stage246A-win64/'+name,names)
                self.assertFalse(any(n.endswith('.dll') or n.endswith('.dll.a') for n in names))
                m=json.loads(z.read('libmpv-stage246A-win64/MANIFEST.json'))
                self.assertFalse(m['runtime_included'])
                self.assertEqual(m['files_sha256']['mpv-preview.exe'],package.sha(binary))
            before=package.sha(archive)
            with self.assertRaises(ValueError):package.package(binary,delivery,ROOT)
            self.assertEqual(package.sha(archive),before)


if __name__=='__main__':unittest.main()
