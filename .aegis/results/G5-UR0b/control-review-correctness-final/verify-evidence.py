"""Independent archive, scope, population and compiler-reuse audit; file-only."""
import hashlib
import json
import subprocess
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
read = lambda p: json.loads(Path(p).read_text())
def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as f:
        for block in iter(lambda:f.read(1048576), b''): h.update(block)
    return h.hexdigest()
def ref(p):
    p = Path(p)
    return {'path': str(p.relative_to(ROOT)), 'sha256': sha(p)}
def put(n,v): (OUT/n).write_text(json.dumps(v,indent=2)+'\n')

sys.path.insert(0, str(ROOT/'verification/runners/g5/ur0'))
import source_identity
import repair_runner
subjects = []
for name, expected in [('correctness-repair-r2','926ab4219a0aa1181432281fb3f26a871ea6850757ace6f67459de4572d870e4'),
                       ('correctness-test-reverify-r1','7afd8befa682549490c9c473350836bc68e1c61da6c5c6cca52a17af497ef553')]:
    p = ROOT/f'.aegis/results/G5-UR0b/{name}/result.json'
    assert sha(p) == expected
    r = read(p); b = Path(r['materialized_ref']['path']); m = read(b/'manifest.json')
    assert sha(b/'manifest.json') == r['materialized_ref']['manifest_sha256']
    for n, item in m['files'].items(): assert sha(b/n) == item['sha256'], n
    for k,v in r.items():
        if k.endswith('_ref') and isinstance(v,dict) and 'path' in v and 'sha256' in v:
            assert sha(ROOT/v['path']) == v['sha256'], k
    subjects.append({'task':name,'result_ref':ref(p),'manifest_ref':ref(b/'manifest.json'),'payload_files':len(m['files'])})

current = source_identity.identity(ROOT)
assert current['revision'] == 'df214581c1c80c2d546939271f3db2dc0e77a38b'
assert current['source_tree_sha256'] == '118f3029bab049983f03333e4ca1ff90361adad1ffe6f743715dcfc32ecccf5b'
assert subprocess.check_output(['git','remote','get-url','origin'],cwd=ROOT,text=True).strip() == 'git@github.com:Mostorm-Labs/axiom.git'
assert subprocess.check_output(['git','branch','--show-current'],cwd=ROOT,text=True).strip() == 'codex/geometry-navigation-integration'
r2 = read(ROOT/'.aegis/results/G5-UR0b/correctness-repair-r2/result.json')
r2_bundle = Path(r2['materialized_ref']['path'])
r2_source = read(r2_bundle/'source/full-source-identity.json')
delta = sorted(n for n in current['files'].keys()|r2_source['files'].keys() if current['files'].get(n)!=r2_source['files'].get(n))
assert all(n == 'verification/tests/test_g5_ur0_remaining_runner.py' or n.startswith('.aegis/packages/G5-UR0b-correctness-test-reverify-r1/') for n in delta),delta
preserved = read(ROOT/'.aegis/results/G5-UR0b/correctness-test-reverify-r1/preservation.json')
compiler = preserved['unchanged_compiler_file_sha256']
for n, digest in compiler.items(): assert sha(ROOT/'verification/runners/g5/ur0'/n) == digest,n
profile = json.loads(subprocess.check_output(['node','--input-type=module','-e',
    "import {repairedInitializationProfile as p} from './verification/runners/g5/ur0/initialization_basis.mjs';console.log(JSON.stringify(p))"],cwd=ROOT,text=True))
assert profile == repair_runner.REPAIRED_INITIALIZATION_PROFILE
for n, digest in profile.items(): assert sha(ROOT/n) == digest,n

