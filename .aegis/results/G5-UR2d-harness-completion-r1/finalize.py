"""Finish the existing UR2d evidence graph without mutating historic evidence."""
from pathlib import Path
import difflib
import importlib.util
import json
import shutil
import subprocess
import urllib.request
import xml.etree.ElementTree as ET

from aggregate import ROOT, OUT, PARENT, ORIGINAL, CONTROL, WINDOW, sha, read, write, ref, require


def main():
    package = ROOT / '.aegis/packages/G5-UR2d-harness-completion-r1/package.json'
    require(sha(package) == 'd55bf3e4cecdba6f85afa6a627fe470b0e4220b49cf9774acff64520e86685de', 'package identity')
    for key, binding in read(package).items():
        if key.endswith('_ref') and isinstance(binding, dict) and 'sha256' in binding:
            require(sha(package.parent / binding['path']) == binding['sha256'], 'package binding ' + key)
    changed = ['verification/runners/g5/ur2d_selection_up.mjs', 'verification/tests/g5_ur2d_selection_up.test.mjs']
    added = ['verification/runners/g5/ur2d_pair_completion_runner.py', 'verification/tests/test_g5_ur2d_pair_completion.py']
    files, delta = {}, []
    for name in changed + added:
        current = ROOT / name
        post = OUT / 'new-source-postimages' / name
        post.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(current, post)
        pre = OUT / 'preimages' / name
        files[name] = {'current': ref(current), 'postimage': ref(post), 'preimage': ref(pre) if pre.exists() else None}
        delta.extend(difflib.unified_diff(pre.read_text().splitlines(keepends=True) if pre.exists() else [],
                                        current.read_text().splitlines(keepends=True),
                                        fromfile='a/' + name if pre.exists() else '/dev/null', tofile='b/' + name))
    (OUT / 'source-delta.patch').write_text(''.join(delta))
    write('source-manifest.json', {'schema': 'g5-ur2d-harness-only-source-v1', 'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'worktree': str(ROOT), 'branch': 'codex/geometry-navigation-integration', 'files': files,
        'delta': ref(OUT / 'source-delta.patch'), 'frozen_product_source': ref(PARENT / 'source-manifest.json'),
        'production_and_sdk_changes_in_this_package': False})
    tests = []
    commands = [
        ('offline-node-final.log', ['node', '--test', 'verification/tests/g5_ur2d_selection_up.test.mjs']),
        ('offline-python-final.log', ['python3', '-m', 'unittest', 'discover', '-s', 'verification/tests', '-p', 'test_g5_ur2d_pair_completion.py', '-v']),
        ('diff-check-final.log', ['git', 'diff', '--check'])]
    for filename, command in commands:
        cp = subprocess.run(command, cwd=ROOT, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, text=True)
        (OUT / filename).write_text(cp.stdout)
        require(cp.returncode == 0, 'offline verification failure ' + filename)
        tests.append({'command': command, 'exit_code': cp.returncode, 'log': ref(OUT / filename)})
    junit = ET.parse(PARENT / 'final-keeper-junit.xml').getroot()
    cases = {c.attrib['name']: {'name': c.attrib['name'], 'time': c.attrib.get('time'),
                              'failed': bool(c.findall('failure') or c.findall('error'))} for c in junit.iter('testcase')}
    require(len(cases) == 78 and not any(v['failed'] for v in cases.values()), 'frozen native keepers')
    recovery_names = ['canvas_rf01_scene_atomicity', 'canvas_g2_a6_atomic_publication', 'canvas_g2_a6_recovery',
        'G5UR2CAtomicRecovery', 'G4EditorHistory', 'FC01EditorHistoryInverseFamilies', 'G5GpuResourceRecovery',
        'G5UR2BExactPressure', 'G5UR2CLeaseAccounting', 'G5UR2CHistoryAdmission', 'G5UR2CHistoryProgressSafety',
        'G5TransientDisplayRecovery', 'G5UR2AReadmissionAcquire', 'G5UR2AReadmissionPresent',
        'G5UR2ASceneRecovery', 'G5UR2AHistory', 'G5UR2BExactInputRuntime', 'G5UR2CContinuousInput', 'G5UR2DUpRuntime']
    write('failure-recovery.json', {'status': 'FROZEN_KEEPERS_REUSED_EXACT_SOURCE', 'junit': ref(PARENT / 'final-keeper-junit.xml'),
        'native_log': ref(PARENT / 'final-native-regression.log'), 'cases': [cases[n] for n in recovery_names],
        'source_binding': ref(PARENT / 'source-manifest.json'), 'source_stability': ref(OUT / 'source-artifact-checks.json'),
        'no_native_rerun': True})
    native = read(PARENT / 'B-final-paired-native.json')
    require(len(native['candidate']) == 12 and all(v['canonical_mask_bytes_equal'] and v['history_exactly_once'] and v['noop_cancel_undo_redo'] for v in native['candidate']), 'actual Up mechanism proofs')
    write('commit-history-scene.json', {'status': 'CANONICAL_HISTORY_SCENE_KEEPERS_AND_PUBLIC_RECEIPTS_QUALIFIED',
        'native_actual_up': ref(PARENT / 'B-final-paired-native.json'), 'native_candidate_cases': len(native['candidate']),
        'full_snapshot_roundtrips': ref(OUT / 'snapshot-review.json'), 'public_receipts': ref(OUT / 'web-functional.json'),
        'atomicity_and_recovery': ref(OUT / 'failure-recovery.json'),
        'ordering': 'prepareRecord -> publication reservation -> OperationEngine -> commitPrepared/observer -> Scene unchanged',
        'per_copy_payload': 'Canonical masks are still constructed per copy. Only complete equivalent exact decisions are shared.'})
    write('regressions.json', {'status': 'SCOPED_REGRESSIONS_QUALIFIED_WITH_HISTORICAL_FINDINGS', 'new_offline_tests': tests,
        'frozen_native': {'junit': ref(PARENT / 'final-keeper-junit.xml'), 'cases': len(cases), 'failures': 0},
        'old634': ref(PARENT / 'original634-final-comparison.json'), 'finite6': ref(PARENT / 'finite-six-final.log'),
        'selection55': ref(PARENT / 'selection-parity.json'), 'independent172': ref(CONTROL / 'independent-A-parity.json'),
        'old_focused_node': ref(PARENT / 'final-node-keepers.log'), 'old_focused_python': ref(PARENT / 'final-python-keepers.log'),
        'boundary120': ref(PARENT / 'checkpoint-boundary.log'),
        'prior_broad_8_failures': 'Historical baseline retained, not rerun, fixed, or waived.', 'no_new_broad_native_run': True})
    olddemo = read(PARENT / 'demo.json')
    http = []
    for old in olddemo['http_bytes']:
        url = 'http://127.0.0.1:4316/' + old['file']
        with urllib.request.urlopen(url, timeout=15) as response:
            data = response.read()
            import hashlib
            digest = hashlib.sha256(data).hexdigest()
            require(response.status == 200 and digest == old['sha256'], 'ordinary demo changed ' + url)
            http.append({'file': old['file'], 'status': response.status, 'sha256': digest, 'matches_frozen_sdk': True})
    write('demo-check.json', {'url': olddemo['url'], 'pid_preserved': olddemo['pid'], 'http_bytes': http,
        'ordinary': True, 'browser_opened': False, 'restarted': False, 'documents_or_user_tabs_touched': False,
        'live_logs_excluded': True})
    ledger = read(OUT / 'sampling-ledger.json')
    guard = read(OUT / 'local-web/U2D1-PAIR-COMPLETE-R1.worker.json')
    require(ledger['charged_seconds'] == 269 and guard['cleanup_complete'] and not guard['remaining_owned_pids'], 'ledger or cleanup')
    write('cleanup.json', {'status': 'CLEAN_WITH_USER_DEMOS_PRESERVED', 'new_browser_starts': len(ledger['attempts']),
        'historical_starts': ledger['prior_starts'], 'total_starts': ledger['prior_starts'] + len(ledger['attempts']),
        'prior_charged_seconds': ledger['prior_charged_seconds'], 'new_charged_seconds': ledger['attempts'][0]['charged_seconds'],
        'cumulative_charged_seconds': ledger['charged_seconds'], 'original_cap_seconds': ledger['cap_seconds'],
        'remaining_owned_pids': guard['remaining_owned_pids'], 'cleanup_complete': guard['cleanup_complete'],
        'guard': ref(OUT / 'local-web/U2D1-PAIR-COMPLETE-R1.worker.json'), 'ledger': ref(OUT / 'sampling-ledger.json'),
        'old_cleanups': [ref(ORIGINAL / 'cleanup.json'), ref(PARENT / 'cleanup.json')], 'demo': ref(OUT / 'demo-check.json'),
        'preserved_user_pages': [4316, 4315, 4298, 4297], 'no_new_server': True, 'no_commit_push': True,
        'old_failed_windows_and_evidence_unchanged': True})
    aliases = {
        'PackageBindingPreflight.json': [OUT / 'PackageBindingPreflight.json', ORIGINAL / 'PackageBindingPreflight.json'],
        'EvidenceContractPreflight.json': [OUT / 'EvidenceContractPreflight.json', ORIGINAL / 'EvidenceContractPreflight.json'],
        'implementation-design.json': [ORIGINAL / 'implementation-design.json'],
        'source-before.json': [ORIGINAL / 'source-before.json'], 'baseline-oracles.json': [ORIGINAL / 'baseline-oracles.json'],
        'old-cost-RED.json': [ORIGINAL / 'old-cost-RED.json'],
        'milestone-A.json': [PARENT / 'milestone-A.json'], 'milestone-B.json': [PARENT / 'milestone-B.json', OUT / 'web-functional.json'],
        'selection-parity.json': [PARENT / 'selection-parity.json', CONTROL / 'independent-A-parity.json'],
        'selection-cost-and-parity.json': [PARENT / 'selection-cost-and-parity-final.json'],
        'input-lifecycle.json': [PARENT / 'input-lifecycle-A.json', OUT / 'web-functional.json'],
        'up-phase-baseline.json': [PARENT / 'up-phase-baseline.json'], 'up-phase-candidate.json': [PARENT / 'up-phase-candidate-final.json'],
        'up-structural-red-green.json': [PARENT / 'up-structural-red-green.json', PARENT / 'B-final-paired-native.json'],
        'exact-oracle-parity.json': [PARENT / 'original634-final-comparison.json', PARENT / 'finite-six-final.log'],
        'commit-history-scene.json': [OUT / 'commit-history-scene.json'], 'failure-recovery.json': [OUT / 'failure-recovery.json'],
        'lease-and-basis.json': [PARENT / 'lease-and-basis.json', CONTROL / 'memo-result.log', OUT / 'failure-recovery.json'],
        'regressions.json': [OUT / 'regressions.json'], 'native-regression.log': [PARENT / 'final-native-regression.log', PARENT / 'final-keeper-junit.xml'],
        'paired-native-costs.json': [PARENT / 'paired-native-costs-A.json', PARENT / 'B-final-paired-native.json'],
        'web-functional.json': [OUT / 'web-functional.json'], 'web-timing.json': [OUT / 'web-timing.json'],
        'performance-classification.json': [OUT / 'performance-classification.json'],
        'normal-artifacts.json': [PARENT / 'final-normal-artifacts.json', PARENT / 'normal-test-hooks-absent-final.json', OUT / 'demo-check.json'],
        'source-manifest.json': [OUT / 'source-manifest.json', PARENT / 'source-manifest.json'],
        'source-delta.patch': [OUT / 'source-delta.patch', PARENT / 'source-delta.patch'],
        'new-source-postimages/': [OUT / 'source-manifest.json', PARENT / 'source-manifest.json'],
        'cleanup.json': [OUT / 'cleanup.json'],
    }
    obligations = {
        'D-A1': ['old-cost-RED.json', 'selection-cost-and-parity.json', 'source-delta.patch'],
        'D-A2': ['selection-parity.json', 'input-lifecycle.json', 'native-regression.log', 'web-functional.json'],
        'D-B1': ['up-phase-baseline.json', 'up-phase-candidate.json', 'up-structural-red-green.json', 'exact-oracle-parity.json'],
        'D-B2': ['commit-history-scene.json', 'failure-recovery.json', 'web-functional.json'],
        'D-PERF': ['paired-native-costs.json', 'web-timing.json', 'performance-classification.json'],
        'D-REG': ['regressions.json', 'lease-and-basis.json', 'normal-artifacts.json', 'source-manifest.json', 'cleanup.json'],
    }
    write('obligation-index.json', {'schema': 'g5-ur2d-original-obligation-index-repair-v1',
        'status': 'ALL_ORIGINAL_OBLIGATIONS_HASH_RESOLVABLE', 'original_contract': ref(ROOT / '.aegis/packages/G5-UR2d-selection-up-r1/evidence-contract.json'),
        'aliases': {name: [ref(p) for p in paths] for name, paths in aliases.items()}, 'obligations': obligations,
        'milestone_B_historical_note': 'Original milestone said missing pairs; immutable history. New three-pair files close that gap without rewriting it.',
        'result.json': {'path': str((OUT / 'result.json').relative_to(ROOT)), 'identity_owner': 'Final callback SHA; omitted here to avoid self-hash cycle.'},
        'new_source_postimages_rule': 'Both manifests list exact per-file postimage refs. No floating directory alias is the proof.'})
    status = 'READY_FOR_CONTROL_REVIEW_WITH_PERFORMANCE_FINDINGS'
    write('closure.json', {'status': status, 'package': ref(package), 'required_evidence_present': read(package.parent / 'evidence-contract.json')['required'],
        'production_frozen': True, 'three_pairs_actual': True, 'old_failed_result_preserved': ref(PARENT / 'result.json'),
        'remaining': ['Independent control review', 'Observed content-feedback latency and mixed ordinary-Web eraser timing are performance findings.',
                      'Fullscreen and general object interaction feedback are separate authorized follow-up scopes.'],
        'official_gate_p34_performance_pass': False})
    files = [p for p in sorted(OUT.rglob('*')) if p.is_file() and p.name not in ('artifact-manifest.json', 'result.json') and '__pycache__' not in p.parts and p.name != 'demo-server.log']
    write('artifact-manifest.json', {'schema': 'g5-ur2d-harness-completion-artifacts-v1', 'files': [ref(p) for p in files],
        'excludes': ['artifact-manifest.json (self)', 'result.json (callback hash owner)', '__pycache__/', 'all live server logs'],
        'old_evidence': ref(OUT / 'preserved-evidence-before.json'), 'old_raws_remain_in_original_roots': True})
    required = read(package.parent / 'evidence-contract.json')['required']
    for name in required:
        if name != 'result.json':
            require((OUT / name).is_file(), 'missing obligation file ' + name)
    write('result.json', {'schema': 'g5-ur2d-harness-completion-result-v1', 'status': status, 'package': ref(package),
        'materialized_ref': {'provider': 'shared-controlled-worktree', 'repository': 'Mostorm-Labs/axiom', 'root': str(ROOT), 'path': str(OUT.relative_to(ROOT))},
        'result_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'source': ref(OUT / 'source-manifest.json'), 'delta': ref(OUT / 'source-delta.patch'), 'manifest': ref(OUT / 'artifact-manifest.json'),
        'product_source': ref(PARENT / 'source-manifest.json'), 'normal_SDK': ref(PARENT / 'final-normal-artifacts.json'),
        'driver': ref(ROOT / 'verification/runners/g5/ur2d_selection_up.mjs'), 'runner': ref(ROOT / 'verification/runners/g5/ur2d_pair_completion_runner.py'),
        'evidence_input_refs': [ref(OUT / n) for n in required if n not in ('result.json', 'artifact-manifest.json', 'source-manifest.json')],
        'three_actual_pairs': [0, 1, 2], 'timing_scope': 'Ordinary Web paired dispatch observations; content-write gaps explicitly separate from RAF and GPU completion.',
        'ledger': ref(OUT / 'sampling-ledger.json'), 'cleanup': ref(OUT / 'cleanup.json'),
        'normalDemoURL': olddemo['url'], 'prior_BLOCKED_EVIDENCE_immutable': ref(PARENT / 'result.json'),
        'new_production_changes': False, 'new_sdk_build': False, 'commit_push': False,
        'official_gate_p34_performance_pass': False, 'next_action_owner': 'Control review; no extra browser allowance remains in this package.'})
    for entry in read(OUT / 'artifact-manifest.json')['files']:
        require(sha(ROOT / entry['path']) == entry['sha256'], 'final artifact hash mismatch')
    print(json.dumps({name: ref(OUT / name) for name in ('result.json', 'source-manifest.json', 'source-delta.patch', 'artifact-manifest.json', 'sampling-ledger.json')}, indent=2))


if __name__ == '__main__':
    main()
