"""Compile the frozen-stop return without adopting unqualified timings.

This evidence compiler is outside the measured source-tree hash by its published
self-output exclusion. It never changes sources, budgets, raw runs or old evidence.
"""
import collections,hashlib,importlib.util,json,re,shutil,subprocess,tarfile
from pathlib import Path

ROOT=Path(__file__).resolve().parents[3]
OUT=ROOT/'out/ur0b'
RESULT=ROOT/'.aegis/results/G5-UR0b'
spec=importlib.util.spec_from_file_location('ur0_identity',ROOT/'verification/runners/g5/ur0/source_identity.py')
identity=importlib.util.module_from_spec(spec);spec.loader.exec_module(identity)
def read(p):return json.loads(Path(p).read_text())
def write(p,x):
    p=Path(p);p.parent.mkdir(parents=True,exist_ok=True);p.write_text(json.dumps(x,indent=2)+'\n')
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def git(*args):return subprocess.check_output(['git','-C',str(ROOT),*args])
def copy(src,dst):
    dst=Path(dst);dst.parent.mkdir(parents=True,exist_ok=True);shutil.copy2(src,dst)

source=identity.identity(ROOT)
tree=source['source_tree_sha256']
BUNDLE=ROOT/'verification/evidence/performance/g5-unified-render-ur0b'/tree
assert not BUNDLE.exists(),'do not replace an existing terminal bundle'
package=read(ROOT/'.aegis/packages/G5-UR0b/package.json')
scope=read(ROOT/'.aegis/packages/G5-UR0b/scope-contract.json')
cohort=OUT/'frozen-cohort'
assert read(cohort/'execution-stop.json')['status']=='FROZEN_VERIFICATION_FAILURE'
assert git('branch','--show-current').decode().strip()=='codex/geometry-navigation-integration'
assert git('rev-parse','HEAD').decode().strip()=='f66592001012444cd99db968961014497c0f120f'
for key in ['control_activation_ref','control_review_ref','scope_correction_ref']:
    ref=package[key];assert sha(ROOT/ref['path'])==ref['sha256']

tracked=git('diff','--name-only','-z','HEAD').decode().split('\0');tracked=[p for p in tracked if p]
untracked=[p for p in git('ls-files','--others','--exclude-standard','-z').decode().split('\0') if p]
violations=[p for p in tracked if p not in scope['allowed_existing_source_files']]
violations += [p for p in untracked if p not in scope['allowed_new_source_files'] and not any(p.startswith(root) for root in scope['allowed_new_roots'])]
assert not violations,violations
for prefix in scope['read_only_root_sdk']+scope['preserved_roots']+scope['preserved_exact_files']:
    assert not any(p==prefix or p.startswith(prefix.rstrip('/')+'/') for p in tracked),prefix
assert not subprocess.run(['git','diff','--check'],cwd=ROOT,capture_output=True).returncode

write(BUNDLE/'source/full-source-identity.json',source)
(BUNDLE/'source/tracked.patch').write_bytes(git('diff','--binary','HEAD'))
source_new=[p for p in untracked if not p.startswith(identity.EXCLUDED)]
with tarfile.open(BUNDLE/'source/untracked-source.tar.gz','w:gz') as archive:
    for p in source_new:archive.add(ROOT/p,arcname=p,recursive=False)
audit={'status':'QUALIFIED_SCOPE_ONLY','tracked_changed':tracked,'untracked_source':source_new,'violations':violations,
       'read_only_and_frozen_inputs_unchanged':True,'source_edit_class':'qualifier-gated observation and private tests/runners only',
       'not_a_product_or_gate_pass':True,'head_does_not_contain_dirty_changes':True}
write(BUNDLE/'source-scope-audit.json',audit)
write(BUNDLE/'source/git-status.json',{'head':source['revision'],'branch':'codex/geometry-navigation-integration',
      'porcelain':git('status','--porcelain=v1').decode(),'patch_basis':'HEAD, binary diff; untracked archive applied after patch',
      'hash_definition':'full tracked and nonignored files, executable/symlink/deletion identity; only documented self-outputs excluded'})

shutil.copytree(cohort,BUNDLE/'cohort')
for p in sorted(OUT.iterdir()):
    if p.is_file():copy(p,BUNDLE/'preflight-and-regression'/p.name)
for name in ['fixture-mechanism-data','on-mechanism-data','on-compact-mechanism-data','on-priority-mechanism-data','normal-root-smoke-data']:
    if (OUT/name).exists():shutil.copytree(OUT/name,BUNDLE/'mechanism-diagnostics'/name)
copy(__file__,BUNDLE/'compile-blocked-evidence.py')
for root in [ROOT/'.aegis/packages/G5-UR0b',RESULT]:
    for p in root.iterdir():
        if p.is_file() and p.name not in ['result.json','output-sha256.json','README.md']:
            copy(p,BUNDLE/'authority'/root.name/p.name)

