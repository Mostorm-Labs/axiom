import gzip,hashlib,json,shutil,statistics,subprocess,re
from pathlib import Path
ROOT=Path.cwd();DEST=ROOT/'verification/evidence/performance/g5-dense-navigation'
REV='32418c783170eea56a4131579fac627c003c91bb'
inputs=subprocess.check_output(['python3.12','-c','import json; from verification.tools.g5_preflight import ROOT, source_identity; print(json.dumps(source_identity(ROOT)))'],text=True)
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def read(p):return json.loads(Path(p).read_text())
def copy(src,dst):
 src=Path(src);dst=DEST/dst;dst.parent.mkdir(parents=True,exist_ok=True)
 if src.suffix=='.log' or (src.suffix=='.json' and src.stat().st_size>131072):
  dst=dst.with_suffix(dst.suffix+'.gz');dst.write_bytes(gzip.compress(src.read_bytes(),mtime=0))
 else:shutil.copyfile(src,dst)
DEST.mkdir(parents=True,exist_ok=True)
matrices={}
for mode in ['on','off']:
 source=Path('out/g5-dense-matrix-'+mode)
 for p in source.rglob('*'):
  if p.is_file():copy(p,Path(mode)/p.relative_to(source))
 matrices[mode]=read(source/'matrix.json')
 copy('out/g5-dense-matrix-'+mode+'.log',mode+'/driver.log')
for name in ['before','after','lead','limit','quarter','demand','stable']:
 source=Path('out/g5-dense-'+name)
 if not source.exists():continue
 for p in source.rglob('*'):
  if p.is_file():copy(p,Path('exploratory')/name/p.relative_to(source))
 for p in Path('out').glob('g5-dense-'+name+'-worker.*'):
  copy(p,Path('exploratory')/name/p.name)
for p in Path('out').glob('g5-dense-gpu-*'):
 if p.is_file():copy(p,Path('gpu')/p.name)
for name in ['final-red.json','final-red-dense.log','final-red-coarse.log','final-red-build.log',
 'native-comparison.json','native-targets.json','native-test-names.txt','related-build.log','related-tests.log',
 'final-native-build.log','final-native-tests.log','node.log','boundary.log','final-on-build.log','final-off-build.log']:
 copy('out/g5-dense-'+name,Path('checks')/name)
for name in ['package-checks.json','demo-smoke.json','demo.png','prefilled.png','demo-smoke.mjs','demo-smoke-worker.worker.json']:
 copy('out/g5-dense-'+name,Path('demo')/name)
comparison=[];all_measured=True;all_pixels=True;all_state=True;all_provenance=True;reductions={}
for mode,matrix in matrices.items():
 rows=matrix['results'];all_measured &= len(rows)==8 and all(r['status']=='MEASURED' for r in rows)
 paired=[]
 for case in sorted({r['case'] for r in rows}):
  a=next(r for r in rows if r['case']==case and r['side']=='before')
  b=next(r for r in rows if r['case']==case and r['side']=='after')
  old=read(Path('out/g5-dense-matrix-'+mode)/(case+'-before')/'summary.json')
  new=read(Path('out/g5-dense-matrix-'+mode)/(case+'-after')/'summary.json')
  all_provenance &= all(s['sourceStable'] and s['artifactStable'] for s in [old,new])
  all_provenance &= new['results'][0]['setup']['build']['sourceRevision']==REV
  all_provenance &= all(s['results'][0]['setupQuiescence']['stopped'] for s in [old,new])
  all_state &= all(all(s['results'][0]['checks'].values()) for s in [old,new])
  all_pixels &= old['results'][0]['setup']['initialPixels']['sha256']==new['results'][0]['setup']['initialPixels']['sha256']
  phases=[]
  for p,q in zip(a['phases'],b['phases']):
   same=p['pixels']['sha256']==q['pixels']['sha256'];all_pixels &= same
   phases.append({'id':p['id'],'before_gap_ms':p['summary']['input_display_submission_gap_ms']['max'],
    'after_gap_ms':q['summary']['input_display_submission_gap_ms']['max'],'same_full_pixels':same,
    'before_settle_ms':p['summary']['settle_ms'],'after_settle_ms':q['summary']['settle_ms'],
    'before_quality':(p['summary']['navigation'] or {}).get('quality'),'after_quality':(q['summary']['navigation'] or {}).get('quality')})
  comparison.append({'mode':mode,'case':case,'objects':[a['setup']['state']['objects'],b['setup']['state']['objects']],
   'selection_kind':[a['setup']['selectedKind'],b['setup']['selectedKind']],'phases':phases})
  if case in ['dense-pan','dense-selected-pan']:paired.extend(p for p in phases if p['id'] in ['repeat-0','repeat-1'])
 old_median=statistics.median(p['before_gap_ms'] for p in paired);new_median=statistics.median(p['after_gap_ms'] for p in paired)
 reductions[mode]={'before_median_ms':old_median,'after_median_ms':new_median,'ratio':new_median/old_median,'task_half_reduction':new_median<=old_median*.5}
