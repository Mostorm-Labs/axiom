import json,subprocess,sys,tempfile,unittest,os
from pathlib import Path
BINARY=Path(os.environ.get('AXIOM_G5_FIXTURE_BINARY','out/host/axiom_g5_scale_fixture')).resolve()
if __name__=='__main__' and len(sys.argv)>1 and not sys.argv[1].startswith('-'):BINARY=Path(sys.argv.pop(1)).resolve()
@unittest.skipUnless(BINARY.is_file(),'private native fixture tool is not built')
class Fixture(unittest.TestCase):
 def command(self,*args):return json.loads(subprocess.check_output([str(BINARY),*map(str,args)],text=True).splitlines()[-1])
 def test_real_codec_roundtrip_and_not_input_sample_count(self):
  with tempfile.TemporaryDirectory() as d:
   for axis,count,objects,points in [('objects',16,16,0),('points',256,1,256)]:
    path=Path(d)/(axis+'.axsnap');r=self.command('fixture',axis,count,'sparse',path)
    self.assertEqual(r['status'],'completed');self.assertEqual(r['objects'],objects);self.assertEqual(r['points'],points)
    inspected=self.command('inspect',path);self.assertEqual(inspected['objects'],objects);self.assertEqual(inspected['points'],points)
    self.assertEqual(r['checkpointDigest'],inspected['checkpointDigest']);self.assertTrue(path.stat().st_size>0)
 def test_component_query_matches_literal_sparse_and_dense_visibility(self):
  for mode,visible in [('sparse',25),('dense',64)]:
   r=self.command('component','objects',64,mode)
   self.assertEqual(r['visible_objects'],visible);self.assertTrue(r['checks']['linear_parity']);self.assertEqual(r['objects'],64)
  r=self.command('component','points',2048,'sparse')
  self.assertEqual(r['points'],2048);self.assertEqual(r['visible_points'],1600);self.assertTrue(r['checks']['linear_parity'])
 def test_component_rejects_points_outside_the_accepted_compact_contract(self):
  r=self.command('component','points',262145,'sparse')
  self.assertEqual(r['status'],'rejected');self.assertEqual(r['scope'],'point-projection')
  self.assertEqual(r['reason'],'invalid_compact_stroke');self.assertEqual(r['points'],262145)
 def test_document_total_can_exceed_single_stroke_limit(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'multiple.axsnap'
   r=self.command('fixture','points',262145,'sparse',p,100000)
   self.assertEqual(r['status'],'completed');self.assertEqual(r['objects'],3)
   self.assertEqual(r['points'],262145);self.assertEqual(r['visible_points'],4800)
   decoded=self.command('inspect',p);self.assertEqual(decoded['points'],262145)
   self.assertEqual(decoded['objects'],3)
 def test_corrupt_snapshot_is_not_reported_as_a_completed_fixture(self):
  with tempfile.TemporaryDirectory() as d:
   p=Path(d)/'bad.axsnap';p.write_bytes(b'bad')
   r=self.command('inspect',p);self.assertEqual(r['status'],'rejected')
if __name__=='__main__':
 if not BINARY.is_file():raise FileNotFoundError(BINARY)
 unittest.main()
