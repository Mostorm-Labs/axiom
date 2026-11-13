import importlib.util,unittest
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'runners/g5/live_erase_feedback_analyze.py';s=importlib.util.spec_from_file_location('le_analyze',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class LiveEraseAnalysis(unittest.TestCase):
 def test_mask_segments_are_counted_from_actual_geometry(self):
  # mask_id omitted in this pure field fixture; geometry.swept_circle has two segments.
  self.assertEqual(m.mask_segments(b'\x12\x06\x0a\x04\x0a\x00\x0a\x00'),2)
 def test_non_swept_geometry_is_not_assumed_countable(self):
  with self.assertRaises(ValueError):m.mask_segments(b'\x12\x02\x12\x00')
 def test_pose_and_masks_excluded_without_losing_body(self):
  self.assertEqual(m.without_pose_masks(b'\x08\x01\x2a\x00\x42\x00\x12\x01x'),[(1,1),(2,b'x')])
if __name__=='__main__':unittest.main()