(DEST/'comparison.json').write_text(json.dumps(comparison,indent=2)+'\n')
native=read('out/g5-dense-native-comparison.json');gpu=read('out/g5-dense-gpu-results.json')
assert read('out/g5-dense-demo-smoke.json')['passed']
assert '# fail 0' in Path('out/g5-dense-node.log').read_text()
result={'status':'READY_FOR_CONTROL_REVIEW' if all_measured and all_pixels and all_state and all_provenance and not native['new_failures'] and all(r['task_half_reduction'] for r in reductions.values()) and all(r['status']=='PASS' for r in gpu) else 'BLOCKED_EVIDENCE',
 'scope':'dense mixed Web navigation coverage repair; no formal G5/P34/platform acceptance',
 'implementation_revision':REV,'baseline_sdk_revisions':sorted({r['setup']['build']['sourceRevision'] for m in matrices.values() for r in m['results'] if r['side']=='before'}),'official_gate_pass_claimed':False,
 'measured':all_measured,'same_final_full_canvas_pixels':all_pixels,'state_and_receipt_checks':all_state,'provenance_checks':all_provenance,
 'pan_reduction':reductions,'native':native,'gpu':gpu,'node_tests':int(re.search(r'# tests (\d+)',Path('out/g5-dense-node.log').read_text())[1]),'boundary_contracts':int(re.search(r'passed \((\d+) contract',Path('out/g5-dense-boundary.log').read_text())[1]),
 'remaining_findings':['Zoom/pan combinations and resume still have nonzero submission tails; inspect comparison.json.',
 'Conservative three-image resident tile reservation unchanged; high-DPR resolution/residency limit requires a separate completion-owned staging design.',
 'Synthetic root SDK wheel input and real Chrome WebGL2, not physical Mac trackpad/input-to-photon or full Demo DOM cost.',
 'Old nine-kind snapshot authority and other baseline native failures remain unchanged.'],
 'rejected_candidates':'exploratory/ includes unsuccessful prototypes; not acceptance evidence for the final implementation. Dirty prototype source hashes/artifacts are recorded but those source states were not committed.',
 'demo':{'port':4219,'path':'/example.html','prefilled':'/mixed-navigation.html?dense=1','artifact':'out/g5-dense-final-off/js','old4218_preserved':True}}
(DEST/'result.json').write_text(json.dumps(result,indent=2)+'\n')
artifacts={mode:{str(p.relative_to(Path('out/g5-dense-final-'+mode+'/js'))):sha(p) for p in sorted(Path('out/g5-dense-final-'+mode+'/js').rglob('*')) if p.is_file()} for mode in ['on','off']}
(DEST/'consumer-artifacts.json').write_text(json.dumps(artifacts,indent=2)+'\n')

(DEST/'source-inputs.json').write_text(inputs)
files={str(p.relative_to(DEST)):sha(p) for p in sorted(DEST.rglob('*')) if p.is_file() and p.name!='SHA256.json'}
(DEST/'SHA256.json').write_text(json.dumps(files,indent=2)+'\n')
print(json.dumps({'status':result['status'],'pan_reduction':reductions,'files':len(files)}))
