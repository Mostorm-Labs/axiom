import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest

ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'verification/sdk_cpp_first/verify_public_header.py'
class PublicHeaderRunnerTests(unittest.TestCase):
    def test_missing_compiler_is_explicit_blocker(self):
        with tempfile.TemporaryDirectory() as directory:
            output=Path(directory)/'result.json'
            env=os.environ.copy();env['PATH']=''
            p=subprocess.run([sys.executable,str(SCRIPT),'--repo',str(ROOT),'--output',str(output)],env=env,capture_output=True,text=True,check=False)
            self.assertEqual(2,p.returncode,(p.stdout,p.stderr))
            self.assertEqual('BLOCKED_COMPILER',json.loads(output.read_text(encoding='utf-8'))['status'])
if __name__=='__main__':unittest.main()
