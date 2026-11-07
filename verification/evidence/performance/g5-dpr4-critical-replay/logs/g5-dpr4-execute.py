from pathlib import Path
import json,gzip,time,sys,subprocess,hashlib,re
R=Path.cwd();sys.path.insert(0,str(R/'verification/runners/g5'));from bounded_worker import run
O=R/'out/g5-dpr4-final';O.mkdir(exist_ok=True)
def load(p):return json.loads(gzip.decompress(p.read_bytes()))if p.suffix=='.gz'else json.loads(p.read_text())
def write(p,x):p.write_text(json.dumps(x,indent=2)+'\n')
def emit(x):print(json.dumps(x),flush=True)
log=R/'out/g5-dpr4-full-native.log'
while 'Total Test time' not in log.read_text(errors='replace'):time.sleep(2)
s=log.read_text(errors='replace');names=re.findall(r'^\s*\d+ - (.*?) \(',s.split('The following tests FAILED:')[-1],re.M);prior=load(R/'verification/evidence/performance/g5-post-main-followup/raw/native-comparison.json')['failed_names'];new=sorted(set(names)-set(prior))
report={'passed':1076-len(names),'total':1076,'failed_names':sorted(names),'new_failed_names':new,'resolved_names':sorted(set(prior)-set(names))};write(O/'native-comparison.json',report);emit({'stage':'native-complete','passed':report['passed'],'new_failed_names':new})
if not names or new:raise RuntimeError('native regression')
while not (R/'out/g5-dpr4-off/js/axiom.wasm').exists():time.sleep(2)
packages={}
for side in ['on','off']:
 for version,prefix in [('baseline','g5-followup'),('candidate','g5-dpr4')]:
  root=R/f'out/{prefix}-{side}/js';files=sorted(p for p in root.rglob('*')if p.is_file());meta=load(root/'package.json');assert len(files)==36 and list(meta['exports'])==['.']and not any(p.suffix in ['.h','.hpp','.a']for p in files)
  packages[version+'-'+side]={'root':str(root),'files':{str(p.relative_to(root)):hashlib.sha256(p.read_bytes()).hexdigest()for p in files},'version':meta['version']}
write(O/'packages.json',packages)
fixed=subprocess.check_output(['git','rev-parse','HEAD'],text=True).strip();cases=[('baseline-off-dpr4','g5-followup','off',4,False),('candidate-off-dpr4','g5-dpr4','off',4,False),('baseline-on-dpr4','g5-followup','on',4,False),('candidate-on-dpr4','g5-dpr4','on',4,False),('candidate-on-dpr4-repeat','g5-dpr4','on',4,False),('candidate-on-dpr1','g5-dpr4','on',1,False),('candidate-off-dpr1','g5-dpr4','off',1,False),('candidate-on-dpr2','g5-dpr4','on',2,False),('candidate-off-dpr2','g5-dpr4','off',2,False),('candidate-on-dpr4-restore','g5-dpr4','on',4,True)]
for name,prefix,side,dpr,restore in cases:
 emit({'stage':'browser-start','case':name});cmd=['node','verification/runners/g5/post_edit_navigation.mjs',str(R/f'out/{prefix}-{side}/js'),str(O/name),str(dpr)]
 if restore:cmd=['env','G5_CONTEXT_RESTORE=1',*cmd]
 w=run(cmd,O/(name+'-worker'),180,3072*1024);p=O/name/'observation.json.gz';b=load(p)if p.exists()else{};build=b.get('setup',{}).get('build',{});expected=fixed if prefix=='g5-dpr4'else'c05d3a2f54fcb44c771d797bbe3255f46dcac962'
 ok=w['reason']=='completed'and w['cleanup_complete']and b.get('status')=='MEASURED'and b.get('sourceStable')and b.get('artifactStable')and build.get('sourceRevision')==expected and build.get('dirty')is False
 pan=next((x for x in b.get('navigation',[])if x['id']=='idle-selected-pan'),{});gap=pan.get('feedback',{}).get('contentGapMs',{}).get('max');endpoint=prefix!='g5-dpr4'or dpr!=4 or (gap is not None and gap<150)
 emit({'stage':'browser-complete','case':name,'measured':bool(ok),'selectedPanGapMs':gap,'endpoint':endpoint,'error':b.get('error')})
 if not ok or not endpoint:raise RuntimeError('blocking browser/endpoint '+name)
for name,meta in [('10000-objects','out/g5-followup-final/scale/objects-10000-sparse/fixture.json'),('1000000-total-points','out/g5-followup-final/point-total/fixture.json')]:
 m=load(R/meta);assert hashlib.sha256(Path(m['snapshot_path']).read_bytes()).hexdigest()==m['snapshot_sha256'];emit({'stage':'scale-smoke','case':name});w=run(['node','verification/runners/g5/complex_scale_browser.mjs',str(R/'out/g5-dpr4-on/js'),str(O/name),'scale',str(R/meta),'2'],O/(name+'-worker'),180,3072*1024);emit({'stage':'scale-smoke-complete','case':name,'reason':w['reason']})
emit({'stage':'complete'})
