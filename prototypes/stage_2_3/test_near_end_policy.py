# GPL-2.0-or-later. Exercise the actual C policy without LibVLC or Windows IO.
import ctypes
from pathlib import Path
import re
import shutil
import subprocess
import tempfile
import unittest


class Capture(ctypes.Structure):
    _fields_ = [('ended', ctypes.c_int), ('captured', ctypes.c_int),
                ('decoder_error', ctypes.c_int)]


class NearEndPolicyTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        compiler = shutil.which('cc')
        if not compiler:
            raise unittest.SkipTest('Host C compiler required for C policy tests')
        source = Path(__file__).with_name('thumbnail_helper.c').read_text()
        policy = re.search(r'static int near_end_retry_start\(.*?\n\}', source, re.S).group()
        policy = policy.replace('static int near_end_retry_start', 'int near_end_retry_start', 1)
        definitions = '\n'.join(re.findall(r'^#define NEAR_END_.*$', source, re.M))
        cls.temp = tempfile.TemporaryDirectory(prefix='vlc-near-end-policy-')
        root = Path(cls.temp.name)
        (root / 'policy.c').write_text('#include <stdint.h>\n' + definitions +
            '\ntypedef struct { int ended, captured, decoder_error; } Capture;\n' + policy)
        subprocess.run([compiler, '-std=c11', '-Wall', '-Wextra', '-Werror', '-shared',
                        '-fPIC', str(root / 'policy.c'), '-o', str(root / 'policy.so')], check=True)
        cls.library = ctypes.CDLL(str(root / 'policy.so'))
        cls.policy = cls.library.near_end_retry_start
        cls.policy.argtypes = [ctypes.POINTER(Capture), ctypes.c_int, ctypes.c_int,
            ctypes.c_int, ctypes.c_uint64, ctypes.c_uint64, ctypes.POINTER(ctypes.c_uint64)]
        cls.policy.restype = ctypes.c_int

    @classmethod
    def tearDownClass(cls):
        cls.temp.cleanup()

    def evaluate(self, target=29950, length=30000, ended=1, captured=0,
                 decoder_error=0, retried=0, timed_out=0, has_video=1):
        state = Capture(ended, captured, decoder_error)
        start = ctypes.c_uint64(2**64 - 1)
        result = self.policy(ctypes.byref(state), retried, timed_out, has_video,
                             target, length, ctypes.byref(start))
        return result, start.value

    def test_supplied_mp4_and_mkv_request(self):
        for length in (30000, 30021):
            self.assertEqual(self.evaluate(length=length), (1, length - 250))

    def test_explicit_tail_boundary(self):
        self.assertEqual(self.evaluate(target=29900), (1, 29750))
        self.assertEqual(self.evaluate(target=29899), (0, 2**64 - 1))

    def test_no_retry_after_capture_or_non_eof(self):
        for changes in ({'captured': 1}, {'ended': 0}):
            self.assertEqual(self.evaluate(**changes), (0, 2**64 - 1))

    def test_no_retry_for_error_timeout_or_second_retry(self):
        for changes in ({'decoder_error': 1}, {'timed_out': 1}, {'retried': 1}):
            self.assertEqual(self.evaluate(**changes), (0, 2**64 - 1))

    def test_no_retry_for_audio_unknown_length_or_past_end(self):
        for changes in ({'has_video': 0}, {'length': 0}, {'target': 30001},
                        {'length': 249, 'target': 249}):
            self.assertEqual(self.evaluate(**changes), (0, 2**64 - 1))

    def test_integer_boundaries(self):
        self.assertEqual(self.evaluate(target=150, length=250), (1, 0))
        self.assertEqual(self.evaluate(target=49, length=100), (0, 2**64 - 1))
        self.assertEqual(self.evaluate(target=2**64 - 1, length=2**64 - 1),
                         (1, 2**64 - 251))


if __name__ == '__main__':
    unittest.main()
