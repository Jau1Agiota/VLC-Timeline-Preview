# GPL-2.0-or-later. Protect the experimental evidence gate from false passes.
import unittest
from score_results import score


def row(**changes):
    result = dict(exit_code='0', external_timeout='False', observed_ms='1000',
                  requested_ms='1000', observation_method='visible_pts')
    result.update(changes)
    return result


class AccuracyGateTests(unittest.TestCase):
    def test_empty_run_is_not_a_pass(self):
        self.assertFalse(score([])['accuracy_gate_passed'])

    def test_unmeasured_capture_is_not_a_pass(self):
        self.assertEqual(score([row(observed_ms='')])['unreviewed'], 1)

    def test_player_clock_is_not_image_evidence(self):
        self.assertFalse(score([row(observation_method='get_time')])['accuracy_gate_passed'])

    def test_threshold_is_inclusive(self):
        self.assertTrue(score([row(observed_ms='1250')])['accuracy_gate_passed'])

    def test_inaccurate_frame_cannot_pass(self):
        self.assertEqual(score([row(observed_ms='1251')])['inaccurate'], 1)

    def test_failed_and_timed_out_extractions_cannot_pass(self):
        for candidate in [row(exit_code='124'), row(external_timeout='True')]:
            self.assertEqual(score([candidate])['extraction_failed'], 1)

    def test_nonfinite_measurements_rejected(self):
        with self.assertRaises(ValueError):
            score([row(observed_ms='nan')])


if __name__ == '__main__':
    unittest.main()
