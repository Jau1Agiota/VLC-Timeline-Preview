# GPL-2.0-or-later. Host-only evidence framing tests; no libmpv runtime needed.
import io
from pathlib import Path
import struct
import tempfile
import unittest
from test_worker import exact, bmp, stats


class Framing(unittest.TestCase):
    def test_partial_reads(self):
        class Fragmented(io.BytesIO):
            def read(self, n=-1):
                return super().read(min(n, 3))
        self.assertEqual(exact(Fragmented(b'0123456789'), 10), b'0123456789')

    def test_eof_fails(self):
        with self.assertRaises(RuntimeError):
            exact(io.BytesIO(b'partial'), 230400)

    def test_bmp_top_down(self):
        with tempfile.TemporaryDirectory() as folder:
            path = Path(folder)/'candidate.bmp'
            pixels = b'\x20\x80\xf0\xff'*(320*180)
            bmp(path, pixels)
            data = path.read_bytes()
            self.assertEqual(len(data), 54+230400)
            self.assertEqual(struct.unpack_from('<ii', data, 18), (320, -180))
            self.assertEqual(data[54:], pixels)

    def test_percentiles(self):
        self.assertEqual(stats([9, 1, 2, 3]), {'n': 4, 'median_ms': 2.5, 'p95_ms': 9})


if __name__ == '__main__':
    unittest.main()
