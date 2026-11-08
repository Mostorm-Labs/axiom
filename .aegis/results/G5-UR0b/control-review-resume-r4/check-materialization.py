from pathlib import Path
import json,hashlib,math,subprocess,sys
R=Path(__file__).resolve().parents[4];C=Path(__file__).resolve().parent;D=R/'.aegis/results/G5-UR0b/resume-r4'
load=lambda p:json.loads(p.read_text());sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git';assert subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip()=='codex/geometry-navigation-integration'
result=load(D/'result.json');assert sha(D/'result.json')=='d764ac56a2de69a47b9048a5556470a2e4bb68eb4cfe9a395b24a45ddd8094c3'
P=R/result['package_ref']['path'];assert sha(P)==result['package_ref']['sha256']=='2cadd6e12c73dc4006fd20691feca9402974a9f31fe705b2200c4654e020b569';package=load(P)
for k in ['task_anchor','resume_cursor']:subprocess.run(['git','merge-base','--is-ancestor',package[k]['revision'],'HEAD'],cwd=R,check=True)
for k in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 ref=package[k];assert (P.parent/ref['path']).resolve().parent==P.parent and sha(P.parent/ref['path'])==ref['sha256']
B=Path(result['materialized_ref']['path']);assert B.is_relative_to(R) and sha(B/'manifest.json')==result['materialized_ref']['manifest_sha256']
def check_manifest(p):
 m=load(p);bad=[]
 for n,info in m['files'].items():
  f=p.parent/n
  if not f.is_file() or sha(f)!=info['sha256'] or 'bytes' in info and f.stat().st_size!=info['bytes']:bad.append(n)
 assert not bad,(str(p),bad)
 return {'manifest':str(p.relative_to(R)),'files':len(m['files']),'mismatches':bad}
checks=[check_manifest(B/'manifest.json')]
for item in result['history_preservation']:
 assert sha(R/item['result_ref']['path'])==item['result_ref']['sha256'];assert sha(R/item['manifest_ref']['path'])==item['manifest_ref']['sha256'];checks.append(check_manifest(R/item['manifest_ref']['path']))
refs=[]
def walk(x):
 if isinstance(x,dict):
  if 'path' in x and 'sha256' in x:
   p=(R/x['path']).resolve();assert p.is_relative_to(R) and p.is_file() and sha(p)==x['sha256'],str(p);refs.append(str(p.relative_to(R)))
  for v in x.values():walk(v)
 elif isinstance(x,list):
  for v in x:walk(v)
for n in ['result.json','mechanism-proofs.json']:walk(load(D/n))
sys.path.insert(0,str(R/'verification/runners/g5/ur0'));import source_identity
current=source_identity.identity(R);assert current['source_tree_sha256']==result['source_tree_sha256']=='d33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a';assert current['files']==load(B/'source/full-source-identity.json')['files']
lock=load(P.parent/'authority.lock.json');base=load(R/lock['accepted_source_identity_ref']['path']);delta=sorted(n for n in set(base['files'])|set(current['files']) if base['files'].get(n)!=current['files'].get(n));packagepaths={str(f.relative_to(R)) for f in P.parent.iterdir() if f.is_file()};code_delta=sorted(set(delta)-packagepaths);assert code_delta==['verification/runners/g5/ur0/remaining_runner.py','verification/tests/test_g5_ur0_remaining_runner.py']
w=load(D/'window.json');assert w['status']=='CLOSED' and len(w['rows'])==38
started=[r for r in w['rows'] if 'outcome' in r];assert len(started)==3;charged=0;guard_counts={}
for row in started:
 g=row['outcome']['guard'];assert g['cleanup_complete'];cost=math.ceil(g['elapsed_s']);assert cost==row['charged_seconds'];charged+=cost;guard_counts[g['reason']]=guard_counts.get(g['reason'],0)+1
assert charged==118 and w['charged_seconds']==1251 and w['reserved_seconds']==0;assert sum(r['status']=='MISSING' for r in w['rows'])==35
stop=next(r for r in started if r['run_id'].startswith('correctness'));g=stop['outcome']['guard'];assert g['returncode']==124 and g['reason']=='nonzero_exit' and g['last_stage']['phase']=='correctness-only' and g['last_stage']['limit_ms']==30000
cleanup=load(D/'cleanup-verification.json');assert cleanup.get('alive_observed_processes',[])==[]
j={'status':'MATERIALIZATION_SCOPE_AND_GUARDS_VERIFIED','result_sha256':sha(D/'result.json'),'package_sha256':sha(P),'source_tree_sha256':current['source_tree_sha256'],'source_entries':len(current['files']),'code_delta':code_delta,'manifest_checks':checks,'references_verified':len(refs),'new_starts':3,'new_charge_seconds':charged,'cumulative_charge_seconds':1251,'unstarted_rows':35,'guard_reasons':guard_counts,'actual_stop':'correctness-only30s timer exit124','old_stop_text_retained':w['reason'],'new_browser_workers_by_reviewer':0}
(C/'identity-check.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps(j))
