import sys,time,json,gzip,subprocess,hashlib,re
from pathlib import Path
sys.path.insert(0,str(Path.cwd()))
sys.path.insert(0,str(Path('verification/runners/g5').resolve()))
from bounded_worker import run
R=Path.cwd(); out=R/'out/g5-followup-final';out.mkdir(exist_ok=True)
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def emit(x):print(json.dumps(x),flush=True)
log=R/'out/g5-followup-full-native.log'
while 'Total Test time' not in log.read_text(errors='replace'): time.sleep(2)
s=log.read_text(errors='replace');prev=json.loads((R/'verification/evidence/performance/g5-main-integration-2026-11-07/result.json').read_text())['native']['failed_names']
fail=s.split('The following tests FAILED:')[-1]
names=re.findall(r'^\s*\d+ - (.*?) \(',fail,re.M)
if not names:raise RuntimeError('cannot parse full native outcomes')
resolved=sorted(set(prev)-set(names));new=sorted(set(names)-set(prev));expected=['FC01Creation-arrow-arrow','FC01Creation-arrow-arrow-pointer','FC01Creation-line-line-pointer','FC01CreationSources']
report={'configured':1076,'failed_names':sorted(names),'passed':1076-len(names),'new_failed_names':new,'resolved_names':resolved,'prior':'verification/evidence/performance/g5-main-integration-2026-11-07/result.json'}
write(out/'native-comparison.json',report);emit({'stage':'native-complete',**{k:v for k,v in report.items()if k not in ['failed_names','prior']}})
if new or resolved!=sorted(expected):raise RuntimeError('unexpected native delta')
packages={}
for side in ['on','off']:
 root=R/f'out/g5-followup-{side}/js';files=sorted(p for p in root.rglob('*')if p.is_file());j=json.loads((root/'package.json').read_text());bad=[str(p)for p in files if p.suffix in ['.h','.hpp','.a']]
 if len(files)!=36 or bad or list(j['exports'])!=['.'] or j['version']!='0.4.3':raise RuntimeError('package boundary')
 packages[side]={'directory':str(root),'file_count':len(files),'exports':j['exports'],'version':j['version'],'files':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in files}}
write(out/'packages.json',packages)
for side,dpr,restore in [('on',1,False),('off',1,False),('on',2,False),('off',2,False),('on',4,False),('on',2,True),('off',2,True)]:
 name=f'{side}-dpr{dpr}'+('-restore'if restore else'');dest=out/name;emit({'stage':'edited-navigation-start','case':name})
 cmd=['node','verification/runners/g5/post_edit_navigation.mjs',str(R/f'out/g5-followup-{side}/js'),str(dest),str(dpr)]
 if restore:cmd=['env','G5_CONTEXT_RESTORE=1',*cmd]
 w=run(cmd,out/(name+'-worker'),180,3072*1024)
 p=dest/'observation.json.gz';body=json.loads(gzip.decompress(p.read_bytes()))if p.exists()else{}
 build=body.get('setup',{}).get('build',{});expectedrev=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip()
 stable=body.get('sourceStable')and body.get('artifactStable')and build.get('sourceRevision')==expectedrev and build.get('dirty')is False
 good=w['reason']=='completed'and w['cleanup_complete']and body.get('status')=='MEASURED'and stable
 emit({'stage':'edited-navigation-complete','case':name,'measured':bool(good),'reason':w['reason'],'status':body.get('status'),'sourceStable':bool(stable),'error':body.get('error')})
 if not good and dpr!=4:raise RuntimeError('required navigation '+name)
emit({'stage':'scale-matrix-start'})
with(out/'scale-driver.log').open('w')as f:
 code=subprocess.run(['python3.12','verification/runners/g5/complex_scale_matrix.py','out/host/axiom_g5_scale_fixture','out/g5-followup-on/js',str(out/'scale'),'--default-sdk','out/g5-followup-off/js'],stdout=f,stderr=subprocess.STDOUT).returncode
if code:raise RuntimeError('matrix orchestration')
emit({'stage':'scale-matrix-complete'})
with(out/'total-driver.log').open('w')as f:
 code=subprocess.run(['python3.12','verification/runners/g5/complex_scale_point_total.py','out/host/axiom_g5_scale_fixture','out/g5-followup-on/js',str(out/'point-total')],stdout=f,stderr=subprocess.STDOUT).returncode
if code:raise RuntimeError('point-total orchestration')
emit({'stage':'all-observations-complete'})