boundary=read(OUT/'normal-qualification-boundary.json');assert boundary['status']=='QUALIFIED'
write(BUNDLE/'normal-qualification-boundary.json',boundary)
write(BUNDLE/'normal-artifact-manifest.json',boundary['normal_install_manifest'])
for variant,sdk in [('normal',OUT/'normal-install/js'),('qualification',OUT/'build-qualified-64-192/sdk/web')]:
    manifest=[]
    with tarfile.open(BUNDLE/f'{variant}-web-artifacts.tar.gz','w:gz') as archive:
        for f in boundary['normal_install_manifest']:
            relative=f['path'].removeprefix('js/');p=sdk/relative
            assert p.is_file(),p
            manifest.append({'path':relative,'sha256':sha(p),'size':p.stat().st_size})
            archive.add(p,arcname=relative,recursive=False)
    write(BUNDLE/f'{variant}-artifact-manifest.json',manifest)
for variant in ['qualified','normal']:
    build=OUT/f'build-{variant}-64-192'
    for name in ['CMakeCache.txt','compile_commands.json','ur0b-build-request.json','configure.log','build.log']:
        if (build/name).exists():copy(build/name,BUNDLE/'build'/variant/name)

oracle=read(OUT/'fixture-oracle-qualified.stdout.log');assert oracle['status']=='QUALIFIED'
write(BUNDLE/'exact-query-oracle.json',oracle)
write(BUNDLE/'fixture-qualification.json',{'status':'QUALIFIED_INITIAL_FIXTURE_ONLY','oracle':'exact-query-oracle.json',
      'snapshot':'mechanism-diagnostics/fixture-mechanism-data/initial.axsnap',
      'snapshot_sha256':sha(OUT/'fixture-mechanism-data/initial.axsnap'),
      'rule':'accepted Semantic decode and independent replay/exact geometry; no adaptive query',
      'float_reproduction':'private oracle compiles existing algorithm with ffp-contract=off to match scalar WASM; production algorithm unchanged',
      'earlier_diagnostic':'preflight-and-regression/fixture-oracle.json retained; its preliminary TASK_PACKAGE_DEFECT classification was disproved by the FMA-controlled replay, not an authority amendment'})
write(BUNDLE/'snapshot-inputs.json',{'snapshots':[{'path':str(p.relative_to(BUNDLE)),'sha256':sha(p),'bytes':p.stat().st_size} for p in sorted(BUNDLE.rglob('*.axsnap'))]})

on=cohort/'raw-runs/overhead-p1-B'
run=read(on/'worker-result.json');decoded=read(on/'decoded.json')
assert tree==run['source_before']['source_tree_sha256']==run['source_after']['source_tree_sha256']
raw=[json.loads(line) for line in (on/'raw-trace.jsonl').read_text().splitlines()]
copy(on/'raw-trace.jsonl',BUNDLE/'raw-trace.jsonl')
(BUNDLE/'stage-trace.jsonl').write_text(''.join(json.dumps(r)+'\n' for r in raw if r['kind'] in [2,3] or r['kind']==10 and r['tag'] in [17,22]))
write(BUNDLE/'commit-scene-reconciliation.json',{'status':'DIAGNOSTIC_ONLY_NOT_PRIMARY_QUALIFIED',
      'commits':decoded['commits'],'scenes':decoded['scenes'],'frames':decoded['frames'],
      'qualified_completions':decoded['qualified_completions'],'errors':decoded['errors'],'missing':decoded['missing'],
      'commit_timestamp_limit':'returned commit-record observation is an upper bound; may follow inside-apply Scene publication; no invented negative/zero commit-to-Scene duration'})
by_event={e.get('diagnostic_event'):e['event_id'] for e in run['measurement']['planned']}
rejections=[{'raw':r,'event_id':by_event.get(r['event'])} for r in raw if r['kind']==16 and r['tag']==28 and r['values'][0]=='0']
write(BUNDLE/'input-rejections.json',{'native_result_provenance':rejections,'browser_errors_unmodified':run['measurement']['errors'],
      'browser_error_without_event_id':'not assigned to an event by temporal guessing'})
with (BUNDLE/'input-plan-and-delivery.jsonl').open('w') as stream:
    for row in read(cohort/'overhead-plan.json'):
        p=cohort/'raw-runs'/row['run_id'];m=p/'measurement.json'
        if m.exists():
            for e in read(m)['planned']:stream.write(json.dumps({'run_id':row['run_id'],**e})+'\n')
        elif (p/'prepared-plan.json').exists():
            for e in read(p/'prepared-plan.json')['events']:stream.write(json.dumps({'run_id':row['run_id'],'actual_delivery_status':'MISSING_KILLED_WORKER',**e})+'\n')
write(BUNDLE/'clock-calibration.json',{'status':'PARTIAL_NOT_PRIMARY_QUALIFIED','runs':[
      {'run_id':r['run_id'],'calibration':read(cohort/'raw-runs'/r['run_id']/'worker-result.json')['calibration']}
      for r in read(cohort/'overhead-plan.json') if (cohort/'raw-runs'/r['run_id']/'worker-result.json').exists()]})
