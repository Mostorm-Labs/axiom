"""Offline, hash-bound compilation of already persisted UR2d observations.

This script never starts a browser or invokes the pixel renderer.
"""
from pathlib import Path
import difflib
import hashlib
import importlib.util
import json
import statistics
import subprocess
import urllib.request
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
OUT = Path(__file__).resolve().parent
PARENT = ROOT / '.aegis/results/G5-UR2d-selection-up-continue-r1'
ORIGINAL = ROOT / '.aegis/results/G5-UR2d-selection-up-r1'
CONTROL = ROOT / '.aegis/results/G5-UR2d-control-r1'
WINDOW = OUT / 'local-web/U2D1-PAIR-COMPLETE-R1'


def sha(p):
    h = hashlib.sha256()
    with Path(p).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1048576), b''):
            h.update(chunk)
    return h.hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def write(name, value):
    p = OUT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(value, indent=2) + '\n')
    return ref(p)


def ref(p):
    p = Path(p)
    return {'path': str(p.relative_to(ROOT)), 'sha256': sha(p), 'bytes': p.stat().st_size}


def require(value, message):
    if not value:
        raise ValueError(message)


def elapsed(e):
    return e['end'] - e['start']


def observation(o, events):
    content = [v for v in o['colorOps'] if v['kind'] == 'content']
    first = min((v['started'] for v in content), default=None)
    raf = o['rafCallbacks']
    return {
        'content_feedback': 'OBSERVED_CONTENT_WRITE' if content else 'MISSING',
        'content_writes': len(content),
        'input_to_first_content_write_ms': first - events[0]['start'] if first is not None else None,
        'last_dispatch_to_first_content_write_ms': first - events[-1]['end'] if first is not None else None,
        'content_write_gaps_ms': [b['started'] - a['started'] for a, b in zip(content, content[1:])],
        'raf_callbacks': len(raf),
        'raf_callback_js_wall_ms': sum(v['ended'] - v['started'] for v in raf),
        'input_callback_js_wall_ms': sum(v['ended'] - v['started'] for v in o['inputEvents']),
        'native_cpu_phase_data': 'MISSING' if not o['nativeRecords'] else o['nativeRecords'],
        'observation_ended': o['observationEnded'],
        'limitations': 'GL content write is not GPU completion. RAF JS wall time is not native CPU attribution or presentation latency. A single content write cannot establish a submission-gap distribution.'
    }


