import json,subprocess,sys
from pathlib import Path
checks=['integrity','PackageBindingPreflight','EvidenceContractPreflight','DesignPreflight','all']
package='.aegis/packages/AXIOM-SDK-01-v0.3/preflight.py'
records=[]
for name in checks:
 cmd=[sys.executable,package,'--repo','.','--check',name]
 p=subprocess.run(cmd,capture_output=True,text=True,check=False)
 records.append({'check':name,'argv':cmd,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr})
expected=[0,2,2,2,2]
assert [r['exit_code'] for r in records]==expected,records
path=Path('.aegis/packages/AXIOM-SDK-01-v0.3/implementation-context.md')
before=path.read_bytes()
try:
 path.write_bytes(before+b'\nTAMPER-CONTROL\n')
 cmd=[sys.executable,package,'--repo','.','--check','integrity']
 p=subprocess.run(cmd,capture_output=True,text=True,check=False)
 control={'argv':cmd,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr}
 assert p.returncode==2 and 'package ref mismatch: implementation_context_ref' in p.stdout,control
finally:
 path.write_bytes(before)
cmd=[sys.executable,package,'--repo','.','--check','integrity']
p=subprocess.run(cmd,capture_output=True,text=True,check=False)
assert p.returncode==0,(p.stdout,p.stderr)
output={'checks':records,'tamper_control':control,'restored_integrity':{'argv':cmd,'exit_code':p.returncode,'stdout':p.stdout,'stderr':p.stderr}}
Path('.aegis/results/AXIOM-SDK-01-P31-V03-007/raw/preflights.json').write_text(json.dumps(output,indent=2)+'\n',encoding='utf-8')
for item in records: print(item['check'],'exit',item['exit_code'],item['stdout'].strip())
print('tamper control exit',control['exit_code'],'restored integrity exit',p.returncode)