candidates=read(ROOT/'.aegis/packages/G5-UR0b/execution-contract.json')['EXECUTION_CLOSURE_CONTRACT']['performance_execution']['candidate_route_set']
observed=collections.Counter(l['route'] for l in decoded['leaves'])
write(BUNDLE/'routes-observed.json',{'origin':'CURRENT_BRANCH','status':'PARTIAL_DIAGNOSTIC_NOT_COVERED',
      'candidate_routes':[{ 'route':r,'observed_leaves':observed.get(r,0),'qualification':'OBSERVED_UNQUALIFIED' if r in observed else 'MISSING'} for r in candidates],
      'unknown_skia_software_coverage':'UNKNOWN; actual GPU target is not proof of all-GPU coverage',
      'no_full_route_or_43_route_coverage_claim':True})
write(BUNDLE/'target-leaf-reconciliation.json',{'status':'DIAGNOSTIC_ONLY','leaves':decoded['leaves'],'decisions':decoded['decisions'],'errors':decoded['errors'],
      'target_label_limit':'equivalent actual target uses within a job, not persistent SkSurface identity; address may be reused'})
write(BUNDLE/'work-counter-capabilities.json',{'status':'INCOMPLETE','loss':run['trace']['loss'],'capabilities':[
      {'counter':'retained individual work/copy/reference/lease values','capability':'EXACT','scope':'only at each retained actual observation; never total work under statistical loss'},
      {'counter':'full measurement work totals / mask and body cache totals / lease classes','capability':'MISSING','reason':'ordinary statistical records dropped'},
      {'counter':'ImmutableValue equality header size','capability':'EXACT','scope':'sizeof(T) object/header only'},
      {'counter':'deep payload scanned bytes, hash bytes, implicit allocator copies','capability':'UNSUPPORTED','reason':'no extra scan introduced solely to count'},
      {'counter':'Skia internal software coverage on GPU surface','capability':'UNSUPPORTED'}]})
write(BUNDLE/'work-costs.json',{'status':'INCOMPLETE_DIAGNOSTIC_ONLY','counters':decoded['counters'],'spans':decoded['spans'],'whole_run_totals':None,'reason':'statistical loss and cohort not overhead-qualified'})
(BUNDLE/'gpu-queue-timeline.jsonl').write_text(''.join(json.dumps(g)+'\n' for g in decoded['gpu']))
write(BUNDLE/'gpu-convergence.json',{'status':'DIAGNOSTIC_ONLY','observed_groups':len(decoded['gpu']),
      'group_status_counts':dict(collections.Counter(g['status'] for g in decoded['gpu'])),
      'collector_pending_gpu_at_measurement_drain':run['trace']['pending_gpu'],'cleanup_trace':run.get('cleanup_trace'),
      'physical_display_or_pure_gpu_duration':'UNSUPPORTED','budget_class_convergence':'MISSING_STATISTICAL_LEASE_LOSS',
      'navigation_canonical_receipt_created':False})

ledger=read(cohort/'matrix-ledger.json');assert len(ledger)==30 and all(r['status']=='MISSING' for r in ledger)
copy(cohort/'matrix-ledger.json',BUNDLE/'matrix-ledger.json')
copy(cohort/'overhead-pairs.json',BUNDLE/'overhead-pairs.json')
copy(cohort/'overhead-qualification.json',BUNDLE/'overhead-qualification.json')
missing='UR0b-OVERHEAD stopped by frozen process-family RSS guard; primary sampling not admitted'
for name in ['pixel-oracle','semantic-state','receipt-oracle']:
    write(BUNDLE/(name+'.json'),{'status':'MISSING','reason':missing,'existing_component_regressions':'preflight-and-regression/ctest-required-evidence.log','not_substitute_for_six_correctness_workers':True})
write(BUNDLE/'separate-cohort-ledger.json',[
    *[{'run_id':'REALTIME-'+str(hz),'cadence_hz':hz,'status':'MISSING','reason':missing} for hz in [60,120,240]],
    *[{'run_id':'CORRECTNESS-'+str(i+1),'case':name,'status':'MISSING','reason':missing} for i,name in enumerate(['lower-z translucent drag','permanent+live mask','transparent/color background','warm-miss-pressure','DPR switch','feedback-canonical'])]])
write(BUNDLE/'baseline-summary.json',{'status':'NOT_ADMITTED','performance_status':'NOT_EVALUATED','primary_timing_adopted':False,
      'matrix_outcomes':dict(collections.Counter(r['status'] for r in ledger)),
      'overhead_outcomes':dict(collections.Counter(r['status'] for r in read(cohort/'overhead-plan.json'))),
      'cause':read(cohort/'execution-stop.json'),'diagnostic_reruns':0,
      'rss_interpretation':'sum of owned process RSS, shared pages may count repeatedly; not unique physical bytes, GPU memory, or Renderer cache budget'})
print(json.dumps({'bundle':str(BUNDLE),'source_tree_sha256':tree,'status':'COMPILED_PENDING_REGRESSION_MANIFEST'}))
