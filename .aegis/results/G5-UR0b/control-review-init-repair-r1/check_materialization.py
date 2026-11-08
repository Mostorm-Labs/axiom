from pathlib import Path
import json,hashlib,sys,subprocess
R=Path(__file__).resolve().parents[4];C=Path(__file__).resolve().parent;D=R/'.aegis/results/G5-UR0b/init-repair-r1'
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest();load=lambda p:json.loads(p.read_text())
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip()=='codex/geometry-navigation-integration'
result=load(D/'result.json');assert sha(D/'result.json')=='5a2a36131ef0b4ad5c389a9179873f51a6fa63a5db15557c6f9fe83b10707773'
P=R/result['package_ref']['path'];assert sha(P)==result['package_ref']['sha256']=='247da63294671d763da805758e88bf449db111150f7c37ff7fa2c1272781e710';pkg=load(P)
for k in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',pkg[k]['revision'],'HEAD'],cwd=R,check=True)
for k in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 ref=pkg[k];p=(P.parent/ref['path']).resolve();assert p.parent==P.parent and sha(p)==ref['sha256']
request=load(R/'.aegis/verification/G5-UR0b-initialization-v1/implementation-request.json');contract=load(P.parent/'execution-contract.json')['EXECUTION_CLOSURE_CONTRACT']
assert contract['authorized_source_paths']==request['source_scope'];assert contract['new_sampling_workers_max']==0
bundle=Path(result['materialized_ref']['path']);assert bundle.is_relative_to(R);assert sha(bundle/'manifest.json')==result['materialized_ref']['manifest_sha256']
def verify_manifest(p):
 m=load(p);bad=[]
 for name,info in m['files'].items():
  f=p.parent/name
  if not f.is_file() or sha(f)!=info['sha256'] or 'bytes' in info and f.stat().st_size!=info['bytes']:bad.append(name)
 assert not bad,(str(p),bad)
 return {'path':str(p.relative_to(R)),'files':len(m['files']),'mismatches':bad}
manifest_checks=[verify_manifest(bundle/'manifest.json')]
preserve=load(D/'preservation.json')
for h in preserve['historical']:
 assert sha(R/h['result_ref']['path'])==h['result_ref']['sha256'];assert sha(R/h['manifest_ref']['path'])==h['manifest_ref']['sha256'];manifest_checks.append(verify_manifest(R/h['manifest_ref']['path']))
refs=[]
def walk_refs(x):
 if isinstance(x,dict):
  if 'path' in x and 'sha256' in x:
   p=(R/x['path']).resolve();assert p.is_relative_to(R) and p.is_file();assert sha(p)==x['sha256'],str(p);refs.append(str(p.relative_to(R)))
  for v in x.values():walk_refs(v)
 elif isinstance(x,list):
  for v in x:walk_refs(v)
for name in ['authority.lock.json','verification.lock.json','evidence-contract.json']:walk_refs(load(P.parent/name))
for name in ['result.json','oracle-coverage.json','red-precondition.json']:walk_refs(load(D/name))
sys.path.insert(0,str(R/'verification/runners/g5/ur0'));import source_identity
current=source_identity.identity(R);assert current['source_tree_sha256']==result['source_tree_sha256']=='2ca47a17ae94034a574379e32ae118b95b5f7c35fee9706be91dc21c46477ebb'
assert current['files']==load(bundle/'source/full-source-identity.json')['files']
start=load(D/'starting-source-identity.json');changed=sorted(n for n in set(start['files'])|set(current['files']) if start['files'].get(n)!=current['files'].get(n));assert len(changed)==10 and set(changed)<=set(request['source_scope'])
audit=load(D/'source-scope-audit.json');assert changed==audit['changed_paths']
# Accepted sampling scope stays unchanged, including exact old window bytes.
window=R/'.aegis/results/G5-UR0b/resume-r3/window.json';reuse=load(D/'reuse-assessment.json');assert sha(window)==reuse['old_window_sha256'];w=load(window);assert w['status']=='CLOSED' and w['charged_seconds']==1133
j={'status':'PACKAGE_SOURCE_AND_EVIDENCE_VERIFIED','result_sha256':sha(D/'result.json'),'package_sha256':sha(P),'source_tree_sha256':current['source_tree_sha256'],'source_entries':len(current['files']),'changed_source_paths':changed,'manifest_checks':manifest_checks,'hash_bound_refs_checked':len(refs),'old_window':'CLOSED','sampling_debit_seconds':1133,'new_browser_workers_by_review':0,'product_source_changes':[]}
(C/'identity-check.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps(j))
