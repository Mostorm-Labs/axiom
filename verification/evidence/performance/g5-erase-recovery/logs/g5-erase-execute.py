from pathlib import Path
import json,gzip,time,sys,subprocess,re
R=Path.cwd();sys.path.insert(0,str(R/'verification/runners/g5'));from bounded_worker import run
O=R/'out/g5-erase-final';O.mkdir(exist_ok=True)
def load(p):return json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
def emit(v):print(json.dumps(v),flush=True)
def write(p,v):p.write_text(json.dumps(v,indent=2)+'\n')
log=R/'out/g5-erase-full-ctest.log'
emit({'stage':'waiting-for-regressions'})
while not log.exists()or'Total Test time'not in log.read_text(errors='replace'):time.sleep(2)
while not(R/'out/g5-erase-pilot-baseline.worker.json').exists():time.sleep(2)
s=log.read_text(errors='replace');total=int(re.search(r'/([0-9]+) Test',s).group(1));names=re.findall(r'^\s*\d+ - (.*?) \(',s.split('The following tests FAILED:')[-1],re.M);prior=load(R/'verification/evidence/performance/g5-dpr4-critical-replay/raw/native-comparison.json')['failed_names'];new=sorted(set(names)-set(prior));v={'passed':total-len(names),'total':total,'failed_names':sorted(names),'new_failed_names':new,'resolved_names':sorted(set(prior)-set(names))};write(O/'native-comparison.json',v);emit({'stage':'regressions-complete',**v})
if not names or new:raise RuntimeError('new native failure')
for name,binary,required in [('geometry-before','out/g5-erase-geometry-before/axiom_native_transport_host',False),('candidate','out/host/sdk/common/axiom_native_transport_host',True)]:
 
 if (O/(name+'-native.json')).exists():
  emit({'stage':'native-reuse','case':name,'reason':'Only Web test entropy/error observation changed;native runner and binaries unchanged'});continue
 emit({'stage':'native-start','case':name});cmd=['python3.12','verification/runners/g5/dense_erase_native.py',binary,str(O/(name+'-native.json'))]+(['--expect-repair']if required else[]);w=run(cmd,O/(name+'-native-worker'),180,2048*1024);emit({'stage':'native-complete','case':name,'worker':w});
 if w['reason']!='completed'or not w['cleanup_complete']:raise RuntimeError('native '+name)
fixed=subprocess.check_output(['git','rev-parse','b47781cc'],text=True).strip()
for name,prefix,dpr,mode in [('baseline-off-dpr2','g5-dpr4-off',2,'baseline'),('candidate-off-dpr2','g5-erase-off',2,'candidate'),('candidate-on-dpr2','g5-erase-on',2,'candidate'),('candidate-off-dpr2-repeat','g5-erase-off',2,'candidate'),('candidate-off-dpr1','g5-erase-off',1,'candidate'),('candidate-off-dpr4','g5-erase-off',4,'candidate')]:
 emit({'stage':'dense-start','case':name});w=run(['node','verification/runners/g5/dense_erase.mjs',str(R/f'out/{prefix}/js'),str(O/name),str(dpr),mode],O/(name+'-worker'),180,3072*1024);p=O/name/'observation.json.gz';b=load(p)if p.exists()else{};expected=fixed if mode=='candidate'else'5ab41d5f8782958f8bdcbf329f81929da55c2f03';status='MEASURED'if mode=='candidate'else'REPRODUCED_REJECTION_BUG';ok=w['reason']=='completed'and w['cleanup_complete']and b.get('status')==status and b.get('sourceStable')and b.get('artifactStable')and b.get('setup',{}).get('build',{}).get('sourceRevision')==expected and b.get('setup',{}).get('build',{}).get('dirty')is False
 emit({'stage':'dense-complete','case':name,'ok':bool(ok),'status':b.get('status'),'error':b.get('error'),'upMs':[s['upMs']for s in b.get('sweeps',[])]})
 if not ok:raise RuntimeError('dense '+name)
emit({'stage':'capacity-start'});w=run(['node','verification/sdk_js/verify_partial_erase_capacity.mjs',str(R/'out/g5-erase-off/js'),str(O/'capacity.json'),'2'],O/'capacity-worker',180,3072*1024);emit({'stage':'capacity-complete','worker':w})
if w['reason']!='completed'or not w['cleanup_complete']:raise RuntimeError('capacity regression')
for name,prefix in [('baseline-post-edit-dpr4','g5-dpr4-on'),('candidate-post-edit-dpr4','g5-erase-on')]:
 emit({'stage':'post-edit-start','case':name});w=run(['node','verification/runners/g5/post_edit_navigation.mjs',str(R/f'out/{prefix}/js'),str(O/name),'4'],O/(name+'-worker'),180,3072*1024);p=O/name/'observation.json.gz';b=load(p)if p.exists()else{};ok=w['reason']=='completed'and w['cleanup_complete']and b.get('status')=='MEASURED'and b.get('sourceStable')and b.get('artifactStable');emit({'stage':'post-edit-complete','case':name,'ok':bool(ok),'error':b.get('error')})
 if not ok:raise RuntimeError('post-edit '+name)
emit({'stage':'complete'})
