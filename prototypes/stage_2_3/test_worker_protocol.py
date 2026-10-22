# GPL-2.0-or-later. Exercise actual C parser, independent of LibVLC/Windows IO.
import ctypes
from pathlib import Path
import re
import subprocess
import tempfile
import unittest


class WorkerProtocolTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.temp=tempfile.TemporaryDirectory(prefix='vlc-worker-parser-')
        root=Path(cls.temp.name)
        source=Path(__file__).with_name('thumbnail_helper.c').read_text()
        parser=re.search(r'static int request_line\(.*?\n\}',source,re.S).group()
        (root/'parser.c').write_text(parser.replace('static int request_line','int request_line',1))
        subprocess.run(['cc','-std=c11','-Wall','-Wextra','-Werror','-shared','-fPIC',
                        str(root/'parser.c'),'-o',str(root/'parser.so')],check=True)
        cls.lib=ctypes.CDLL(str(root/'parser.so'))
        cls.parse=cls.lib.request_line
        cls.parse.argtypes=[ctypes.c_char_p,ctypes.POINTER(ctypes.c_uint)]
        cls.parse.restype=ctypes.c_int

    @classmethod
    def tearDownClass(cls):cls.temp.cleanup()

    def test_valid_lines(self):
        for text in (b'1 29950 5000\n',b'64 86400000 100\r\n'):
            fields=(ctypes.c_uint*3)()
            self.assertEqual(self.parse(text,fields),1)

    def test_reject_overflow_ambiguous_or_partial_lines(self):
        for text in (b'1 1 1',b'-1 1 1\n',b'1 4294967296 1\n',b'1 1 1 4\n',
                     b'1 1 1\nextra',b'1\t1 1\n',b'1  1 1\n',b'1 x 1\n',b'\n',b'9'*128):
            fields=(ctypes.c_uint*3)()
            self.assertEqual(self.parse(text,fields),0,text)


if __name__=='__main__':unittest.main()
