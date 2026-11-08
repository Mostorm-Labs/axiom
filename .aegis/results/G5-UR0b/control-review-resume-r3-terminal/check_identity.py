from pathlib import Path
import json,hashlib,math,sys,subprocess
R=Path(__file__).resolve().parents[4];D=Path(__file__).resolve().parent
sys.path.insert(0,str(R/'verification/runners/g5/ur0'));import source_identity
sha=lambda p:hashlib.sha256(p.read_bytes()).hexdigest()
read=lambda p:json.loads(p.read_text())
result_path=R/'.aegis/results/G5-UR0b/resume-r3/result.json';result=read(result_path)
assert sha(result_path)=='81f44aca6da61e86ae11eedca3645ae2f13c18881493f078d3f6a83befbc07d0'
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip()=='codex/geometry-navigation-integration'
for anchor in ['a2d99bfd9e2dc89b159cc7a2ba583c3f53d6129e',result['starting_revision']]:subprocess.run(['git','merge-base','--is-ancestor',anchor,'HEAD'],cwd=R,check=True)
P=R/result['package_ref']['path'];assert sha(P)==result['package_ref']['sha256'];pkg=read(P)
for k in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 ref=pkg[k];assert sha(P.parent/ref['path'])==ref['sha256']
bundle=Path(result['materialized_ref']['path']);assert bundle.is_relative_to(R);assert sha(bundle/'manifest.json')==result['materialized_ref']['manifest_sha256']
def verify_manifest(p):
 j=read(p);bad=[]
 for name,entry in j['files'].items():
  f=p.parent/name
  if not f.is_file() or sha(f)!=entry['sha256'] or 'bytes' in entry and f.stat().st_size!=entry['bytes']:bad.append(name)
 return {'manifest':str(p.relative_to(R)),'files':len(j['files']),'mismatches':bad}
checks=[verify_manifest(bundle/'manifest.json')]
for h in result['history_preservation']:
 assert sha(R/h['result_ref']['path'])==h['result_ref']['sha256'];assert sha(R/h['manifest_ref']['path'])==h['manifest_ref']['sha256'];checks.append(verify_manifest(R/h['manifest_ref']['path']))
assert all(not c['mismatches'] for c in checks)
proof_path=R/result['mechanism_proof_ref']['path'];assert sha(proof_path)==result['mechanism_proof_ref']['sha256'];proof=read(proof_path)
proof_bad=[entry['path'] for entry in proof['files'] if sha(R/entry['path'])!=entry['sha256']];assert not proof_bad
current=source_identity.identity(R);assert current['source_tree_sha256']==result['source_tree_sha256']==proof['source_tree_sha256']
assert current['files']==read(bundle/'source/full-source-identity.json')['files']
window=read(R/'.aegis/results/G5-UR0b/resume-r3/window.json');assert window['status']=='CLOSED';rows=window['rows'];started=[r for r in rows if 'outcome' in r]
assert len(started)==15
charged=0;guard_keys=[]
for row in started:
 g=row['outcome']['guard'];assert g['reason']=='completed' and g['returncode']==0 and g['cleanup_complete']
 guardref=row['outcome'].get('guard_record');guard_keys.append(guardref)
 if guardref:assert read(Path(guardref) if Path(guardref).is_absolute() else R/guardref)==g
 cost=math.ceil(g['elapsed_s']);assert cost==row['charged_seconds'];charged+=cost
assert charged==536 and window['charged_seconds']==597+536==1133
assert sum(r['status']=='MISSING' for r in rows)==38
cleanup=read(R/'.aegis/results/G5-UR0b/resume-r3/cleanup-verification.json');assert cleanup['alive_observed_processes']==[]
j={'status':'IDENTITIES_AND_IMMUTABILITY_VERIFIED','result_sha256':sha(result_path),'source_tree_sha256':current['source_tree_sha256'],'current_source_entries':len(current['files']),'bundle_checks':checks,'mechanism_refs_verified':len(proof['files']),'new_workers':len(started),'new_charged_seconds':charged,'cumulative_charged_seconds':597+charged,'missing_rows':38,'old_window_closed':True,'recorded_owned_pids_cleaned':cleanup['observed_pid_count'],'new_browser_workers_by_reviewer':0}
(D/'identity-check.json').write_text(json.dumps(j,indent=2)+'\n');print(json.dumps(j))
