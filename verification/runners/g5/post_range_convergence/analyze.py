"""Offline audit of the new focused post-Up pair; never starts a browser."""
from pathlib import Path
import hashlib
import importlib.util
import json

ROOT = Path(__file__).resolve().parents[4]
TASK = 'G5-post-range-convergence-r1'
OUT = ROOT / 'out' / TASK
RESULT = ROOT / '.aegis/results' / TASK
PACKAGE = json.loads((ROOT / '.aegis/packages' / TASK / 'package.json').read_text())
PRIOR = Path(PACKAGE['accepted_prior_results']['heavy_scene_report']['path']).parents[2]
spec = importlib.util.spec_from_file_location('retained_analysis', PRIOR / 'verification/runners/g5/heavy_scene_attribution_analyze.py')
analysis = importlib.util.module_from_spec(spec)
spec.loader.exec_module(analysis)


def ref(path):
    return {'path': str(path), 'sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    initial = analysis.wire.objects((ROOT / '.aegis/results/G5-web-up-attribution-and-witness-r1/local-web/G5WA1-BASELINE-TAIL/growth6/initial.axsnap').read_bytes())
    rows = []
    buffers = {}
    for variant in ['B', 'C']:
        base = OUT / 'web' / ('postup-batch-' + variant) / 'primary'
        row, index, records = analysis.read_row(base)
        semantic = analysis.check_primary(base, row, initial)
        ups = [e for e in row['events'] if e.get('phase') == 'up']
        up = ups[0]
        next_down = row['events'][row['events'].index(up) + 1]
        lo, hi = up['at'], row['stages']['AB-settled']['at']
        clo, chi = row['stages']['C-Up']['at'], row['stages']['followup-erase']['at']
        facts = {}
        for phase in ['AB-post-Up', 'C-followup']:
            values = [r['facts'] for r in records if r['type'] == 'pending-facts' and r['phase'] == phase]
            facts[phase] = values[-1] if values else {}
        counters = ['targetPrepareNs', 'sceneStampPrepareNs', 'historyPrepareNs', 'publicationPrepareNs', 'publishNs']
        costs = {k: float(facts['AB-post-Up'][k]) / 1e6 if k in facts['AB-post-Up'] else None for k in counters}
        c_costs = {k: (float(facts['C-followup'][k]) - float(facts['AB-post-Up'][k])) / 1e6 if k in facts['C-followup'] and k in facts['AB-post-Up'] else None for k in counters}
        phases = [analysis.phase_account(records, lo, hi, variant + '-AB'), analysis.phase_account(records, clo, chi, variant + '-C')]
        workers = [analysis.worker_summary(records, lo, hi), analysis.worker_summary(records, clo, chi)]
        for worker in workers:
            for key, source in [('compute_sum_ms', 'compute_ms'), ('yield_sum_ms', 'yield_ms')]:
                if any(r.get(source) is None for r in worker['rows']):
                    worker[key] = None
        for phase in phases:
            if not phase['native_owner_attempts']:
                phase['native_owner_inclusive_attempt_ms'] = None
            if not phase['native_nested_canonical_attempts']:
                phase['native_nested_canonical_attempt_ms'] = None
        for label, worker, end in [('AB-after-last-worker', workers[0], hi), ('C-after-last-worker', workers[1], chi)]:
            if worker['last_terminal_js_ms'] is not None:
                part = analysis.phase_account(records, worker['last_terminal_js_ms'], end, variant + '-' + label)
                if not part['native_owner_attempts']:
                    part['native_owner_inclusive_attempt_ms'] = None
                if not part['native_nested_canonical_attempts']:
                    part['native_nested_canonical_attempt_ms'] = None
                phases.append(part)
        assert not row['errors'] and row['closed'] and row['observation']['criticalLost'] == 0
        assert all(r['result']['ok'] for r in records if r['type'] == 'bridge-terminal' and r.get('command') == 'pointer')
        rows.append({'variant': variant, 'raw': ref(base / 'raw.json'), 'records': ref(base / 'records.jsonl.gz'),
                     'inputCadence': row['inputCadence'], 'A_input_wall_ms': row['A_down_to_up_ms'],
                     'A_Up_handler_ms': up['end'] - up['at'], 'next_B_Down_handler_ms': next_down['end'] - next_down['at'],
                     'A_Up_to_first_operation_ms': row['first_A_commit_ms'], 'A_Up_to_AB_qualified_callback_ms': row['first_AB_present_ms'],
                     'AB_drain_ms': row['up_to_drain_ms'], 'C_drain_ms': row['followup_erase_ms'],
                     'masked_selection_ms': row['masked_selection_ms'], 'AB_stamp_and_commit_cost_ms': costs,
                     'C_stamp_and_commit_cost_ms': c_costs, 'phases': phases, 'workers': workers,
                     'semantic': semantic, 'entropy': row['entropy'], 'closed': True, 'criticalLost': 0,
                     'capture_cost_ms': {k: v['captureEnd'] - v['captureStart'] for k, v in row['snapshots'].items()}})
        buffers[variant] = {name: info['sha256'] for name, info in index['files'].items()}
    comparisons = {name: {'equal': digest == buffers['C'].get(name), 'B_sha256': digest, 'C_sha256': buffers['C'].get(name)}
                   for name, digest in buffers['B'].items()}
    assert all(v['equal'] for name, v in comparisons.items() if not name.endswith('.rgba')), 'B/C semantic/operation difference'
    pixel_differences = json.loads((RESULT / 'focused-pixel-differences.json').read_text())
    result = {'status': 'FOCUSED_POSTUP_PAIR_VERIFIED_WITH_FINDINGS', 'rows': rows, 'buffer_comparisons': comparisons,
              'all_semantic_operation_buffers_identical': True, 'pixel_differences': pixel_differences, 'interpretation': [
                  'This new case batches the same confirmed points before RAF; it does not repair or replace the original interleaved-input timeout.',
                  'Normal SDKs, one serial pair only. These are observations, not a relative performance PASS.',
                  'Current lightweight status calls do not provide Scene stamp stage counters. Missing counters are unknown/null, never zero; no extra private facts polling was inserted.',
                  'Qualified callbacks and final actual-target captures do not identify the first correct physical display pixel.',
                  'The independent full compiler native oracle verifies the optimized stamp; B/C Web snapshots, operations and Undo/Redo are byte-identical. Target pixel differences are separately recorded without changing AA policy or claiming byte equality.',
                  '3s/5s are progress bands; remaining canonical/feedback work and synchronous long calls remain unresolved.']}
    (RESULT / 'focused-postup-paired.json').write_text(json.dumps(result, indent=2) + '\n')
    print(json.dumps([{'variant': r['variant'], 'AB_ms': r['AB_drain_ms'], 'C_ms': r['C_drain_ms'],
                       'AB_stamp_ms': r['AB_stamp_and_commit_cost_ms']['sceneStampPrepareNs'],
                       'C_stamp_ms': r['C_stamp_and_commit_cost_ms']['sceneStampPrepareNs']} for r in rows], indent=2))


if __name__ == '__main__':
    main()
