import importlib.util
import json
from pathlib import Path
import struct
import tempfile
import unittest
from unittest import mock

ROOT=Path(__file__).resolve().parents[2]
def module(name,path):
    spec=importlib.util.spec_from_file_location(name,path)
    obj=importlib.util.module_from_spec(spec);spec.loader.exec_module(obj);return obj
build=module('runtime_build',ROOT/'ci/runtime/build.py')
matrix=module('runtime_matrix',ROOT/'test/runtime/matrix.py')

class Tools(unittest.TestCase):
    def test_wrong_cached_input_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            root=Path(d);cache=root/'cache';cache.mkdir();(cache/'input').write_bytes(b'wrong')
            with self.assertRaisesRegex(ValueError,'checksum'):
                build.fetch(dict(filename='input',sha256='0'*64),root/'output',cache)
    def test_existing_build_root_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with mock.patch('sys.argv',['build.py',d]):
                with self.assertRaises(SystemExit):build.main()
            self.assertEqual(list(Path(d).iterdir()),[])
    def test_non_https_rejected(self):
        with tempfile.TemporaryDirectory() as d:
            with self.assertRaises(ValueError):build.fetch(dict(filename='input',url='http://example.invalid'),Path(d)/'output',None)
    def test_frame_association_not_claimed(self):
        h=dict(protocol=1,bytes=230400,requested_ms=1000,request_id=1,frame_timestamp_verified=False)
        raw=json.dumps(h).encode()+b'\n'+bytes(230400)
        rows=matrix.parse(raw,[1000]);self.assertFalse(rows[0][0]['image_pts_reviewed'])
        for bad in [raw[:-1],raw+b'x']:
            with self.assertRaises(ValueError):matrix.parse(bad,[1000])
        with self.assertRaises(ValueError):matrix.parse(raw,[2000])
    def test_lock_pins_downloads(self):
        lock=json.loads((ROOT/'ci/runtime/inputs.lock.json').read_text())
        for item in lock['inputs'].values():
            self.assertTrue(item['url'].startswith('https://'))
            self.assertRegex(item['sha256'],r'^[a-f0-9]{64}$')
        self.assertEqual(lock['patches'],[])
    def test_export_table_with_current_objdump_hints(self):
        verify=module('runtime_verify',ROOT/'ci/runtime/verify.py')
        text='[Ordinal/Name Pointer] Table -- Ordinal Base 1\n[ 0] +base[ 1] 0000 FT_Init\n[ 1] +base[ 2] 0001 mpv_create\n[ 2] +base[ 3] 0002 mpv_client_api_version\n\n'
        self.assertEqual(verify.exports(text),['mpv_client_api_version','mpv_create'])
    def test_statistics(self):
        self.assertEqual(matrix.stats([20,10,30,40]),dict(n=4,median_ms=25,p95_ms=40))

if __name__=='__main__':unittest.main()
