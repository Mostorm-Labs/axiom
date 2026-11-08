"""Independent file-only review. Never starts browser, oracle, build, or sampling."""
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
RAW = ROOT / 'out/ur0b/resume-r4/execution/raw-runs'
STATE = ROOT / '.aegis/results/G5-UR0b/resume-r4'

def read(p):
    return json.loads(Path(p).read_text())

def write(name, value):
    (OUT / name).write_text(json.dumps(value, indent=2, ensure_ascii=False) + '\n')

def ref(p):
    p = Path(p)
    return {'path': str(p.relative_to(ROOT)), 'sha256': hashlib.sha256(p.read_bytes()).hexdigest()}

identity = read(OUT / 'identity-check.json')
producer = read(STATE / 'result.json')
assert identity['result_sha256'] == ref(STATE / 'result.json')['sha256']
assert identity['new_charge_seconds'] == 118 and identity['cumulative_charge_seconds'] == 1251
assert identity['code_delta'] == [
    'verification/runners/g5/ur0/remaining_runner.py',
    'verification/tests/test_g5_ur0_remaining_runner.py']
node = (OUT / 'node.log').read_text()
python = (OUT / 'python.log').read_text()
boundary = (OUT / 'boundary.log').read_text()
assert '# pass 100' in node and '# fail 0' in node
assert 'Ran 56 tests' in python and python.rstrip().endswith('OK')
assert 'passed (120 contract files)' in boundary

rt = []
for hz in (120, 240):
    name = f'realtime-{hz}'
    archived = read(RAW / name / 'compiled-summary.json')
    independent = read(OUT / name / 'compiled-summary.json')
    assert independent['analysis']['mode'] == 'OFFLINE_REVERIFICATION'
    assert archived['analysis']['mode'] == 'WORKER_COMPILE'
    assert archived['analysis']['input_source_tree_sha256'] == producer['source_tree_sha256']
    assert {k: v for k, v in archived.items() if k != 'analysis'} == {
        k: v for k, v in independent.items() if k != 'analysis'}
    for filename in ('canonical-chain.json', 'metrics.json', 'compiler-provenance.json', 'external-metrics.json'):
        assert (RAW / name / filename).read_bytes() == (OUT / name / filename).read_bytes(), filename
    c = archived['canonical_chain']
    assert [c[k] for k in ('accepted_raw_count', 'qualified_initialization_count', 'qualified_committed_count', 'unresolved_accepted_count')] == [2, 1, 1, 0]
    assert archived['schema_status'] == 'VALID' and archived['clock'] == 'QUALIFIED'
    assert archived['critical_loss'] == 0 and not archived['identity_errors']
    rt.append({'run_id': name, 'accepted': True, 'original_worker_summary': ref(RAW / name / 'compiled-summary.json'),
               'independent_offline_summary': ref(OUT / name / 'compiled-summary.json'),
               'all_non_analysis_summary_fields_equal': True, 'metrics_and_chain_bytes_equal': True,
               'accepted_raw': 2, 'initialization': 1, 'committed': 1, 'unresolved': 0})
write('recompilation-check.json', {'status': 'INDEPENDENT_OFFLINE_RECOMPILATION_EQUAL', 'new_browser_workers': 0, 'rows': rt})

samples_path = ROOT / 'out/ur0b/resume-r4/execution/guards/correctness-lower_z_translucent_drag.rss.jsonl'
samples = [json.loads(line) for line in samples_path.read_text().splitlines() if line.strip()]
start = int(samples[0]['monotonic_ns'])
timeline = []
last = object()
for sample in samples:
    if sample['stage'] != last:
        timeline.append({'elapsed_from_first_guard_sample_seconds': (int(sample['monotonic_ns']) - start) / 1e9,
                         'last_reported_stage': sample['stage']})
        last = sample['stage']
guard_path = ROOT / 'out/ur0b/resume-r4/execution/guards/correctness-lower_z_translucent_drag.worker.json'
guard = read(guard_path)
assert guard['returncode'] == 124 and guard['last_stage'] == {'stage': 'phase-timeout', 'phase': 'correctness-only', 'limit_ms': 30000}
assert guard['cleanup_complete'] and not guard['remaining_owned_pids']
assert guard['peak_process_family_rss_kib'] < guard['rss_limit_kib']
timing = {'status': 'FILE_ONLY_COARSE_STAGE_RECONSTRUCTION', 'source': ref(samples_path), 'guard_ref': ref(guard_path),
          'timeline': timeline, 'sample_count': len(samples),
          'known': 'Frozen correctness-only phase deadline fired; both OFF comparisons completed before termination; no completed ON pair.',
          'unresolved': 'correctnessNext/render/settle, RGBA transfer, close, reload and ON initialization do not have individual brackets here. No unique pending await or rendering-vs-transport root cause can be assigned.',
          'not_a_product_latency_measurement': True, 'deadline_unchanged': True}
