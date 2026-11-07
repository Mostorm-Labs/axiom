import gzip,hashlib,json,pathlib,shutil,subprocess,xml.etree.ElementTree as ET,collections
ROOT=pathlib.Path.cwd(); OUT=ROOT/'verification/evidence/performance/g5-v4-gpu-coverage'
PROD='10a9a0f0'; VER='HEAD'
H=lambda b:hashlib.sha256(b).hexdigest()
def put(name,data):
 p=OUT/name;p.parent.mkdir(parents=True,exist_ok=True);p.write_bytes(data)
def write(name,v):put(name,(json.dumps(v,indent=2,ensure_ascii=False)+'\n').encode())
def load(path):
 p=ROOT/path
 return json.loads(gzip.decompress(p.read_bytes()) if str(p).endswith('.gz') else p.read_bytes())
def copy(src,dst):put(dst,(ROOT/src).read_bytes())
cohorts={
 'cpu-on-dpr2':'out/g5-v4-measure-cpu-on-dpr2',
 'gpu-on-dpr2':'out/g5-repeat-unique-on-data',
 'cpu-off-dpr2':'out/g5-v4-measure-cpu-off-dpr2',
 'gpu-off-dpr2':'out/g5-repeat-unique-off-data',
 'gpu-on-dpr1':'out/g5-v4-measure-gpu-on-dpr1',
 'gpu-on-dpr4':'out/g5-v4-measure-gpu-on-dpr4',
 'repeat-red':'out/g5-repeat-red-data',
 'repeat-green-on':'out/g5-repeat-green-on-data',
 'repeat-green-off':'out/g5-repeat-green-off-data',
 'repeat-contained-off':'out/g5-repeat-contained-off-data',
 'gpu-before-fast-miss-on-dpr2':'out/g5-v4-measure-gpu-on-dpr2',
 'gpu-before-fast-miss-off-dpr2':'out/g5-v4-measure-gpu-off-dpr2',
 'gpu-lighten-prototype':'out/g5-v4-loopy-candidate-data',
 'gpu-srcover-exploration':'out/g5-v4-batched-explore-data',
}
def stages(phase,postup=True):
 rows=[json.loads(x['text'][19:]) for x in phase['raw']['nativeRecords'] if x['text'].startswith('AXIOM_FRAME_TIMING ') and x['at']>=(phase['raw']['upAt'] if postup else phase['raw']['started'])]
 cs=collections.Counter();ss=collections.Counter()
 for f in rows:
  cs.update(f['counts'])
  for k,v in f['stages'].items():ss[k]+=v['inclusive_ns']/1e6
 def count(k):return sum(f['counts'][k] for f in rows) if rows and all(k in f['counts'] for f in rows) else None
 cpu=lambda m:next((v['value'] for v in m if v['name']=='TaskDuration'),None)
 a,b=cpu(phase['cpuBefore']),cpu(phase['cpuAfter'])
 return {'timing':phase['timing'],'gpuTiles':count('gpu_coverage_tiles'),'cpuTiles':count('cpu_coverage_tiles'),'gpuFallbackTiles':count('coverage_target_fallback'),'cpuPolicyTiles':count('coverage_cpu_policy'),'noGpuContextTiles':count('coverage_no_gpu_context'),'cpuScalarTiles':count('scalar_coverage_tiles'),'gpuCommandCpuWallMs':ss.get('gpu_coverage_submit') if rows else None,'gpuFlushCpuWallMs':ss.get('coverage_flush') if rows and all('coverage_flush' in f['stages'] for f in rows) else None,'wholeBrowserTaskCpuMs':None if a is None or b is None else (b-a)*1000,'checks':phase['raw']['checks'],'inputUpMs':phase['raw']['upMs'],'maxContentGapMs':phase['feedback']['contentGapMs']['max'],'endpoint':phase['endpoint']}
