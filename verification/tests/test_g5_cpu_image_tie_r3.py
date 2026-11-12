import unittest,importlib.util,copy,struct
from pathlib import Path
from fractions import Fraction as F
spec=importlib.util.spec_from_file_location('image',Path(__file__).resolve().parents[1]/'runners/g5/cpu_image_tie_oracle.py');m=importlib.util.module_from_spec(spec);spec.loader.exec_module(m)
ROOT=Path(__file__).resolve().parents[2]
RESOURCE=ROOT/'.aegis/results/G5-object-interaction-r2-image-move-diagnostic/image-2_3mb.png'
SNAPSHOT=ROOT/'out/G5-CPUW3-complete-interaction-r2/formal-reference/G5CWC2-DPR1-mixed-initial.axsnap'
class TieNegative(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  recording=ROOT/'.aegis/results/G5-CPUW3-complete-interaction-r2/local-web/G5CWC2-DPR1/mixed-focused-dpr1/mixed-initial.rgba'
  if not SNAPSHOT.is_file()or not recording.is_file():
   raise unittest.SkipTest('Historical DPR1 browser recording pruned by the authorized 2026-11-13 cleanup; regenerate it for evidence reverification.')
  cls.objects=m.snapshot(SNAPSHOT);cls.ori=m.TieOracle(cls.objects,{'scale':1,'x':0,'y':0},1431,1010,1431,1010,RESOURCE);cls.actual=(ROOT/'.aegis/results/G5-CPUW3-complete-interaction-r2/local-web/G5CWC2-DPR1/mixed-focused-dpr1/mixed-initial.rgba').read_bytes();cls.image=next(o for o in cls.objects if'image'in o)
 def disagreement(self,changed,resource=RESOURCE):
  oracle=m.TieOracle(changed,{'scale':1,'x':0,'y':0},1431,1010,1431,1010,resource);bad=0;tested=0
  # Test exact expected values throughout original visible opaque image,
  # independent of which candidate/CPU pixels differ.
  for y in range(567,684):
   for x in range(1037,1154):
    v=oracle.allowed(x,y,only_ties=False)
    if not v:continue
    a=self.actual[(y*1431+x)*4:(y*1431+x+1)*4];tested+=1
    if not any(max(abs(a[k]-b[k])for k in range(4))<=2 for b in v['values']):bad+=1
  return tested,bad
 def test_source_exact_ties_only_and_near_nontie_remains_single(self):
  self.assertEqual(m.indices(F(682),0,879),[681,682]);self.assertEqual(m.indices(F(682)+F(1,2**40),0,879),[682]);self.assertEqual(m.indices(F(682)-F(1,2**40),0,879),[681]);self.assertLessEqual(len(m.indices(F(1),0,879))*len(m.indices(F(2),0,879)),4)
 def test_pose_1devicepixel_must_fail(self):
  objects=copy.deepcopy(self.objects);next(o for o in objects if'image'in o)['pose'][4]+=1;tested,bad=self.disagreement(objects);self.assertGreater(tested,10000);self.assertGreater(bad,10000)
 def test_crop_one_source_texel_must_fail(self):
  objects=copy.deepcopy(self.objects);image=next(o for o in objects if'image'in o);image['image']['crop']=[1/880,0,879/880,1];tested,bad=self.disagreement(objects);self.assertGreater(bad,1000)
 def test_resource_identity_opacity_and_occlusion_fail_closed(self):
  for mutation in ['resource','opacity','occluder']:
   objects=copy.deepcopy(self.objects);image=next(o for o in objects if'image'in o)
   if mutation=='resource':image['image']['resource']='0'*32
   if mutation=='opacity':image['opacity']=.5
   if mutation=='occluder':
    covering=copy.deepcopy(image);covering['id']='f'*32;covering['kind']=1;covering.pop('image');covering['order']=image['order']+b'\x80';objects.append(covering)
   oracle=m.TieOracle(objects,{'scale':1,'x':0,'y':0},1431,1010,1431,1010,RESOURCE);self.assertIsNone(oracle.allowed(1128,567))
 def test_wrong_decoded_resource_color_must_fail(self):
  oracle=m.TieOracle(self.objects,{'scale':1,'x':0,'y':0},1431,1010,1431,1010,RESOURCE);oracle.rgb=bytes(v^255 for v in oracle.rgb);bad=0
  for y in range(567,684):
   for x in range(1037,1154):
    v=oracle.allowed(x,y,False)
    if not v:continue
    a=self.actual[(y*1431+x)*4:(y*1431+x+1)*4]
    bad+=not any(max(abs(a[k]-c[k])for k in range(4))<=2 for c in v['values'])
  self.assertGreater(bad,10000)
 def test_wrong_tie_color_not_adjacent_value(self):
  choices=self.ori.allowed(1128,567);self.assertTrue(choices['tie']);wrong=next((r,g,b,255)for r in range(0,256,50)for g in range(0,256,50)for b in range(0,256,50)if all(max(abs(x-y)for x,y in zip((r,g,b,255),c))>2 for c in choices['values']));self.assertTrue(all(max(abs(x-y)for x,y in zip(wrong,c))>2 for c in choices['values']))
if __name__=='__main__':unittest.main()
