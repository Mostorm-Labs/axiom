"""Offline integrity/correctness evaluation; never an official Gate decision."""
import gzip,hashlib,json,pathlib
R=pathlib.Path(__file__).resolve().parent
H=lambda b:hashlib.sha256(b).hexdigest()
def load(p):
 b=p.read_bytes();return json.loads(gzip.decompress(b) if str(p).endswith('.gz') else b)
manifest=load(R/'SHA256.json')
for name,digest in manifest.items():assert H((R/name).read_bytes())==digest,name
result=load(R/'result.json');assert result['status']=='BLOCKED_REPRODUCTION'
native=load(R/'native-comparison.json');assert not native['new_failed_names']
assert load(R/'checks/g5-repeat-gpu.json')['status']=='PASS'
assert load(R/'checks/g5-v4-final-affine-aligned.json')['status']=='PASS'
assert all(load(R/'checks/g5-repeat-demo-smoke.json')['checks'].values())
paired=load(R/'paired-content.json')
assert all(x['snapshotEqual'] and x['cpuExportEqual'] for rows in paired.values() for x in rows)
required={'cpu-on-dpr2','gpu-on-dpr2','cpu-off-dpr2','gpu-off-dpr2'}
seen=set();count=0
for p in sorted((R/'raw').glob('*/observation.json.gz')):
 name=p.parent.name;v=load(p);seen.add(name)
 if name in required:
  assert v['status']=='MEASURED',name
  assert v['sourceStable'] and v['artifactStable'] and not v['errors'],name
  assert not v['setup']['build']['dirty'],name
  assert all(all(q['raw']['checks'].values()) for q in v['phases']),name
  assert all(v['historyChecks'].values()),name
 for c in [v['initial'],*(q['after'] for q in v['phases']),v['undo'],v['redo']]:
  s=c['snapshot'];b=gzip.decompress((p.parent/(s['path']+'.gz')).read_bytes());assert len(b)==s['byteLength'] and H(b)==s['sha256'];count+=1
 if name=='gpu-on-dpr2':
  assert v['setup']['build']['sourceRevision']==result['code_revision']
  for phase in v['phases']:
   records=[json.loads(r['text'][19:]) for r in phase['raw']['nativeRecords'] if r['at']>=phase['raw']['upAt'] and r['text'].startswith('AXIOM_FRAME_TIMING ')]
   assert records and sum(f['counts']['gpu_coverage_tiles'] for f in records)>0
   for k in ['cpu_coverage_tiles','cpu_edge_pixels','coverage_target_fallback']:
    assert all(k in f['counts'] and f['counts'][k]==0 for f in records),k
assert required<=seen
for name in ['repeat-green-on','repeat-green-off','repeat-contained-off']:
 v=load(R/'raw'/name/'observation.json.gz');assert v['status']=='MEASURED'
 assert v['repeatNoOp']['upMs']<v['repeatNoOp']['boundMs'] and v['repeatNoOp']['snapshotEqual'] and v['repeatNoOp']['exportEqual']
 assert v['phases'][1]['raw']['after']['generation']==v['phases'][1]['raw']['before']['generation']
red=load(R/'raw/repeat-red/observation.json.gz');assert red['status']=='REPRODUCED_REPEAT_ERASE_CPU_TAIL' and red['repeatNoOp']['upMs']>=red['repeatNoOp']['boundMs']
assert load(R/'summary.json')['gpu-on-dpr4']['status']=='BLOCKED_EVIDENCE'
print(json.dumps({'status':'VERIFIED_LOCAL_EVIDENCE','files':len(manifest),'snapshot_checks':count,'gpu_coverage':'READY_FOR_CONTROL_REVIEW','user_input_rejection':'BLOCKED_REPRODUCTION'}))