def checkpoints(p):return [p['initial'],*(x['after'] for x in p['phases']),p['undo'],p['redo']]
observations={};summary={}
for name,directory in cohorts.items():
 p=ROOT/directory/'observation.json.gz'
 if not p.exists():continue
 v=load(str(p.relative_to(ROOT)));observations[name]=v;copy(str(p.relative_to(ROOT)),f'raw/{name}/observation.json.gz')
 for chk in checkpoints(v):
  s=chk['snapshot'];data=(ROOT/directory/s['path']).read_bytes();assert H(data)==s['sha256'];put(f'raw/{name}/'+s['path']+'.gz',gzip.compress(data,mtime=0))
 summary[name]={'status':v['status'],'fixture':v['fixture'],'dpr':v['dpr'],'build':v['setup']['build'],'sourceStable':v['sourceStable'],'artifactStable':v['artifactStable'],'history':v['historyChecks'],'repeatNoOp':v.get('repeatNoOp'),'errors':v['errors'],'phases':[stages(x) for x in v['phases']],'allPhase':[stages(x,False) for x in v['phases']]}
summary.setdefault('gpu-on-dpr4',{'status':'BLOCKED_EVIDENCE','reason':'180second worker timeout after two correct erases;Undo/Redo closure incomplete','worker':'workers/g5-v4-measure-gpu-on-dpr4-worker.worker.json'})
for f in ['out/g5-v4-measure-gpu-on-dpr4-worker.stdout.log','out/g5-v4-measure-gpu-on-dpr4-worker.stderr.log']:
 if (ROOT/f).exists():put('logs/'+pathlib.Path(f).name+'.gz',gzip.compress((ROOT/f).read_bytes(),mtime=0))
write('summary.json',summary)
pairs={}
for a,b in [('cpu-on-dpr2','gpu-on-dpr2'),('cpu-off-dpr2','gpu-off-dpr2')]:
 x,y=observations[a],observations[b];pairs[a+' vs '+b]=[{'checkpoint':name,'snapshotEqual':u['snapshot']['sha256']==v['snapshot']['sha256'],'cpuExportEqual':u['image']['sha256']==v['image']['sha256']} for name,u,v in zip(['initial','erase0','erase1','undo','redo'],checkpoints(x),checkpoints(y))]
write('paired-content.json',pairs)
xml=ROOT/'out/g5-repeat-full-ctest.xml';tree=ET.parse(xml);tests=tree.findall('.//testcase');bad=sorted(x.get('name') for x in tests if x.find('failure') is not None or x.find('skipped') is not None)
old=load('out/g5-masked-final/native-comparison.json');native={'total':len(tests),'passed':len(tests)-len(bad),'failed_names':bad,'prior_failed_names':old['failed_names'],'new_failed_names':sorted(set(bad)-set(old['failed_names'])),'resolved_names':sorted(set(old['failed_names'])-set(bad))};write('native-comparison.json',native);copy('out/g5-repeat-full-ctest.xml','raw/native-final.xml')
for name in ['g5-v4-red-valid.json','g5-v4-blend-red.json','g5-v4-final-gpu.json','g5-repeat-gpu.json','g5-v4-final-affine-aligned.json','g5-v4-final-demo-smoke.json','g5-repeat-demo-smoke.json']:
 if (ROOT/'out'/name).exists():copy('out/'+name,'checks/'+name)
for p in sorted([*(ROOT/'out').glob('g5-v4-*.worker.json'),*(ROOT/'out').glob('g5-repeat-*.worker.json')]):copy(str(p.relative_to(ROOT)),'workers/'+p.name)
for name in ['g5-repeat-host-build.log','g5-repeat-full-ctest.log','g5-repeat-native.log','g5-repeat-node.log','g5-repeat-boundaries.log','g5-repeat-collector.log','g5-repeat-on-config.log','g5-repeat-on-build.log','g5-repeat-on-install.log','g5-repeat-off-config.log','g5-repeat-off-build.log','g5-repeat-off-install.log','g5-v4-final-host-build.log','g5-v4-final-ctest.log','g5-v4-node.log','g5-v4-boundaries.log','g5-v4-collector.log','g5-v4-final-masked-native.log','g5-v4-final-fallback-native.log','g5-v4-final-on-config.log','g5-v4-final-on-build.log','g5-v4-final-on-install.log','g5-v4-final-off-config.log','g5-v4-final-off-build.log','g5-v4-final-off-install.log','g5-v4-sdk-offline.log']:
 if (ROOT/'out'/name).exists():put('logs/'+name+'.gz',gzip.compress((ROOT/'out'/name).read_bytes(),mtime=0))
