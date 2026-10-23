# GPL-2.0-or-later. Independent contract/math tests, actual images tested in CI.
import importlib.util
from pathlib import Path
import sys
import unittest
import tempfile
import struct
root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('accurate_image_score',root/'score_images.py')
score=importlib.util.module_from_spec(spec);spec.loader.exec_module(score)
from test_worker import bmp


class Accuracy(unittest.TestCase):
    def test_scene_pixel_row_orientation(self):
        with tempfile.TemporaryDirectory() as folder:
            image=Path(folder)/'scene.bmp'
            pixels=bytearray(b'\xff\x00\x00\xff'*(320*180))
            pixels[(130*320+160)*4:((130*320+160)*4)+4]=b'\xff\xff\xff\xff'
            bmp(image,pixels);self.assertEqual(score.scene_id(image),'white')
            data=bytearray(image.read_bytes());struct.pack_into('<i',data,22,180)
            data[54:]=b''.join(pixels[y*1280:(y+1)*1280] for y in range(179,-1,-1))
            image.write_bytes(data);self.assertEqual(score.scene_id(image),'white')
            # LibVLC can expose a coded aspect ratio rather than 16:9.
            small=bytearray(b'\xff\x00\x00\xff'*(313*180))
            small[(130*313+156)*4:((130*313+156)*4)+4]=b'\xff\xff\xff\xff'
            header=bytearray(data[:54]);struct.pack_into('<i',header,18,313)
            struct.pack_into('<i',header,22,-180)
            image.write_bytes(header+small);self.assertEqual(score.scene_id(image),'white')
            image.write_bytes(b'not a BMP')
            with self.assertRaises(ValueError):score.scene_id(image)

    def test_pts_parser(self):
        self.assertEqual(score.parse_pts('00:00:01.001'),1001)
        self.assertEqual(score.parse_pts('01:10:52.123'),4252123)
        for bad in ['00:60:01.001','00:00:01','unavailable']:
            self.assertIsNone(score.parse_pts(bad))



if __name__=='__main__':unittest.main()
