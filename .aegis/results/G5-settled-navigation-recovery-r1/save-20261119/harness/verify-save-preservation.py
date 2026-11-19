from pathlib import Path
import datetime,hashlib,json,subprocess,re
W=Path('/Users/qing/.codex/worktrees/g5-settled-navigation-recovery')
E=W/'.aegis/results/G5-settled-navigation-recovery-r1'
S=E/'save-20261119'
B=Path('/Users/qing/.codex/worktrees/g5-erase-feedback-frame')
C=Path('/Users/qing/.codex/worktrees/g5-exposure-checkpoint/axiom')
O=W/'out/G5-settled-navigation-recovery-r1'
failures=[]
def read(p): return json.loads(p.read_text())
def meta(p):
 h=hashlib.sha256()
 with p.open('rb') as f:
  while b:=f.read(4*1024*1024): h.update(b)
 return {'path':str(p),'bytes':p.stat().st_size,'sha256':h.hexdigest()}
def check(ok,msg):
 if not ok: failures.append(msg)
def exact(r):
 p=Path(r['path'])
 try: check(all(meta(p)[k]==r[k] for k in ['bytes','sha256']),str(p))
 except OSError as ex: failures.append(str(ex))
def git(root,*args): return subprocess.check_output(['git','-C',str(root),*args],text=True).strip()
for row in read(S/'preservation-before.json')['originalFiles']: exact(row)
archives=read(S/'archive-map.json')['entries']
for row in archives:
 exact(row['original']);exact(row['saved'])
 check(row['original']['sha256']==row['saved']['sha256'],'archive not exact')
probe=read(E/'source-B-progress.json')
for n,h in probe.items():
 for root in [W,O/'source-B-progress']:
  check(meta(root/n)['sha256']==h,'probe source '+str(root/n))
listeners=[]
for old in read(E/'preservation-before.json')['listenersCurrent']:
 pid=old.split()[0]
 r=subprocess.run(['ps','-p',pid,'-o','pid=','-o','command='],text=True,stdout=subprocess.PIPE)
 actual=r.stdout.strip()
 check(r.returncode==0 and actual==old,'protected listener '+old)
 listeners.append({'before':old,'actual':actual,'equal':actual==old})
primary=Path('/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom')
check(not git(primary,'diff','--cached','--name-only'),'protected primary index changed')
check(not git(W,'diff','--cached','--name-only'),'save must be checked before staging')
check(git(W,'branch','--show-current')=='codex/g5-settled-navigation-recovery','execution branch')
check(git(W,'rev-parse','HEAD')=='c3a84c289d5b3dcb258fcbe7f481febf227df0c0','execution HEAD')
expected={str(Path(row['path']).relative_to(W)) for row in read(E/'diagnostic-source-delta.json')['files']}
check(set(git(W,'diff','--name-only').splitlines())==expected,'unexpected tracked mutation')
for row in read(E/'diagnostic-source-delta.json')['files']:exact(row)
for row in read(E/'root-dependency-binding.json')['files']:exact(row)
check(all(row['exit']==0 for row in read(E/'B-progress-build.json')),'old strict build exit')
logs=[meta(O/('B-progress-'+name+'.log')) for name in ['configure','build','install']]
for name in ['configure','build','install']:check((O/('B-progress-'+name+'.log')).is_file(),'missing old build log')
for n in ['node','boundary','source-whitespace','offline-audit']:
 check(read(S/(n+'.command.json'))['exit']==0,'fresh check exit '+n)
audit=read(S/'offline-binding-recheck.json')
check(not audit['failures'],'offline audit failures')
check(audit['manifestFileCount']==2016 and len(audit['freezeCounts'])==5 and len(audit['rawRecomputation'])==16,'audit coverage')
check(len(read(B/'.aegis/results/G5-erase-feedback-frame-r1/C-normal-SDK.json')['files'])==42,'normal SDK count')
check(len(read(E/'B-progress-SDK.json')['files'])==42,'probe SDK count')
# Only inspect the process table: never start, stop, or contact a browser/demo.
processes=subprocess.check_output(['ps','-axo','pid=,ppid=,command='],text=True).splitlines()
active=[]
for line in processes:
 if str(W) in line or str(C) in line:
  if re.search(r'(node |chrome |chromium |cmake |ninja |run-browser\.py|run-corrective\.py|build-probe\.py)',line) and ' - <<' not in line:
   active.append(line.strip())
check(not active,'unexpected active task browser/build family')
record={'schema':'axiom-checkpoint-preservation-recheck-v1','at':datetime.datetime.now(datetime.timezone.utc).isoformat(),'failures':failures,'originalFileCount':len(read(S/'preservation-before.json')['originalFiles']),'archiveEntries':len(archives),'probeSource1602Each':{'current':len(probe),'snapshot':len(probe)},'normalAndProbeSDKFilesEach':42,'protectedListeners':listeners,'primaryIndexEmpty':True,'freshNodeTests':6,'freshBoundaryContracts':122,'oldStrictBuild':{'rerun':False,'record':meta(E/'B-progress-build.json'),'logs':logs,'qualificationOffFrameTimingOn':True,'thirdPartyRebuild':False},'auditReplay':{'independentControlReview':False,'description':'Exact archived control audit script replayed offline in new save output; prior independent review left unchanged.','record':meta(S/'offline-binding-recheck.json')},'activeTaskBrowserOrBuildProcesses':active,'newSamplingStarts':0,'originalSamplingStarts':5,'originalChargedSeconds':590,'verdict':'BLOCKED / ROOT_CAUSE_NOT_ESTABLISHED','originalEFF4':'UNMET; performance problem not closed','normalDemo':'http://127.0.0.1:4349/example.html','browserAndDocuments':'No browser control, navigation, refresh, capture or document mutation performed during save.'}
(S/'preservation-recheck.json').write_text(json.dumps(record,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'failures':failures,'originals':record['originalFileCount'],'archiveEntries':len(archives),'probeSourceFilesEach':len(probe),'listeners':len(listeners),'existingStrictBuildLogsBound':len(logs),'newSamplingStarts':0},ensure_ascii=False,indent=2))
raise SystemExit(bool(failures))