for p in ['out/g5-repeat-contained.mjs','out/g5-repeat-final-measures.py','out/g5-v4-run-measures.py','out/g5-repeat-native-comparison.json','out/g5-repeat-demo.png','out/g5-repeat-demo-smoke.mjs','out/g5-repeat-native-binaries-before.json','out/g5-repeat-native-binaries-after.json','out/g5-repeat-native.log','out/g5-v4-final-demo.png','out/g5-v4-final-demo-smoke.mjs','out/g5-v4-extra-probe.mjs']:
 if (ROOT/p).exists():copy(p,'checks/'+pathlib.Path(p).name)
write('result.json',{'status':'BLOCKED_REPRODUCTION','gpu_coverage':'READY_FOR_CONTROL_REVIEW','repeat_noop':'READY_FOR_CONTROL_REVIEW','dpr4':'BLOCKED_EVIDENCE_TIMEOUT','scope':'Local Web/Ganesh engineering evidence, not physical input, competitor, full scale, P34 or release acceptance','code_revision':subprocess.check_output(['git','rev-parse',PROD],text=True).strip(),'verification_revision':subprocess.check_output(['git','rev-parse',VER],text=True).strip(),'input_rejection':'The reported second-erase INPUT_REJECTED was not reproduced. No semantic rejection bypass or claimed fix. Diagnostic-only stage logs added to existing owners.','runtime_cpu_still_owns':['input normalization','pinned contour compilation','exact erase hit/no-op filtering','OperationEngine/History/Scene'],'budgets_MiB':{'optional':32,'shared':96},'preserved':['Brush algorithm and semantic document/Operations','CPU Direct bytes','SDK public APIs/ABI/version','dependency locks','old P34 evidence','old user pages']})
# Validation below is engineering evidence evaluation, not an official Gate.
assert not native['new_failed_names'],native['new_failed_names']
for k in ['cpu-on-dpr2','gpu-on-dpr2','cpu-off-dpr2','gpu-off-dpr2']:
 v=observations[k];assert v['sourceStable'] and v['artifactStable'] and not v['errors'];assert all(all(p['raw']['checks'].values()) for p in v['phases']);assert all(v['historyChecks'].values())
assert all(row['snapshotEqual'] and row['cpuExportEqual'] for v in pairs.values() for row in v)
assert load('out/g5-repeat-gpu.json')['status']=='PASS'
assert observations['repeat-contained-off']['status']=='MEASURED'
assert observations['repeat-red']['status']=='REPRODUCED_REPEAT_ERASE_CPU_TAIL'
for name in ['repeat-green-on','repeat-green-off']:
 v=observations[name];assert v['status']=='MEASURED' and v['sourceStable'] and v['artifactStable']
 assert v['repeatNoOp']['upMs']<v['repeatNoOp']['boundMs'] and v['repeatNoOp']['snapshotEqual'] and v['repeatNoOp']['exportEqual']
 for a,b in zip(checkpoints(observations['repeat-red']),checkpoints(v)):
  assert a['snapshot']['sha256']==b['snapshot']['sha256'] and a['image']['sha256']==b['image']['sha256']
assert all(s['gpuTiles'] and s['cpuTiles']==0 and s['gpuFallbackTiles']==0 for s in summary['gpu-on-dpr2']['phases'])
write('SHA256.json',{str(p.relative_to(OUT)):H(p.read_bytes()) for p in sorted(OUT.rglob('*')) if p.is_file() and p.name!='SHA256.json'})
print(json.dumps({'directory':str(OUT),'native':{k:native[k] for k in ['total','passed','new_failed_names']},'status':'BLOCKED_REPRODUCTION','gpu_coverage':'READY_FOR_CONTROL_REVIEW'}))
