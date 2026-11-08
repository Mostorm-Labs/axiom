"""Run against the real non-installed offline oracle, not a mock decoder."""
import json, os, subprocess, tempfile, unittest
from pathlib import Path

class FixtureOracleTest(unittest.TestCase):
    def test_pixel_oracle_decodes_the_public_snapshot_and_rejects_invalid_bytes(self):
        executable=Path(os.environ.get('UR0_FIXTURE_ORACLE','out/host/runtime/render/axiom_ur0_fixture_oracle'))
        with tempfile.TemporaryDirectory() as temp:
            bad=Path(temp)/'bad.axsnap';bad.write_bytes(b'not a snapshot')
            request=Path(temp)/'request.json';request.write_text('{"width":16,"height":16}')
            result=subprocess.run([str(executable.resolve()),'--pixels',str(bad),str(request),str(Path(temp)/'reference')],capture_output=True,text=True)
            self.assertNotEqual(result.returncode,0)
            self.assertEqual(json.loads(result.stdout)['status'],'INVALID_SNAPSHOT')
    def test_malformed_snapshot_is_rejected(self):
        executable=Path(os.environ.get('UR0_FIXTURE_ORACLE','out/host/runtime/render/axiom_ur0_fixture_oracle'))
        self.assertTrue(executable.is_file(), 'independent fixture decoder unavailable')
        with tempfile.TemporaryDirectory() as temp:
            path=Path(temp)/'bad.axsnap';path.write_bytes(b'not a snapshot')
            run=subprocess.run([str(executable.resolve()),str(path)],capture_output=True,text=True)
            self.assertNotEqual(run.returncode,0)
            self.assertEqual(json.loads(run.stdout)['status'],'INVALID_SNAPSHOT')

if __name__=='__main__':unittest.main()
