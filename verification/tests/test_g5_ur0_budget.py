"""The actual retirement header must admit identical work with/without diagnostics."""
import json,subprocess,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
class RetirementQuota(unittest.TestCase):
    def test_actual_normal_and_qualification_quota_at_boundary(self):
        with tempfile.TemporaryDirectory(prefix='ur0-quota-') as tmp:
            binaries={}
            for name,enabled in [('normal',False),('qualified',True)]:
                target=Path(tmp)/name
                command=['c++','-std=c++20','-pthread','-I'+str(ROOT/'.deps/nlohmann-json/single_include')]
                command += ['-I'+str(p) for p in sorted((ROOT/'runtime').glob('*/include'))]
                if enabled:command += ['-DAXIOM_G5_UR0_DIAGNOSTICS=1',str(ROOT/'runtime/foundation/src/g5_observation.cpp')]
                command += [str(ROOT/'runtime/render/tests/g5_observation_identity_test.cpp'),'-o',str(target)]
                subprocess.run(command,check=True,capture_output=True)
                binaries[name]=target
            def run(name,cap,mode):
                return json.loads(subprocess.check_output([str(binaries[name]),'--quota',str(cap),mode],text=True))
            baseline=run('normal',1024,'OFF');charge=baseline[0]['charged']
            self.assertGreater(charge,0)
            boundary=run('normal',charge,'OFF')
            self.assertTrue(all(x['admitted'] and x['delayed']==charge and x['closed']==0 for x in boundary))
            self.assertEqual(boundary[0]['completed'],0)
            self.assertEqual(boundary[1]['completed'],charge)
            self.assertEqual(boundary[2]['completed'],0)
            self.assertEqual(run('qualified',charge,'OFF'),boundary,'qualification OFF changed product admission/retirement')
            self.assertEqual(run('qualified',charge,'ON'),boundary,'qualification ON changed product admission/retirement')
            self.assertTrue(all(not x['admitted'] for x in run('qualified',charge-1,'ON')))