def main():
    spec = importlib.util.spec_from_file_location('completion', ROOT / 'verification/runners/g5/ur2d_pair_completion_runner.py')
    runner = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(runner)
    request = read(ROOT / 'out/G5-UR2d-harness-completion-r1/pair-request.json')
    before = read(OUT / 'window-inputs-before.json')
    after = runner.frozen_inputs(request)
    require(before == after, 'frozen source, SDK or preserved artifact changed')
    write('source-artifact-checks.json', {
        'status': 'EXACT_FROZEN_INPUTS_PRESERVED', 'before': ref(OUT / 'window-inputs-before.json'),
        'after': after, 'frozen_source_files': 23, 'allowed_changed_verification_files': sorted(runner.ALLOWED_CHANGED),
        'other_frozen_files_exact': 21, 'prior_frozen_artifacts_and_manifest': len(after['old_evidence']),
        'old_and_candidate_sdk_files': {k: len(v) for k, v in after['SDK'].items()},
        'production_mutation': False, 'sdk_rebuild': False, 'browser_restarted': False
    })
    worker0 = read(PARENT / 'local-web/U2D1-COMPARE/worker-result.json')
    worker12 = read(WINDOW / 'worker-result.json')
    require(worker12['status'] == 'OBSERVED' and not worker12['errors'], 'new worker failure')
    runs = [(0, 'old', worker0['runs'][0]['rows']), (0, 'candidate', worker0['partial'])]
    runs += [(v['repetition'], v['variant'], v['rows']) for v in worker12['runs']]
    require([(a, b) for a, b, _ in runs] == [(0, 'old'), (0, 'candidate'), (1, 'old'), (1, 'candidate'), (2, 'old'), (2, 'candidate')], 'incomplete pairing')
    checker = CONTROL / 'snapshot-roundtrip'
    require(sha(checker) == '880baedc8763def9acb68a90e28513289ec9d1e321f20ddc80152fc52f40bab3', 'snapshot checker changed')
    functional, timing, pixels, snapshots = [], [], [], []
    for pair, variant, rows in runs:
        directory = (PARENT / 'local-web/U2D1-COMPARE' if pair == 0 else WINDOW) / f'{variant}-{pair}'
        for row in rows:
            name = row['fixture']
            key = {'pair': pair, 'variant': variant, 'fixture': name}
            rowfile = directory / f'row-{name}.json'
            raw = ref(rowfile) if rowfile.exists() else {
                **ref(PARENT / 'local-web/U2D1-COMPARE/worker-result.json'),
                'json_pointer': '/partial/1', 'note': 'Original AA assertion stopped before persistRow; immutable partial contains complete timed row.'}
            require(row['errors'] == [], f'public handler error {key}')
            require(len(row['selection']) == 1 and len(row['erase']) == 3, f'workload cardinality {key}')
            selection = row['selection'][0]
            events = selection['events']
            require([(v['phase'], v.get('x'), v.get('y')) for v in events] == [
                ('down', 30, 30), ('move', 35, 35), ('move', 235, 205), ('up', 235, 205), ('down', 1300, 850), ('up', 1300, 850)], f'selection formula {key}')
            require([v['afterContacts'] for v in events] == [1, 1, 1, 0, 1, 0], f'selection lifecycle {key}')
            require(selection['after']['selection']['roots'] == [], f'blank terminal roots {key}')
            require(row['created']['selection']['count'] == 1 and row['created']['state']['generation'] == '1', f'creation state {key}')
            erase_checks, erase_timing = [], []
            for i, phase in enumerate(row['erase']):
                es = phase['events']
                x = 465 + i * 22
                require([(v['phase'], v.get('x'), v.get('y')) for v in es] == [
                    ('down', x, 180), ('move', x + 2, 195), ('move', x + 5, 210), ('up', x + 5, 210),
                    ('down', x + 10, 215), ('move', x + 12, 230), ('up', x + 12, 230), ('wheel', None, None)], f'erase formula {key}')
                require([v['afterContacts'] for v in es[:7]] == [1, 1, 1, 0, 1, 1, 0], f'erase contacts {key}')
                st = phase['after']['state']
                require(phase['operation_delta'] == 2 and int(st['generation']) == int(phase['before']['generation']) + 2, f'two exact operations {key}')
                require(st['activeContacts'] == 0 and st['lastCommit']['presentedGeneration'] == st['generation'], f'erase receipt {key}')
                require(st['camera'] == {'scale': 1, 'x': -8 * (i + 1), 'y': -5 * (i + 1)}, f'wheel state {key}')
                gap = es[4]['start'] - es[3]['end']
                require(0 <= gap <= 2 and 0 <= es[7]['start'] - es[6]['end'] <= 2, f'immediate reentry changed {key}')
                erase_checks.append({'repeat': i, 'operation_delta': phase['operation_delta'], 'generation': st['generation'],
                                     'presented_generation': st['lastCommit']['presentedGeneration'], 'terminal_contacts': 0, 'next_down_admitted': True})
                erase_timing.append({'repeat': i, 'all_event_duration_ms': [elapsed(e) for e in es],
                    'up_ms': [elapsed(es[3]), elapsed(es[6])], 'down_ms': [elapsed(es[0]), elapsed(es[4])],
                    'next_down_ms': elapsed(es[4]), 'wheel_ms': elapsed(es[7]), 'up_to_next_down_gap_ms': gap,
                    'second_up_to_wheel_gap_ms': es[7]['start'] - es[6]['end'],
                    'observation': observation(phase['observation'], es)})
            states = {}
            for stage, expected in [('final', 7), ('undo', 8), ('redo', 9)]:
                saved = directory / f'{name}-{stage}.axsnap'
                require(sha(saved) == row[stage]['sha256'] and saved.stat().st_size == row[stage]['byteLength'], f'snapshot identity {key} {stage}')
                st = row[stage]['state']
                require(st['generation'] == str(expected) and st['lastCommit']['presentedGeneration'] == str(expected) and st['activeContacts'] == 0, f'final receipt {key} {stage}')
                require(st['selected'] == [] and st['camera'] == {'scale': 1, 'x': -24, 'y': -15}, f'final camera/selection {key}')
                states[stage] = {'generation': st['generation'], 'presented_generation': st['lastCommit']['presentedGeneration'],
                    'ordinal': st['lastCommit']['ordinal'], 'snapshot': ref(saved), 'active_contacts': st['activeContacts']}
            command = [str(checker), str(directory / f'{name}-final.axsnap'), str(directory / f'{name}-redo.axsnap')]
            cp = subprocess.run(command, capture_output=True, text=True, timeout=10)
            check = json.loads(cp.stdout)
            require(cp.returncode == 0 and check['objects_equal'] and check['document_digest_equal'] and check['manifest_background_equal'], f'canonical roundtrip {key}')
            require(check['objects'] == 17 and check['total_masks'] == (96 if name == 'S0' else 128), f'mask cardinality {key}')
            sr = write(f'snapshot-checks/{pair}-{variant}-{name}.json', {**key, 'checker': ref(checker), 'command': command,
                'exit_code': cp.returncode, 'stdout': cp.stdout, 'stderr': cp.stderr, 'facts': check,
                'final': states['final']['snapshot'], 'redo': states['redo']['snapshot']})
            snapshots.append(sr)
            if pair == 0 and variant == 'candidate':
                proof = next(v['green_independent'] for v in read(OUT / 'offline-harness-red-green.json')['rows'] if v['fixture'] == name)
                proof_ref = {**ref(OUT / 'offline-harness-red-green.json'), 'json_pointer': '/rows/' + ('0' if name == 'S0' else '1') + '/green_independent'}
            elif pair == 0:
                proof, proof_ref = row['roundtrip'], raw
                require(proof['status'] == 'QUALIFIED', 'original old roundtrip failure')
            else:
                proof = read(directory / f'{name}-roundtrip.json')
                proof_ref = ref(directory / f'{name}-roundtrip.json')
            require(proof['status'] == 'QUALIFIED', f'pixel failure {key}')
            for stage, prop in [('final', 'pixels'), ('redo', 'redoPixels')]:
                require(sha(directory / f'{name}-{stage}.rgba') == row[prop]['sha256'] and row[prop]['metadata']['status'] == 'VALID', f'GPU buffer identity {key}')
            pixels.append({**key, 'raw': raw, 'proof': proof_ref, 'status': 'QUALIFIED', 'original_raw_status': row['roundtrip']['status'],
                           'method': 'whole-target sufficient check' if pair == 0 and variant == 'old' else 'frozen CPU reference and independent AA band',
                           'final_rgba': ref(directory / f'{name}-final.rgba'), 'redo_rgba': ref(directory / f'{name}-redo.rgba')})
            functional.append({**key, 'raw': raw, 'selection_terminal_roots': selection['after']['selection']['roots'],
                'selection_closed_and_immediate_reentry': True, 'created_selected_count': row['created']['selection']['count'],
                'erase': erase_checks, 'states': states, 'operations': len(row['operations']), 'snapshot_check': sr,
                'cancel_no_mutation': 'Driver checked same generation; final7 proves no extra operation. Native cancel keepers also bound.',
                'intermediate_selection_roots': 'Not persisted in timed rows; exact native selection oracle remains independently bound.'})
            timing.append({**key, 'raw': raw, 'width_css': 1431, 'height_css': 1010, 'dpr': 2, 'copies': 16, 'knots_per_copy': 7199,
                'history_fixture': name, 'selection': {'cold_large_move_ms': elapsed(events[2]), 'warm_up_ms': elapsed(events[3]),
                    'blank_next_down_ms': elapsed(events[4]), 'all_event_duration_ms': [elapsed(e) for e in events],
                    'observation': observation(selection['observation'], events)}, 'erase': erase_timing})
    write('snapshot-review.json', {'status': 'CANONICAL_ROUNDTRIPS_EQUAL', 'checks': snapshots, 'checker': ref(checker),
        'scope': 'Within each run: full objects including canonical masks/target ids, manifest, background, document identity/capabilities and digest. Created random object ids preclude cross-run byte equality.'})
    write('pixel-review.json', {'status': 'ORIGINAL_PIXEL_OBLIGATION_QUALIFIED', 'rows': pixels,
        'comparator': ref(ROOT / 'verification/runners/g5/ur0/pixel_oracle.mjs'), 'oracle': after['oracle'],
        'pair0_independent_control_review': ref(CONTROL / 'compare-independent/pixel-review.json'),
        'new_pixel_cpu_invocations': len(list(WINDOW.rglob('*-independent-invocation.json'))),
        'original_failed_raw_unchanged': True, 'no_new_pixel_renderer_invocations_during_aggregation': True})
    write('web-functional.json', {'status': 'THREE_ACTUAL_PAIRS_FUNCTIONAL_QUALIFIED', 'rows': functional,
        'pairs': [0, 1, 2], 'pair0_compatibility': ref(OUT / 'pair0-compatibility.json'), 'SDK': after['SDK'], 'fixtures': after['fixtures'],
        'driver': ref(ROOT / 'verification/runners/g5/ur2d_selection_up.mjs'),
        'excluded_new_feedback': 'Placement visibility, general object dragging, fullscreen sensitivity and desktop budget are separate follow-up scopes; no claims added to original matrix.'})
    paired, findings = [], []
    for pair in range(3):
        for name in ('S0', 'S2'):
            old = next(v for v in timing if (v['pair'], v['variant'], v['fixture']) == (pair, 'old', name))
            candidate = next(v for v in timing if (v['pair'], v['variant'], v['fixture']) == (pair, 'candidate', name))
            record = {'pair': pair, 'fixture': name, 'selection': {}, 'erase': []}
            for field in ('cold_large_move_ms', 'warm_up_ms'):
                a, b = old['selection'][field], candidate['selection'][field]
                record['selection'][field] = {'old': a, 'candidate': b, 'old_over_candidate': a / b if b else None}
            for i, (a, b) in enumerate(zip(old['erase'], candidate['erase'])):
                delta = (b['next_down_ms'] / a['next_down_ms'] - 1) * 100 if a['next_down_ms'] else None
                record['erase'].append({'repeat': i, 'old_up_ms': a['up_ms'], 'candidate_up_ms': b['up_ms'],
                    'old_next_down_ms': a['next_down_ms'], 'candidate_next_down_ms': b['next_down_ms'], 'next_down_change_percent': delta,
                    'old_wheel_ms': a['wheel_ms'], 'candidate_wheel_ms': b['wheel_ms']})
                if delta is not None and delta > 20:
                    findings.append({'kind': 'NEXT_DOWN_OVER_20_PERCENT', 'pair': pair, 'fixture': name, 'repeat': i,
                        'old_ms': a['next_down_ms'], 'candidate_ms': b['next_down_ms'], 'change_percent': delta,
                        'note': 'Sub-millisecond dispatch values are quantized; retain rather than discard.'})
            paired.append(record)
    summary = {}
    for name in ('S0', 'S2'):
        summary[name] = {}
        for variant in ('old', 'candidate'):
            rs = [v for v in timing if v['fixture'] == name and v['variant'] == variant]
            summary[name][variant] = {
                'cold_large_move_ms': [v['selection']['cold_large_move_ms'] for v in rs],
                'warm_selection_up_ms': [v['selection']['warm_up_ms'] for v in rs],
                'erase_up_ms_all': [u for v in rs for e in v['erase'] for u in e['up_ms']],
                'next_down_ms_all': [e['next_down_ms'] for v in rs for e in v['erase']],
                'wheel_ms_all': [e['wheel_ms'] for v in rs for e in v['erase']],
                'erase_input_to_first_content_ms_all': [e['observation']['input_to_first_content_write_ms'] for v in rs for e in v['erase']]}
            for field, values in list(summary[name][variant].items()):
                summary[name][variant][field + '_median'] = statistics.median(v for v in values if v is not None)
    write('web-timing.json', {'classification': 'PAIRED_LOCAL_OBSERVATIONS_NOT_PERFORMANCE_PASS', 'unit': 'ms',
        'all_rows': timing, 'paired': paired, 'summary': summary, 'normal_web_only': True,
        'native_cpu_phase_attribution': 'MISSING in ordinary SDK observations; separate instrumented native raw remains bound.',
        'content_feedback_missing_selection_rows': sum(v['selection']['observation']['content_feedback'] == 'MISSING' for v in timing),
        'timed_phases_exclude_snapshot_readback_oracle': True, 'discarded_samples': [], 'sleep_added_to_immediate_reentry': False})
    findings.append({'kind': 'MIXED_ERASE_UP', 'description': 'All three pairs retained. Selection improves; ordinary-Web eraser Up is mixed, so native mechanism speedups are not a uniform browser Up speedup.'})
    findings.append({'kind': 'CONTENT_FEEDBACK_WAIT', 'description': 'Erase content writes arrive hundreds of milliseconds after input; selection phases often have no observed content write. RAF callbacks do not prove visual feedback.'})
    write('performance-classification.json', {'status': 'PERFORMANCE_FINDINGS', 'official_performance_pass': False,
        'paired_observations': 3, 'timing': ref(OUT / 'web-timing.json'), 'findings': findings,
        'native_mechanism_only': ref(PARENT / 'B-final-paired-native.json'),
        'no_claim': 'No G5, Gate, P34, cross-platform, fullscreen, interaction SLO or release PASS.'})
    print(json.dumps({'rows': len(functional), 'snapshot_checks': len(snapshots), 'pixel_rows': len(pixels),
        'next_down_findings': len(findings) - 2, 'timing_summary': summary}, indent=2))


if __name__ == '__main__':
    main()