write('timing-diagnostic.json', timing)

pixels = read(OUT / 'pixel-diagnostic.json')
assert pixels['browser_runs'] == 0 and pixels['reference_oracle_rerun'] is False
p0, p1 = pixels['rows']
assert p0['original']['interior_mismatched_pixels'] == 2463
assert p0['diagnostic_only_comparison']['interior_mismatched_pixels'] == 2441
assert p1['original']['interior_mismatched_pixels'] == 16112
assert p1['old_mismatches_in_added_public_band'] == 16112
assert p1['diagnostic_only_comparison']['interior_mismatched_pixels'] == 0

findings = [
    {'id': 'HUD-1', 'classification': 'IMPLEMENTATION_DEFECT', 'owner': 'correctness harness',
     'finding': 'The second lower-z test step supplies empty hud_bounds after selecting hand, although archived public readSelection reports one selected object and Runtime intentionally renders its read-only selection outline.',
     'witness': 'All 16112 originally mismatching pixels fall in the same existing publicHudBand constructed solely from archived public selection bounds; the diagnostic comparison has zero remaining mismatches.',
     'effect': 'The original OFF1 FAILED result cannot identify a canonical body rendering error. It is retained unchanged, and the diagnostic does not retroactively qualify the worker.',
     'repair_direction': 'Use actual public selection state for the already-defined HUD exclusion; test retained-selection hand mode and empty selection. No product change or broader pixel tolerance is justified.'},
    {'id': 'LIVE-HUD-2', 'classification': 'VERIFICATION_DESIGN_DEFECT', 'earliest_untrusted_stage': 'P20',
     'finding': 'The live drag candidate can contain snap guides and handle chrome, while its reference is a post-Up canonical snapshot and HUD eligibility is restricted to public selection bounds. The driver assumes a 24/12 move but the committed transform is 24/10.505767822265625.',
     'witness': 'Adding archived actual public bounds leaves 2441 OFF0 mismatches; common colors agree with known snap-guide/chrome colors, but color agreement does not prove all remaining pixels are HUD. Current SDK readSelection does not publish the live snap-guide geometry.',
     'residual_proof_gap': 'A trustworthy live-transient-vs-canonical reference applicability and HUD separation rule for the already-frozen lower-z correctness case. Existing AA masks exclude stroke edges, not independent interaction decorations.',
     'required_resolution': 'Verification owner must define the applicable transform/time and candidate-independent HUD handling or a bounded fixture/reference correction that still detects lower-z transparency/order defects. Do not mask by observed differences, accept unknown mismatches, expose Runtime internals, or modify product snapping to make the test green.',
     'blocking_basis': 'Frozen paired lower-z correctness obligation is already incomplete; this review adds no new product Gate or threshold.'},
    {'id': 'TIME-3', 'classification': 'FROZEN_REQUIREMENT_FAILURE', 'owning_root_cause': 'UNRESOLVED',
     'finding': 'The combined OFF/ON correctness-only phase exhausted the existing 30000ms timer. OFF0 and OFF1 each have long unseparated render/readback/transport brackets; no ON comparison completed.',
     'required_resolution': 'Preserve the frozen deadline and guard result. Locate individual awaits in a separately authorized harness repair/diagnostic scope before any deadline change. This result is not evidence that the product hangs during close or that GPU coverage fails.'},
    {'id': 'STOP-4', 'classification': 'NON_BLOCKING_HARDENING', 'owner': 'remaining_runner diagnostics',
     'finding': 'require_fresh checks missing summary/mode before guard exit, so the retained stop text says fresh WORKER_COMPILE is required while the primary cause is phase exit124.',
     'effect': 'Raw guard and final return preserve the actual cause. Admission still fails closed; no unsafe result was accepted.',
     'repair_direction': 'Optional successor diagnostic precedence fix; never rewrite the old stop or use this to authorize a retry.'}
]
write('findings.json', {'findings': findings, 'new_product_requirements': [], 'original_pixel_verdicts_preserved': True})

