import importlib.util,unittest
from pathlib import Path
p=Path(__file__).resolve().parents[1]/'runners/g5/idle_input_feedback_analyze.py'
s=importlib.util.spec_from_file_location('idle_analyze',p);m=importlib.util.module_from_spec(s);s.loader.exec_module(m)
class DiagnosticFields(unittest.TestCase):
 def test_exact_ids_and_wire_types(self):
  data=b'\x08\x96\x01\x12\x03abc\x19'+b'12345678'
  self.assertEqual(list(m.fields(data)),[(1,150),(2,b'abc'),(3,b'12345678')])
 def test_truncation_cannot_be_missing_evidence(self):
  for value in [b'\x12\x05a',b'\x80',b'\x00']:
   with self.assertRaises(ValueError):list(m.fields(value))
 def test_quantiles_report_worst(self):
  self.assertEqual(m.quantile([1,2,9]),{'count':3,'median_ms':2,'p95_ms':9,'max_ms':9})
if __name__=='__main__':unittest.main()
