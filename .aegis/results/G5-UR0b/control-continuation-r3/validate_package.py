"""Read-only package, ancestry and accounting validation; no worker execution."""
from pathlib import Path
import hashlib,json,math,subprocess
R=Path(__file__).resolve().parents[4]
D=R/'.aegis/packages/G5-UR0b-resume-r3'
C=Path(__file__).resolve().parent
checks=[]
def digest(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def check_ref(ref,base=R):
 p=(base/ref['path']).resolve();assert p.is_relative_to(R)
 assert digest(p)==ref['sha256'],str(p)
 checks.append(str(p.relative_to(R)))
 return json.loads(p.read_text()) if p.suffix=='.json' else p.read_text()
pkg=json.loads((D/'package.json').read_text())
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=R,text=True).strip()=='git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=R,text=True).strip()=='codex/geometry-navigation-integration'
for stamp in [pkg['task_anchor'],pkg['resume_cursor']]:subprocess.run(['git','merge-base','--is-ancestor',stamp['revision'],'HEAD'],cwd=R,check=True)
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref']:
 assert (D/pkg[key]['path']).resolve().parent==D
 check_ref(pkg[key],D)
a=json.loads((D/'authority.lock.json').read_text())
for key,value in a.items():
 if key.endswith('_ref'):check_ref(value)
for value in a['preserved_returns']:check_ref(value)
parent=check_ref(a['parent_package_ref'])
for key in ['authority_lock_ref','verification_lock_ref','execution_contract_ref','implementation_context_ref','evidence_contract_ref','scope_contract_ref']:check_ref(parent[key],R/'.aegis/packages/G5-UR0b')
budget=check_ref(a['historical_budget_ref']);charged=0
for row in budget['rows']:
 if 'guard_ref' in row:
  guard=check_ref(row['guard_ref']);assert guard['cleanup_complete'];assert math.ceil(guard['elapsed_s'])==row['charged_seconds'];assert guard['elapsed_s']==row['elapsed_seconds']
 else:
  ledger=check_ref(row['ledger_ref']);old=next(x for x in ledger if x['run_id']=='qualification-p2-A')
  assert old['status']=='CONTROL_STOP' and row['charged_seconds']==old['limit_seconds']==90
 charged+=row['charged_seconds']
assert charged==597 and len(budget['rows'])==15
contract=json.loads((D/'execution-contract.json').read_text())['EXECUTION_CLOSURE_CONTRACT'];limits=contract['execution_limits']
assert sum(p['count'] for p in limits['phases'])==53
assert sum(p['count']*p['per_worker_seconds'] for p in limits['phases'])==7470
assert charged+7470==8067<=limits['all_workers_cap_seconds']==8370
assert limits['unallocated_seconds']==303 and not limits['unallocated_is_retry_authorization'] and not limits['extra_browser_preflights']
assert limits['concurrency']==1 and limits['process_family_rss_bytes']==3221225472
assert limits['render_budget_pairs_mib']==[[32,96],[64,192]]
assert limits['phases'][1]['order']==['AB','BA','BA','AB','AB','BA']
assert pkg['continue_until_terminal_state'] and not pkg['authorization']['commit_push']
result={'status':'PACKAGE_BINDINGS_AND_BUDGET_VALID','package_sha256':digest(D/'package.json'),'hash_bound_references_checked':len(checks),'references':checks,'historical_charged_seconds':charged,'new_workers_max':53,'cumulative_seconds_max':8067,'original_cap_seconds':8370,'new_workers_started_by_control':0,'production_changes_by_control':False}
(C/'validation.json').write_text(json.dumps(result,indent=2)+'\n')
print(json.dumps({k:v for k,v in result.items() if k!='references'}))