old_inputs = read(ROOT/'.aegis/verification/G5-UR0b-initialization-v1/evidence-inputs.json')['inputs']
expected = {x['run_id']:x for x in old_inputs}
assert len(expected) == 15
for hz in (120,240):
    d = ROOT/f'verification/evidence/performance/g5-unified-render-ur0b/d33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a/resume-r4/execution-and-checks/execution/raw-runs/realtime-{hz}'
    expected[f'realtime-{hz}'] = {'run_id':f'realtime-{hz}', 'input_directory':str(d.relative_to(ROOT)),
                                 'immutable_inputs':[ref(d/n) for n in ['raw-trace.jsonl','worker-result.json','run-inputs.json']],
                                 'expected_v2':{'accepted_raw_count':2,'qualified_initialization_count':1,'qualified_committed_count':1,'metric_receipt_count':1,'unresolved_accepted_count':0}}
derived = {x['run_id']:x for x in read(ROOT/'.aegis/results/G5-UR0b/correctness-repair-r2/reverification.json')['rows']}
assert len(derived)==17 and set(derived)==set(expected)
rows=[]
for name, frozen in expected.items():
    row = derived[name]; directory = r2_bundle/'recompiled-recordings'/name
    summary = read(directory/'compiled-summary.json')
    baseline_path = ROOT/row['baseline_summary_ref']['path']; baseline = read(baseline_path)
    assert sha(baseline_path)==row['baseline_summary_ref']['sha256']
    for n in ['canonical-chain.json','decoded.json','metrics.json','external-metrics.json']:
        assert sha(directory/n)==sha(baseline_path.parent/n),(name,n)
    assert {k:v for k,v in summary.items() if k!='analysis'} == {k:v for k,v in baseline.items() if k!='analysis'},name
    a = summary['analysis']; old_a = baseline['analysis']
    assert a['mode']=='OFFLINE_REVERIFICATION'
    for k in ['input_source_tree_sha256','raw_trace_sha256','worker_result_sha256','run_inputs_sha256','verification_spec_ref','completion_contract_ref']:
        assert a[k]==old_a[k],(name,k)
    assert a['compiler_files']==compiler,(name,'compiler')
    for item,key in zip(frozen['immutable_inputs'], ['raw_trace_sha256','worker_result_sha256','run_inputs_sha256']):
        assert sha(ROOT/item['path'])==item['sha256']==a[key],(name,key)
    chain = summary['canonical_chain']; counts = frozen['expected_v2']
    for k,v in counts.items():
        if k not in ['status','metrics_schema_status']: assert chain[k]==v,(name,k)
    repair_runner.require_canonical_chain(summary,require_positive=chain['qualified_committed_count']>0)
    assert summary['schema_status']=='VALID' and summary['critical_loss']==0
    rows.append({'run_id':name,'recorded_input_refs':frozen['immutable_inputs'], 'summary_ref':ref(directory/'compiled-summary.json'),
                 'proof_ref':ref(directory/'canonical-chain.json'),'recording_source_tree_sha256':a['input_source_tree_sha256'],
                 'mode':a['mode'],'counts':{k:chain[k] for k in ['accepted_raw_count','qualified_initialization_count','qualified_committed_count','metric_receipt_count','unresolved_accepted_count']},
                 'accepted_artifacts':summary['artifact_identity'], 'baseline_summary_ref':row['baseline_summary_ref']})
put('recorded-reuse-check.json',{'status':'ALL17_INDEPENDENTLY_RECONCILED','method':'Independent frozen17 population traversal, immutable raw hashes, all derived proof/metrics/decoded bytes vs accepted baselines, full non-provenance summary equality and current canonical reader checks.',
    'compiler_file_sha256':compiler,'repaired_profile':profile,'rows':rows,'new_browser_workers':0,'new_native_qualification_probes':0})
put('identity-check.json',{'status':'SUBJECTS_MATERIALIZATION_SCOPE_AND_PROFILES_VERIFIED','subjects':subjects,'repository_revision':current['revision'],
    'source_tree_sha256':current['source_tree_sha256'],'dirty':True,'source_delta_from_r2':delta,'compiler_file_sha256':compiler,
    'repaired_profile':profile,'old_r2_failure_preserved':True,'one_test_only_repair_confirmed':True})
print(json.dumps({'status':'REVIEW_FILE_AUDIT_PASS','payload_files':sum(x['payload_files'] for x in subjects),'recordings':len(rows),'source_delta':delta}))
