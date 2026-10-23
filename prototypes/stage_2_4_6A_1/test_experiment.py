# GPL-2.0-or-later. Actual parsers/stats and package preservation/boundary checks.
import contextlib
import io
import json
import shutil
import struct
from pathlib import Path
import tempfile
import unittest
import zipfile
import benchmark
import package
from score_burned_pts import parse_pts, score

ROOT=Path(__file__).resolve().parents[2]


class MetricsTests(unittest.TestCase):
    def test_nearest_rank(self):
        self.assertEqual(benchmark.stats(list(range(1,21))),dict(n=20,median_ms=10.5,p95_ms=19))

    def test_pts_from_actual_text_only(self):
        self.assertEqual(parse_pts('00:00:29.900\n'),29900)
        self.assertEqual(parse_pts('PTS 01:02:03.004'),3723004)
        for text in ('00:00:99.001','00:99:01.001','000029900','requested=29900'):
            self.assertIsNone(parse_pts(text))

    def test_analysis_preserves_failure_evidence(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);evidence=root/'evidence';evidence.mkdir()
            (evidence/'comparison.json').write_text('{}')
            output=root/'accuracy'
            with contextlib.redirect_stdout(io.StringIO()),self.assertRaises(ValueError):
                score(evidence,root/'fixtures',output)
            data=json.loads((output/'accuracy.json').read_text())
            self.assertEqual(len(data['failures']),1)
            self.assertEqual(data['failures'][0]['report'],'comparison.json')

    @unittest.skipUnless(shutil.which('ffmpeg') and shutil.which('tesseract'), 'offline OCR tools unavailable')
    def test_narrow_libvlc_bmp_uses_actual_width(self):
        # The unchanged LibVLC helper bounds width <=320, not width ==320.
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp);evidence=root/'evidence';evidence.mkdir();fixtures=root/'fixtures';fixtures.mkdir()
            (fixtures/'ordinary.mp4.frames.json').write_text(json.dumps(dict(frames=[dict(best_effort_timestamp_time='0.0')])))
            (evidence/'comparison.json').write_text(json.dumps(dict(media='C:/ordinary.mp4',seek='libvlc-accurate',
                records=[dict(id=1,bmp_saved=True,requested_ms=0)])))
            width,height=313,180;stride=(width*3+3)&~3;size=stride*height
            header=struct.pack('<2sIHHI',b'BM',54+size,0,0,54)+struct.pack('<IiiHHIIiiII',40,width,height,1,24,0,size,0,0,0,0)
            (evidence/'frame-000001.bmp').write_bytes(header+(b'\xff'*(width*3)+b'\x00'*(stride-width*3))*height)
            with contextlib.redirect_stdout(io.StringIO()):score(evidence,fixtures,root/'analysis')
            result=json.loads((root/'analysis/accuracy.json').read_text())
            self.assertEqual(result['failures'],[])
            self.assertEqual(len(result['rows']),1)
            self.assertFalse(result['rows'][0]['frame_timestamp_verified'])
            self.assertFalse(result['rows'][0]['plausible_source_frame'])  # Blank is not a timestamp.

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
                    self.assertIn('libmpv-stage246A1-win64/'+name,names)
                self.assertFalse(any(n.endswith('.dll') or n.endswith('.dll.a') for n in names))
                m=json.loads(z.read('libmpv-stage246A1-win64/MANIFEST.json'))
                self.assertFalse(m['runtime_included'])
                self.assertEqual(m['files_sha256']['mpv-preview.exe'],package.sha(binary))
            before=package.sha(archive)
            with self.assertRaises(ValueError):package.package(binary,delivery,ROOT)
            self.assertEqual(package.sha(archive),before)


if __name__=='__main__':unittest.main()
