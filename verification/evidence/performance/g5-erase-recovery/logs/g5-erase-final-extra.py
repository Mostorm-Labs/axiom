from pathlib import Path
import json,sys,time,hashlib
R=Path.cwd();sys.path.insert(0,str(R));sys.path.insert(0,str(R/'verification/runners/g5'));from bounded_worker import run
log=R/'out/g5-erase-execute.log';start=time.monotonic()
while True:
 rows=[]
 for line in log.read_text().splitlines():
  try:rows.append(json.loads(line))
  except ValueError:pass
 if rows and rows[-1].get('stage')=='complete':break
 if 'Traceback'in log.read_text()or time.monotonic()-start>240:raise RuntimeError('primary observation incomplete')
 time.sleep(1)
from verification.tools.g5_preflight import source_identity
before=source_identity(R);O=R/'out/g5-erase-final';w=run(['node','out/g5-erase-reject-pixels.mjs',str(R/'out/g5-erase-off/js'),str(O/'rejected-pixels.json')],O/'rejected-pixels-worker',180,3072*1024);after=source_identity(R)
(O/'rejected-pixels-inputs.json').write_text(json.dumps({'sourceBefore':before,'sourceAfter':after,'scriptSha256':hashlib.sha256((R/'out/g5-erase-reject-pixels.mjs').read_bytes()).hexdigest()},indent=2)+'\n')
print(json.dumps(w),flush=True)
if w['reason']!='completed'or not w['cleanup_complete']:raise RuntimeError('rejected compositor recovery')
