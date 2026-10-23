# GPL-2.0-or-later. Validate missing/stale requests are not silently counted as successes.
import unittest
from replay import summarize

class SummaryTests(unittest.TestCase):
    def test_obsolete_intermediates_optional_latest_required(self):
        planned=[dict(id=1,workload='rapid_intermediate'),dict(id=2,workload='rapid_latest')]
        row=dict(id=1,display_handoff_ms=20)
        data=summarize([row],planned)
        self.assertEqual(data['missing_required_ids'],[2])
        self.assertIsNone(data['rapid_latest'])
        self.assertFalse(data['frame_timestamp_verified'])
        data=summarize([dict(id=2,display_handoff_ms=30)],planned)
        self.assertEqual(data['missing_required_ids'],[])
        self.assertEqual(data['missing_ids'],[1])
        self.assertEqual(data['rapid_latest']['display_handoff_ms']['n'],1)
        self.assertIsNone(data['rapid_latest']['readback_ms']) # Old schema is unknown, not zero.

    def test_first_warm_and_repeat_not_mixed(self):
        planned=[dict(id=1,workload='first_hover'),dict(id=2,workload='warm_random'),dict(id=3,workload='repeat_same')]
        data=summarize([dict(id=i,display_handoff_ms=i*10) for i in (1,2,3)],planned)
        self.assertEqual(data['first_hover']['display_handoff_ms']['median_ms'],10)
        self.assertEqual(data['warm_random']['display_handoff_ms']['median_ms'],20)
        self.assertEqual(data['repeat_same']['display_handoff_ms']['median_ms'],30)

    def test_independent_stage_intervals(self):
        row=dict(id=1,queue_ms=3,restart_ms=20,render_ms=18,display_handoff_ms=21)
        data=summarize([row],[dict(id=1,workload='first_hover')])
        self.assertEqual(data['first_hover']['seek_pipeline_ms']['median_ms'],17)
        self.assertEqual(data['first_hover']['render_to_handoff_ms']['median_ms'],3)

    def test_honest_intermediate_not_counted_as_latest_target_success(self):
        row=dict(id=1,latest_at_handoff=2,display_handoff_ms=20)
        data=summarize([row],[dict(id=1,workload='warm_random')])
        self.assertEqual(data['intermediate_presentations'],1)
        self.assertEqual(data['missing_required_ids'],[1])
