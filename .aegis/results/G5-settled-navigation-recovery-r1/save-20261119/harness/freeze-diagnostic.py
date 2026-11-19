from pathlib import Path
import json,hashlib,sys
W=Path(__file__).resolve().parents[2];O=W/'out/G5-settled-navigation-recovery-r1';E=W/'.aegis/results/G5-settled-navigation-recovery-r1';name=sys.argv[1];files={}
def add(p):p=Path(p);files[str(p)]={'bytes':p.stat().st_size,'sha256':hashlib.sha256(p.read_bytes()).hexdigest()}
for p in(W/'.aegis/packages/G5-settled-navigation-recovery-r1').iterdir():add(p)
for n in ['consumer.mjs','observer.mjs','schedule-observer.mjs','schedule-observer.test.mjs','contract.mjs','web.mjs','storage.mjs','storage.test.mjs','task-storage.py','run-browser.py','freeze-diagnostic.py',name+'-request.json']:add(O/n)
for n in ['implementation-design-baseline.json','preflight.json','storage-preflight.json']:add(E/n)
q=json.loads((O/(name+'-request.json')).read_text());sdk=Path(q['sdk'])
for p in sdk.rglob('*'):
 if p.is_file():add(p)
add(q['fixture']['path']);add(W/'verification/runners/g5/bounded_worker.py')
if name.endswith('PROGRESS'):
 add(E/'source-B-progress.json');add(E/'B-progress-SDK.json');add(E/'B-progress-build.json')
 for n in json.loads((E/'source-B-progress.json').read_text()):add(O/'source-B-progress'/n)
else:
 s=Path('/Users/qing/.codex/worktrees/g5-erase-feedback-frame/.aegis/results/G5-erase-feedback-frame-r1/source-C.json');add(s)
 for n in json.loads(s.read_text())['files']:add(W/n)
p=E/('freeze-'+name+'.json');assert not p.exists();p.write_text(json.dumps({'task':'G5-settled-navigation-recovery-r1','mode':name,'noProductionRepair':True,'files':files},indent=2)+'\n');print('freeze',name,len(files))
