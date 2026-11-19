from pathlib import Path
import json,os,struct,time,fcntl
W=Path('/Users/qing/.codex/worktrees/g5-settled-navigation-recovery')
O=W/'out/G5-settled-navigation-recovery-r1';E=W/'.aegis/results/G5-settled-navigation-recovery-r1'
EXCLUDE={'source-B-progress','build-B-progress','install-B-progress','source-C-progress','build-C-progress','install-C-progress','source-B-probe','build-B-probe','install-B-probe','source-C-probe','build-C-probe','install-C-probe','source-C-normal','build-C-normal','install-C-normal','source-C','build-C-native','build-C-normal-native','build-native','build-diagnostic','build-normal','build-normal-native','install-diagnostic','install-normal','source-diagnostic-B'}
LEDGER=E/'storage-ledger.json'
def inventory():
 files={}
 for root in (O,E):
  for current,dirs,names in os.walk(root,followlinks=False):
   dirs[:]=[n for n in dirs if n not in EXCLUDE and not (Path(current)/n).is_symlink()]
   for name in names:
    p=Path(current)/name
    if p.is_symlink() or p==LEDGER or name.startswith('storage-ledger.json.')or name=='storage-ledger.lock':continue
    try:n=p.stat().st_size
    except FileNotFoundError:continue
    logical=n
    if name.endswith('.gz') and n>=18:
     with p.open('rb')as f:f.seek(-4,2);logical=struct.unpack('<I',f.read(4))[0]
    files[str(p)]={'retained':n,'logical':logical}
 return files

def _update(projected_retained=0,projected_logical=0):
 old=json.loads(LEDGER.read_text())if LEDGER.exists()else{'schema':'axiom-ien-task-storage-v1','paths':{},'peakRetainedBytes':0,'logicalBytes':0}
 files=inventory()
 for path,v in files.items():old['paths'][path]=max(old['paths'].get(path,0),v['logical'])
 retained=sum(x['retained']for x in files.values());logical=sum(old['paths'].values())
 old.update({'actualRetainedBytes':retained,'logicalBytes':logical,'peakRetainedBytes':max(retained,old['peakRetainedBytes']),'retainedCapBytes':4096*1048576,'logicalCapBytes':8192*1048576,'updatedUnix':time.time(),'exclusions':sorted(EXCLUDE),'semantics':'Cumulative per-artifact decoded-byte highwater including failed outputs; gzip and serial scratch raw share artifact identity; rewriting metadata keeps its maximum observed footprint. No ledger reset or deletion credit. Completed pixel raw length is audited by streaming writer.'})
 # Temporary raw oracle output becomes its gzip under the same artifact identity.
 for path in list(old['paths']):
  if path.endswith('.gz') and path[:-3]in old['paths']:
   old['paths'][path]=max(old['paths'][path],old['paths'].pop(path[:-3]))
 logical=sum(old['paths'].values());old['logicalBytes']=logical
 old['projectedRetainedBytes']=retained+projected_retained;old['projectedLogicalBytes']=logical+projected_logical
 old['status']='CLEAR'if retained+projected_retained<=4096*1048576 and logical+projected_logical<=8192*1048576 else'LIMIT_EXCEEDED'
 temp=LEDGER.with_suffix('.json.tmp');temp.write_text(json.dumps(old,indent=2)+'\n');temp.replace(LEDGER)
 if old['status']!='CLEAR':raise RuntimeError('storage limit '+json.dumps({k:v for k,v in old.items()if k!='paths'}))
 return old
def update(projected_retained=0,projected_logical=0):
 with (E/'storage-ledger.lock').open('a')as lock:
  fcntl.flock(lock,fcntl.LOCK_EX)
  return _update(projected_retained,projected_logical)
if __name__=='__main__':
 import sys
 d=update(*(int(v)for v in sys.argv[1:]));print(json.dumps({k:v for k,v in d.items()if k!='paths'}))
