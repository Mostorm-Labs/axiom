import unittest,sys,json,copy,gzip
from pathlib import Path
from fractions import Fraction as F
ROOT=Path(__file__).resolve().parents[2];sys.path.insert(0,str(ROOT/'verification/runners/g5'))
from mixed_reference_image import SourceImageAudit,source_point,f32_axis,f32,snapshot_file
from cpu_image_tie_oracle import indices
class ImageRealizationNegative(unittest.TestCase):
 @classmethod
 def setUpClass(cls):
  cls.request=json.loads((ROOT/'out/G5-mixed-reference-qualification-r1/append-zoom-out-immediate/edge-request.json').read_text());cls.camera=cls.request['camera'];cls.objects=snapshot_file(cls.request['snapshot']);cls.resource=ROOT/'.aegis/results/G5-object-interaction-r2-image-move-diagnostic/image-2_3mb.png';cls.image=next(o for o in cls.objects if'image'in o)
  # Isolated literal material for numeric negatives. Full fixture includes
  # conservativeUp(999,999) bodyAABBs and cannot prove unoccludedimageexpectation.
  cls.audit=SourceImageAudit([cls.image],cls.camera,5724,4040,1431,1010,cls.resource)
 def make_audit(self,objects):return SourceImageAudit(objects,self.camera,5724,4040,1431,1010,self.resource)
 def test_near_nontie_never_gets_adjacent_integer_exception(self):
  for delta in [F(1,2**40),F(1,2**24)]:
   self.assertEqual(indices(F(110)-delta,0,879),[109]);self.assertEqual(indices(F(110)+delta,0,879),[110])
  qx,qy=source_point(self.image,self.camera,5724,4040,1431,1010,4011,3541)
  self.assertNotEqual(qy.denominator,1);self.assertLess(qy,110);self.assertIsNone(self.audit.qualified_alternate_values(4011,3541))
 def test_f32_sequence_does_not_promote_diagnostic_to_expectedset(self):
  values=f32_axis(self.image,self.camera,4,1,3541)
  self.assertEqual(values['device_scale'],5.100000381469727);self.assertEqual(values['device_offset'],3465.000244140625)
  self.assertLess(values['CPU_illustrative_unfused'],110);self.assertLess(values['CPU_illustrative_fma'],110)
  for y in range(3539,3544):self.assertIsNone(self.audit.qualified_alternate_values(4011,y))
 def test_wrong_resource_opacity_and_unsupported_affine_failclosed(self):
  for change in ['resource','opacity','affine','parent']:
   image=copy.deepcopy(self.image)
   if change=='resource':image['image']['resource']='0'*32
   if change=='opacity':image['opacity']=.5
   if change=='affine':image['pose'][1]=.125
   if change=='parent':image['parent']=b'not-a-root'
   audit=self.make_audit([image]);self.assertEqual(audit.images,[]);self.assertIsNone(audit.qualified_alternate_values(4011,3541))
 def test_crop_and_one_devicepixel_pose_change_source_prediction(self):
  base=self.audit.diagnostic_sample(4011,3541);self.assertIsNotNone(base)
  crop=copy.deepcopy(self.image);crop['image']['crop']=[1/880,0,879/880,1]
  moved=copy.deepcopy(self.image);moved['pose'][4]+=1/(4*self.camera['scale'])
  for image in [crop,moved]:
   changed_audit=self.make_audit([image]);changed=changed_audit.diagnostic_sample(4011,3541);self.assertIsNotNone(changed)
   self.assertNotEqual(changed['source_x'],base['source_x'])
   # A small crop can preserve one texel. Scan an independently chosen range
   # rather than assuming every moved coordinate changes the discrete sample.
   differences=0
   for x in range(3900,4250):
    expected=self.audit.diagnostic_sample(x,3551);actual=changed_audit.diagnostic_sample(x,3551)
    differences+=bool(expected and actual and expected['texel']!=actual['texel'])
   self.assertGreater(differences,10)
   self.assertIsNone(changed_audit.qualified_alternate_values(4011,3541))
 def test_wrong_image_colors_still_rejected_by_strict_old_RGBA(self):
  import subprocess
  # Actual comparePixels test is also run in Node; this does not fit choices.
  sample=self.audit.diagnostic_sample(4011,3551);self.assertIsNotNone(sample)
  wrong=tuple(255-v for v in sample['value'][:3])+(255,)
  self.assertGreater(max(abs(a-b)for a,b in zip(wrong,sample['value'])),2)
  self.assertIsNone(self.audit.qualified_alternate_values(4011,3551))
 def test_later_occluder_prohibits_diagnostic_expected_color(self):
  overlay=copy.deepcopy(self.image);overlay['id']='f'*32;overlay['kind']=1;overlay.pop('image');overlay['order']=self.image['order']+b'\x80'
  self.assertIsNone(self.make_audit([self.image,overlay]).diagnostic_sample(4011,3551))
if __name__=='__main__':unittest.main()
