# GPL-2.0-or-later. Independent contract/math tests, actual images tested in CI.
import importlib.util
from pathlib import Path
import sys
import unittest
root=Path(__file__).resolve().parent
spec=importlib.util.spec_from_file_location('accurate_image_score',root/'score_images.py')
score=importlib.util.module_from_spec(spec);spec.loader.exec_module(score)


class Accuracy(unittest.TestCase):
    def test_pts_parser(self):
        self.assertEqual(score.parse_pts('00:00:01.001'),1001)
        self.assertEqual(score.parse_pts('01:10:52.123'),4252123)
        for bad in ['00:60:01.001','00:00:01','unavailable']:
            self.assertIsNone(score.parse_pts(bad))



if __name__=='__main__':unittest.main()
