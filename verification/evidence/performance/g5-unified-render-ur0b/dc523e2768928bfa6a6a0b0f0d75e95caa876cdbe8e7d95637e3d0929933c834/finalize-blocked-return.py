"""Seal the retained failed qualification as a reviewable terminal return."""
import collections,hashlib,importlib.util,json,re,shutil,subprocess
from pathlib import Path
ROOT=Path(__file__).resolve().parents[3];RESULT=Path(__file__).resolve().parent;OUT=ROOT/'out/ur0b'
def read(p):return json.loads(Path(p).read_text())
def write(p,x):Path(p).write_text(json.dumps(x,indent=2)+'\n')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
spec=importlib.util.spec_from_file_location('ur0_identity',ROOT/'verification/runners/g5/ur0/source_identity.py')
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
source=module.identity(ROOT);tree=source['source_tree_sha256']
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree
assert BUNDLE.exists() and not (BUNDLE/'manifest.json').exists(),'refuse resealing an existing manifest'
native=read(OUT/'native-regression-evidence.json');assert all(r['exit_code']==0 for r in native)
native_log=(OUT/'ctest-required-evidence.log').read_text();native_count=int(re.search(r'0 tests failed out of (\d+)',native_log)[1])
node_log=(OUT/'node-required-evidence.log').read_text();node_count=int(re.search(r'# pass (\d+)',node_log)[1]);assert '# fail 0\n' in node_log
python_log=(OUT/'python-required-evidence.log').read_text();python_count=int(re.search(r'Ran (\d+) tests',python_log)[1]);assert python_log.rstrip().endswith('OK')
boundary_log=(OUT/'boundary-evidence.log').read_text();contracts=int(re.search(r'passed \((\d+) contract files\)',boundary_log)[1])
regression={'status':'QUALIFIED_AFFECTED_REGRESSIONS_ONLY','native':{'tests':native_count,'runs':native,'log':'preflight-and-regression/ctest-required-evidence.log'},
    'node':{'passed':node_count,'failed':0,'log':'preflight-and-regression/node-required-evidence.log'},
    'python':{'passed':python_count,'log':'preflight-and-regression/python-required-evidence.log'},
    'boundaries':{'contract_files':contracts,'log':'preflight-and-regression/boundary-evidence.log'},
    'full_ctest':'NOT_RUN_CORROBORATIVE_ONLY; historical failures not repaired or relabeled',
    'historical_authority_manifest':{'path':'.aegis/results/G5-UR0a/output-sha256.json','sha256':sha(ROOT/'.aegis/results/G5-UR0a/output-sha256.json')}}
write(BUNDLE/'regression.json',regression)
for name in ['collector-tests','gpu-retirement-tests']:
    write(BUNDLE/(name+'.json'),{'status':'QUALIFIED_NATIVE_MODELS','native_tests':['G5Ur0Observation','G5Ur0ObservationIdentity'],
        'log':'preflight-and-regression/ctest-required-evidence.log','source_tests':['runtime/foundation/tests/g5_observation_test.cpp','runtime/render/tests/g5_observation_identity_test.cpp'],
        'assertions':'bounded critical priority/overflow, callback-before-submit, duplicate completion, abandon/loss, reset/pinning, borrowed buffer, identity precision',
        'not_physical_device_or_full_instrumentation_qualification':True})
write(BUNDLE/'driver-tests.json',{'status':'QUALIFIED_PURE_RULES_ONLY','node_log':'preflight-and-regression/node-required-evidence.log',
    'coverage':['original plan catch-up and delayed handler','confirmed/terminal preservation','literal and legacy corpus','16-bracket clock errors','nearest-rank/censoring','foreign/stale identities','GPU callback ordering','release minified export resolution','missing common data rejected'],
    'realtime_browser_calibrations':'MISSING_FROZEN_STOP'})
plan=read(BUNDLE/'cohort/overhead-plan.json');ledger=read(BUNDLE/'matrix-ledger.json');separate=read(BUNDLE/'separate-cohort-ledger.json')
assert len(plan)==12 and len(ledger)==30 and len(separate)==9
assert all(r['status']=='MISSING' for r in ledger+separate)
guards=[r['outcome']['guard'] for r in plan if 'outcome' in r]
assert all(g['cleanup_complete'] and not g['remaining_owned_pids'] for g in guards)
failure=next(r for r in plan if r['status']=='RSS_LIMIT');guard=failure['outcome']['guard']
assert guard['peak_process_family_rss_kib']>guard['rss_limit_kib']==3145728
write(BUNDLE/'worker-accounting.json',{'overhead':{'planned':len(plan),'outcomes':dict(collections.Counter(r['status'] for r in plan)),
     'executed_elapsed_seconds':sum(g['elapsed_s'] for g in guards),'max_frozen_seconds':1080},
     'main':{'planned':len(ledger),'outcomes':dict(collections.Counter(r['status'] for r in ledger)),'executed_elapsed_seconds':0,'max_frozen_seconds':5400},
     'separate_realtime_correctness':{'planned':len(separate),'outcomes':dict(collections.Counter(r['status'] for r in separate)),'executed_elapsed_seconds':0,'max_frozen_seconds':810},
     'optional_diagnostic_rerun':{'used':False,'count':0},
     'mechanism_preflights':'separate retained development/qualification checks; not a replacement or retry of the formal cohort',
     'all_formal_owned_workers_cleaned':True})
