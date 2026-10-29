import hashlib
import json
import subprocess
import sys
import tempfile
import unittest
from pathlib import Path

HERE=Path(__file__).resolve().parent
RUNNER=HERE/'run_observation_driver.py'
EXPECTED=HERE/'expected-v1.json'

class DriverBoundaryTests(unittest.TestCase):
    def test_missing_driver_records_blocker(self):
        with tempfile.TemporaryDirectory() as name:
            output=Path(name)/'result.json'
            command=[sys.executable,str(RUNNER),'--driver',str(Path(name)/'absent.exe'),
                     '--expected',str(EXPECTED),'--expected-sha256','0'*64,
                     '--output',str(output)]
            result=subprocess.run(command,capture_output=True,text=True,check=False)
            self.assertEqual(2,result.returncode,(result.stdout,result.stderr))
            self.assertEqual('BLOCKED_INPUT',json.loads(output.read_text())['status'])

    def test_invalid_expected_hash_blocks_before_run(self):
        with tempfile.TemporaryDirectory() as name:
            output=Path(name)/'result.json'
            command=[sys.executable,str(RUNNER),'--driver',sys.executable,
                     '--expected',str(EXPECTED),'--expected-sha256','0'*64,
                     '--output',str(output)]
            result=subprocess.run(command,capture_output=True,text=True,check=False)
            self.assertEqual(2,result.returncode,(result.stdout,result.stderr))
            data=json.loads(output.read_text())
            self.assertEqual('BLOCKED_INPUT',data['status'])
            self.assertIn('expected hash',data['reason'])

    def test_success_exit_without_observations_is_not_pass(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name)
            script=root/'driver.py'
            script.write_text('import sys\nsys.exit(0)\n',encoding='utf-8')
            output=root/'result.json'
            digest=lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
            command=[sys.executable,str(RUNNER),'--driver',sys.executable,
                     '--driver-sha256',digest(sys.executable),'--expected',str(EXPECTED),
                     '--expected-sha256',digest(EXPECTED),'--output',str(output),'--',str(script)]
            result=subprocess.run(command,capture_output=True,text=True,check=False)
            self.assertEqual(1,result.returncode,(result.stdout,result.stderr))
            self.assertEqual('FAILED_EVIDENCE',json.loads(output.read_text())['status'])

    def test_driver_wrong_observations_are_rejected(self):
        with tempfile.TemporaryDirectory() as name:
            root=Path(name)
            script=root/'driver.py'
            script.write_text("import pathlib,sys\npathlib.Path(sys.argv[-1]).write_text('{}')\n",encoding='utf-8')
            output=root/'result.json'
            digest=lambda path: hashlib.sha256(Path(path).read_bytes()).hexdigest()
            command=[sys.executable,str(RUNNER),'--driver',sys.executable,
                     '--driver-sha256',digest(sys.executable),'--expected',str(EXPECTED),
                     '--expected-sha256',digest(EXPECTED),'--output',str(output),'--',str(script)]
            result=subprocess.run(command,capture_output=True,text=True,check=False)
            self.assertEqual(1,result.returncode,(result.stdout,result.stderr))
            self.assertEqual('ORACLE_MISMATCH',json.loads(output.read_text())['status'])

if __name__=='__main__': unittest.main()