handoff = {'schema': 'aegis-ownership-handoff-v1', 'from_primary': 'aegis-gate-review', 'to_primary': 'aegis',
           'required_owner': 'aegis-verification', 'earliest_untrusted_stage': 'P20',
           'reason': 'LIVE-HUD-2: establish same-subject live pixel reference and independent handling of selection/snap/handle HUD for the existing lower-z correctness case before authorizing harness repair or fresh sampling.',
           'package_ref': producer['package_ref'], 'subject_result_ref': ref(STATE / 'result.json'),
           'findings_ref': ref(OUT / 'findings.json'),
           'accepted_work': ['exact historical prerequisite reuse', 'fresh realtime120/240 raw evidence and independent recompilation',
                             'two new runner/test paths within scope', 'bounded stop and cumulative ledger', 'all immutable historical and new archives'],
           'decisions_needed': ['Independent live reference/HUD applicability without candidate-derived exclusions or weaker document-body checks',
                                'Bounded transport/stage repair scope preserving exact byte identity, original correctness deadlines, guard ownership and no primary-timing readback',
                                'Reuse of accepted realtime qualifications and historical prerequisites after any explicitly scoped harness change'],
           'execution_authorized_by_this_handoff': False, 'new_sampling_authorized': False, 'old_window_status': 'CLOSED',
           'next_action': 'Resolve the earliest verification gap, then freeze the minimal implementation-owned harness repair package. No automatic rerun or product optimization from this return.'}
write('ownership-handoff.json', handoff)

result = {'schema_version': 1, 'task_id': 'G5-UR0b-control-review-resume-r4', 'primary_owner': 'aegis-gate-review',
          'surface': 'CONTROL_REVIEW', 'reviewer_thread': '01a1b6f3-e69a-7fe2-8d9b-0222e8741f03',
          'producer_thread': '01a0dacd-61e3-7ac3-a2cb-ab7532f1b28c', 'independent_producer_reviewer': True,
          'status': 'BLOCKED_EVIDENCE', 'partial_result': 'BOUNDED_RETURN_AND_REALTIME_QUALIFICATIONS_ACCEPTED',
          'review_subject': ref(STATE / 'result.json'), 'package_ref': producer['package_ref'],
          'repository': {'origin': 'git@github.com:Mostorm-Labs/axiom.git', 'worktree': str(ROOT),
                         'branch': 'codex/geometry-navigation-integration', 'revision': producer['result_revision'],
                         'dirty': True, 'source_tree_sha256': producer['source_tree_sha256']},
          'gate_review': {
              'frozen_requirements': {'passed': ['Only permitted two new Python source files', 'No product/SDK/compiler/collector/oracle threshold change',
                                                'Old windows remain closed; no hidden retry/rebuild/commit/push', 'Exclusive bounded execution and cleanup'],
                                      'failed': ['First correctness-only worker phase deadline; paired lower-z evidence incomplete']},
              'frozen_evidence': {'passed': ['All 217 new and 1783 historical archived files plus required bindings independently hashed',
                                            'Independent raw recompilation of realtime120/240: metrics/chain identical',
                                            'Historical init/committed/overhead/60Hz accepted reuse preserved'],
                                  'failed': ['Six paired correctness cases not completed (one partial, five unstarted)', '30 main matrix rows not started']},
              'new_findings': {'blocking_high_impact': [], 'non_blocking': ['STOP-4 diagnostic precedence; no Gate expansion']},
              'verdict': 'BLOCKED_EVIDENCE'},
          'root_classification': {'HUD-1': 'IMPLEMENTATION_DEFECT', 'LIVE-HUD-2': 'VERIFICATION_DESIGN_DEFECT',
                                  'TIME-3': 'FROZEN_REQUIREMENT_FAILURE_WITH_UNRESOLVED_ROOT', 'STOP-4': 'NON_BLOCKING_HARDENING'},
          'remaining_proof_gap': 'Actual OFF/ON correctness and main performance evidence; partial OFF contamination cannot prove product RED, and prior native tests do not replace these required real browser checks.',
          'regressions_independently_run': {'node_pass': 100, 'python_pass': 56, 'boundary_contracts': 120,
                                           'new_browser_workers': 0, 'new_native_oracle_invocations': 0, 'new_builds': 0},
          'ledger': producer['accounting'], 'old_window_status': 'CLOSED', 'new_sampling_authorized': False,
          'diagnostic_pixel_math_is_gate_pass': False, 'product_or_performance_PASS': False, 'official_P34_PASS': False,
          'refs': {name: ref(OUT / name) for name in ['identity-check.json', 'recompilation-check.json', 'pixel-diagnostic.json',
                                                     'timing-diagnostic.json', 'findings.json', 'ownership-handoff.json',
                                                     'node.log', 'python.log', 'boundary.log']}}
write('result.json', result)
print(json.dumps({'status': result['status'], 'result_ref': ref(OUT / 'result.json'), 'handoff_ref': ref(OUT / 'ownership-handoff.json')}, ensure_ascii=False))