obligations=[
 ('UR0b-PRIVATE','QUALIFIED','normal-qualification-boundary.json'),
 ('UR0b-CLOCK-INPUT','PARTIAL','clock-calibration.json'),
 ('UR0b-COMMIT-SCENE','PARTIAL','commit-scene-reconciliation.json'),
 ('UR0b-ROUTES-TARGETS','PARTIAL_UNQUALIFIED','routes-observed.json'),
 ('UR0b-WORK-COUNTERS','INCOMPLETE_STATISTICAL_LOSS','work-counter-capabilities.json'),
 ('UR0b-GPU-QUEUE','PARTIAL_UNQUALIFIED','gpu-convergence.json'),
 ('UR0b-FIXTURE','QUALIFIED_INITIAL_ONLY','fixture-qualification.json'),
 ('UR0b-PIXEL-SEMANTICS','MISSING','pixel-oracle.json'),
 ('UR0b-OVERHEAD','FAILED_FROZEN_RSS_STOP','overhead-qualification.json'),
 ('UR0b-MATRIX','NOT_ADMITTED','matrix-ledger.json'),
 ('UR0b-REGRESSION','QUALIFIED_AFFECTED','regression.json')]
write(BUNDLE/'obligation-evaluation.json',{'official_gate_pass':False,'terminal':'FROZEN_VERIFICATION_FAILURE',
    'items':[{'id':i,'status':s,'evidence_ref':e} for i,s,e in obligations]})
open_boundaries=[
 'No six-pair overhead qualification: only the first pair completed; second ON hit the frozen RSS guard.',
 'Main 30 rows, 3 real-time runs and 6 combined Web correctness workers are MISSING, not PASS.',
 'First formal ON retained 31,860 ordinary statistical drops; critical loss was zero. Whole-run work/lease totals are not valid.',
 'Full UR0a metrics-schema compilation, logical demand/job normalization and real-time consumer return shape remain incomplete and have no adopted primary results.',
 'Legacy dynamic reproduction and 32/96 qualifier artifact were not executed/built after the qualification stop.',
 'Actual native rejection E1-0 exists in ON; browser INPUT_REJECTED exists in OFF and ON. Observation task does not repair product admission/History/presentation.',
 'Commit observation is a returned-record upper bound; no exact internal commit timestamp or physical display/pure GPU duration is claimed.',
 'Actual GPU target does not establish absence of internal Skia software coverage.',
 'No G5/P34/performance PASS, no UR0c/UR1 optimization, no commit/push or cross-thread dispatch.'
]
readme=f'''# UR0b blocked qualification return

Status: **FROZEN_VERIFICATION_FAILURE**, returned for independent control review.
This is not completed UR0b, product performance PASS, or P34 PASS.

Source HEAD: `{source['revision']}` with uncommitted observation/runner changes.
Full source-tree SHA256: `{tree}`. HEAD alone does not contain these changes.
`source/tracked.patch`, `source/untracked-source.tar.gz`, and the full identity map
bind the implementation; manifest binds the exact SDK, raw runs and evidence.

The frozen 6 AB/BA pairs stopped at `{failure['run_id']}` in measurement.
Owned-process RSS sum reached {guard['peak_process_family_rss_kib']} KiB against
{guard['rss_limit_kib']} KiB. All owned processes were cleaned. This conservative
sum may count shared pages repeatedly; it is not unique physical memory, GPU
memory, the 16MiB diagnostic allocation cap, or the 64/192MiB Render budget.
No specific external noise cause was established, so no diagnostic rerun was used.

One full pair preserves identical binary/document/contact terminal and error-code
results. It cannot qualify the six-pair gate. ON ordinary statistics were dropped;
the missing work totals are not replaced with zeros. Native E1-0 was rejected.
First-pair timing numbers are retained as unqualified diagnostic observations only.
All 30 main rows and 9 separate real-time/correctness workers are MISSING.

Fresh affected regression: {native_count} native, {node_count} Node,
{python_count} Python; {contracts} boundary contract files. Normal consumer install
has no new private exports/headers/archives. Qualification-only binary adds exactly
3 private symbols and refuses consumer install. Root JS and declarations agree.
The normal installed root consumer built the 16-object fixture successfully.

Earlier development diagnostics are preserved. The preliminary FMA replay mismatch
was an oracle floating-point contraction difference and was disproved by a private
`ffp-contract=off` replay of the existing algorithm. The minified export-label audit
was corrected using real generated JS-to-WASM bindings. Neither changes authority.

Next: independent review of the RSS stop, statistical loss and remaining proof
obligations; repair the observation/runner or explicitly amend the owning sampling
contract before another authorized cohort. Do not raise budgets or retry until a
passing sample appears. No optimization or publication is authorized here.
'''
(BUNDLE/'README.md').write_text(readme)
shutil.copy2(__file__,BUNDLE/'finalize-blocked-return.py')
files={}
for p in sorted(BUNDLE.rglob('*')):
    if p.is_file():files[str(p.relative_to(BUNDLE))]={'sha256':sha(p),'bytes':p.stat().st_size}
