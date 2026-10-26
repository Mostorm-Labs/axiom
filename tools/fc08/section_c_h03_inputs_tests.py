"""Tool-only integrity tests, separate from Axiom SUT regression."""
import json,tempfile,unittest
from pathlib import Path
from tools.fc08 import section_c_h03_inputs as m
class H03InputIntegrity(unittest.TestCase):
 def overlay(self):return json.loads(m.OVERLAY.read_text(encoding='utf-8'))
 def test_accepted_expected_bindings(self):m.validate_refs(self.overlay())
 def test_changed_expected_hash_fails_closed(self):
  value=self.overlay();value['variants'][0]['expected']['sha256']='0'*64
  with self.assertRaises(ValueError):m.validate_refs(value)
 def test_all_ordinal_keys_legal_and_order_preserved(self):
  keys=[bytes.fromhex(m.root_key(i)) for i in range(1,65537)]
  self.assertTrue(all(len(k)==5 and k[-1]==1 for k in keys));self.assertEqual(keys,sorted(set(keys)))
  self.assertEqual(m.root_key(256),'0000010001');self.assertEqual(m.root_key(65536),'0001000001')
 def test_out_of_scope_ordinal_rejected(self):
  for i in [0,65537,-1]:
   with self.assertRaises(ValueError):m.root_key(i)
 def test_existing_input_not_overwritten(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'fixture.pb';m.write_once(p,b'first');m.write_once(p,b'first')
   with self.assertRaises(ValueError):m.write_once(p,b'changed')
   self.assertEqual(p.read_bytes(),b'first')
 def test_wire_hash_mismatch_rejected(self):
  with self.assertRaises(ValueError):m.verify_wire(self.overlay()['variants'][0],'F',b'not the accepted wire')
if __name__=='__main__':unittest.main(verbosity=2)