manifest={'schema':'g5-ur0b-terminal-manifest-v1','source_revision':source['revision'],'source_tree_sha256':tree,
          'source_dirty':True,'status':'FROZEN_VERIFICATION_FAILURE','files':files}
write(BUNDLE/'manifest.json',manifest)
result={
 'schema_version':1,'task_id':'G5-UR0b','status':'FROZEN_VERIFICATION_FAILURE','return_surface':'CONTROL_REVIEW',
 'reason':'Frozen process-family RSS guard stopped the same-binary overhead qualification; primary matrix not admitted.',
 'package_sha256':sha(ROOT/'.aegis/packages/G5-UR0b/package.json'),
 'activation_ref':{'path':'.aegis/results/G5-UR0b/control-activation.json','sha256':sha(RESULT/'control-activation.json')},
 'starting_revision':'f66592001012444cd99db968961014497c0f120f','result_revision':source['revision'],
 'source_tree_sha256':tree,'worktree_dirty_paths':read(BUNDLE/'source/git-status.json')['porcelain'].splitlines(),
 'materialized_ref':{'kind':'shared-controlled-worktree-content-addressed-bundle','path':str(BUNDLE),'manifest_path':str(BUNDLE/'manifest.json'),'manifest_sha256':sha(BUNDLE/'manifest.json')},
 'output_sha256':sha(BUNDLE/'manifest.json'),'validation_results':{'obligations':'obligation-evaluation.json','regression':'regression.json','manifest_validation':'.aegis/results/G5-UR0b/validation.json'},
 'matrix_ledger_ref':str(BUNDLE/'matrix-ledger.json'),
 'correctness_status':'PARTIAL_COMPONENT_REGRESSIONS_PASS; combined Web correctness MISSING',
 'evidence_status':'INCOMPLETE_UNQUALIFIED_PRIMARY; exact failed/partial records preserved',
 'performance_status':'NOT_EVALUATED; primary timing not adopted',
 'scope_audit':read(BUNDLE/'source-scope-audit.json'),
 'open_boundaries':open_boundaries,
 'next_phase_prerequisites':['Independent classification of frozen RSS stop and observation loss','Finish and qualify remaining UR0b proof mechanisms under an accepted package','New authorized cohort; do not substitute retry-success for the preserved failure'],
 'official_gate_pass':False,'continue_execution':False,'stop_after':'UR0b','committed':False,'pushed':False}
write(RESULT/'result.json',result)
(RESULT/'README.md').write_text(f'''UR0b 已执行至冻结终止条件，状态 **FROZEN_VERIFICATION_FAILURE**。

第二组 ON 在测量阶段触发 3GiB 进程族 RSS 保护；其余资格配对与主矩阵停止。
详细结果：[result.json](result.json)。全部源码、产物与失败证据保存在：
`{BUNDLE}`。

该结果不是 UR0b 完成或 P34 PASS，未提交或推送。需独立控制面评审 RSS 保护、普通丢样及未闭合验证后再续跑。
''')
validation={'status':'VERIFIED_FAILED_RUN_MATERIALIZATION','bundle_file_count':len(files),
    'manifest_sha256':sha(BUNDLE/'manifest.json'),'result_sha256':sha(RESULT/'result.json'),
    'checks':{'source_tree_stable':module.identity(ROOT)['source_tree_sha256']==tree,
      'all_listed_files_match':all(sha(BUNDLE/name)==v['sha256'] for name,v in files.items()),
      'all_matrix_ids_accounted':len(ledger)==30,'all_formal_workers_cleaned':all(g['cleanup_complete'] for g in guards),
      'not_primary_qualified':result['performance_status'].startswith('NOT_EVALUATED'),'no_gate_pass':not result['official_gate_pass']}}
assert all(validation['checks'].values())
write(RESULT/'validation.json',validation)
write(RESULT/'output-sha256.json',{'result.json':sha(RESULT/'result.json'),'validation.json':sha(RESULT/'validation.json'),
    'README.md':sha(RESULT/'README.md'),'bundle-manifest':{'path':str(BUNDLE/'manifest.json'),'sha256':sha(BUNDLE/'manifest.json')}})
print(json.dumps({'status':result['status'],'bundle':str(BUNDLE),'manifest_sha256':sha(BUNDLE/'manifest.json'),
    'source_tree_sha256':tree,'regression_counts':{'native':native_count,'node':node_count,'python':python_count,'boundaries':contracts},'validation':validation['status']}))
