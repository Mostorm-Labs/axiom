"""Compile persisted budget observations without new browser sampling."""
from pathlib import Path
import difflib
import hashlib
import importlib.util
import json
import shutil
import statistics
import subprocess
import urllib.request
import xml.etree.ElementTree as ET

ROOT = Path(__file__).resolve().parents[3]
RESULT = Path(__file__).resolve().parent
OUTPUT = ROOT / 'out/G5-desktop-render-budget-r1'


def sha(p):
    digest = hashlib.sha256()
    with Path(p).open('rb') as stream:
        for chunk in iter(lambda: stream.read(1048576), b''):
            digest.update(chunk)
    return digest.hexdigest()


def read(p):
    return json.loads(Path(p).read_text())


def ref(p):
    p = Path(p)
    return {'path': str(p.relative_to(ROOT)), 'sha256': sha(p), 'bytes': p.stat().st_size}


def write(name, data):
    p = RESULT / name
    p.parent.mkdir(parents=True, exist_ok=True)
    p.write_text(json.dumps(data, indent=2) + '\n')
    return ref(p)


def require(ok, message):
    if not ok:
        raise ValueError(message)


def main():
    package_file = ROOT / '.aegis/packages/G5-desktop-render-budget-r1/package.json'
    package = read(package_file)
    require(sha(package_file) == '17714da21f8c44b3d124e53f16ea7a55f06afa51d114f01dc6b061dff6e3bfcf', 'package hash')
    for key, binding in package.items():
        if key.endswith('_ref') and isinstance(binding, dict) and 'sha256' in binding:
            require(sha(package_file.parent / binding['path']) == binding['sha256'], 'package binding ' + key)
    ec = read(package_file.parent / 'execution-contract.json')
    source_files, delta, changed = {}, [], []
    for name in ec['scope']['allowed_existing'] + ec['scope']['allowed_new']:
        current = ROOT / name
        if not current.is_file():
            continue
        pre = RESULT / 'preimages' / name
        same = pre.exists() and sha(pre) == sha(current)
        if not same:
            changed.append(name)
            post = RESULT / 'new-source-postimages' / name
            post.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(current, post)
            source_files[name] = {'current': ref(current), 'postimage': ref(post), 'preimage': ref(pre) if pre.exists() else None}
            delta.extend(difflib.unified_diff(pre.read_text().splitlines(keepends=True) if pre.exists() else [],
                                            current.read_text().splitlines(keepends=True),
                                            fromfile='a/' + name if pre.exists() else '/dev/null', tofile='b/' + name))
        else:
            source_files[name] = {'unchanged': ref(current), 'preimage': ref(pre)}
    (RESULT / 'source-delta.patch').write_text(''.join(delta))
    inherited = read(ROOT / '.aegis/results/G5-UR2d-selection-up-continue-r1/source-manifest.json')['files']
    unchanged = {}
    for name, old in inherited.items():
        if name in changed or name.startswith('verification/'):
            continue
        require(sha(ROOT / name) == old['sha256'], 'unrelated product source changed ' + name)
        unchanged[name] = old['sha256']
    harness = read(ROOT / '.aegis/results/G5-UR2d-harness-completion-r1/source-manifest.json')
    for name, value in harness['files'].items():
        require(sha(ROOT / name) == value['current']['sha256'], 'UR2d completed harness changed ' + name)
    write('source-manifest.json', {'schema': 'g5-desktop-budget-source-v1', 'head': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'branch': 'codex/geometry-navigation-integration', 'worktree': str(ROOT), 'files': source_files,
        'changed_in_this_package': changed, 'inherited_product_sources_unchanged': unchanged,
        'delta': ref(RESULT / 'source-delta.patch'), 'preflight': ref(RESULT / 'PackageBindingPreflight.json'),
        'UR2d_final_result': ref(ROOT / '.aegis/results/G5-UR2d-harness-completion-r1/result.json'),
        'public_SDK_semantic_dependency_lock_changes': False, 'thirdparty_source_builds': False})
    window = RESULT / 'local-web/G5MB1-AREA-COMPARE'
    ledger = read(RESULT / 'sampling-ledger.json')
    require(ledger['attempts'][0]['status'] == 'COMPLETED', 'browser window not completed')
    worker = read(window / 'worker-result.json')
    require(len(worker['rows']) == 8 and worker['status'] == 'OBSERVED', 'browser rows incomplete')
    checker = ROOT / '.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip'
    require(sha(checker) == '880baedc8763def9acb68a90e28513289ec9d1e321f20ddc80152fc52f40bab3', 'snapshot checker changed')
    rows = []
    for raw in worker['rows']:
        directory = window / raw['id']
        cp = subprocess.run([str(checker), str(directory / 'S2-final.axsnap'), str(directory / 'S2-redo.axsnap')], capture_output=True, text=True, timeout=10)
        canonical = json.loads(cp.stdout)
        require(cp.returncode == 0 and canonical['objects_equal'] and canonical['document_digest_equal'] and
                canonical['manifest_background_equal'] and canonical['objects'] == 18, 'canonical roundtrip failed')
        checkref = write('snapshot-checks/' + raw['id'] + '.json', {'checker': ref(checker), 'exit_code': cp.returncode,
            'stdout': cp.stdout, 'stderr': cp.stderr, 'final': ref(directory / 'S2-final.axsnap'), 'redo': ref(directory / 'S2-redo.axsnap')})
        require(raw['roundtrip']['status'] == 'QUALIFIED' and raw['after']['state']['activeContacts'] == 0, 'actual pixels or contacts')
        for stage in ('final', 'redo'):
            require(sha(directory / f'S2-{stage}.axsnap') == raw[stage]['sha256'] and
                    sha(directory / f'S2-{stage}.rgba') == raw[stage]['rgba_sha256'], 'raw byte identity')
        obs = raw['observation']
        content = [v for v in obs['colorOps'] if v['kind'] == 'content']
        times = [v['started'] for v in content]
        gaps = [b - a for a, b in zip(times, times[1:])]
        per_move = []
        for move in raw['moves']:
            subsequent = next((t for t in times if t >= move['start']), None)
            per_move.append({'dispatch_ms': move['end'] - move['start'],
                'input_to_next_content_write_ms': subsequent - move['start'] if subsequent is not None else None})
        row = {k: raw[k] for k in ('id', 'width', 'height', 'dpr', 'kind', 'variant')}
        row.update(raw=ref(directory / 'qualified-row.json'), snapshot_check=checkref, canonical_facts=canonical,
            pixel_status=raw['roundtrip']['status'], generation=raw['after']['state']['generation'],
            presented_generation=raw['after']['state']['lastCommit']['presentedGeneration'],
            active_contacts=raw['after']['state']['activeContacts'], cancel_generation=raw['cancel_generation'],
            initial_selection=raw['initial']['selection'], final_transform=raw['after']['selection']['transform'],
            drag_operations=int(raw['after']['state']['generation']) - int(raw['initial']['state']['generation']),
            content_writes=len(content), first_input_to_content_write_ms=times[0] - raw['down']['start'] if times else None,
            content_write_gaps_ms=gaps, max_content_write_gap_ms=max(gaps, default=None), moves=per_move,
            up_dispatch_ms=raw['up']['end'] - raw['up']['start'], completion_wait_ms=raw['completion_wait_ms'],
            raf_callback_js_wall_ms=sum(v['ended'] - v['started'] for v in obs['rafCallbacks']), raf_callbacks=len(obs['rafCallbacks']),
            native_cache_counters='Not exposed in ordinary SDK; measured separately by actual native Render owner.')
        rows.append(row)
    paired = []
    for width in (600, 1431):
        for kind in ('shape', 'stroke'):
            old = next(r for r in rows if (r['width'], r['kind'], r['variant']) == (width, kind, 'old'))
            new = next(r for r in rows if (r['width'], r['kind'], r['variant']) == (width, kind, 'candidate'))
            paired.append({'width': width, 'kind': kind, 'old_completion_ms': old['completion_wait_ms'],
                'candidate_completion_ms': new['completion_wait_ms'], 'old_first_content_ms': old['first_input_to_content_write_ms'],
                'candidate_first_content_ms': new['first_input_to_content_write_ms'],
                'old_raf_js_wall_ms': old['raf_callback_js_wall_ms'], 'candidate_raf_js_wall_ms': new['raf_callback_js_wall_ms'],
                'old': old['raw'], 'candidate': new['raw']})
    write('web-area-comparison.json', {'status': 'SAME_FIXTURE_PUBLIC_INPUT_AND_PIXEL_QUALIFIED', 'rows': rows, 'paired': paired,
        'sampling_ledger': ref(RESULT / 'sampling-ledger.json'), 'window_freeze': ref(window / 'freeze.json'),
        'unit': 'ms', 'pixel_invocations': len(list(window.rglob('*-independent-invocation.json'))),
        'timing_limits': 'One serial process, three short moves per case; no repeated statistical performance PASS. GL writes are not GPU completion or input-to-photon; RAF wall times are not CPU attribution.',
        'fixture_operations': 'Same restored S2/16x7199knots/2history, add rectangle and one zero-offset duplicate through root public SDK, then drag shape or stroke exactly once. Native mixed matrix separately covers sticky/table/image.',
        'readback_after_timing': True, 'no_user_pages_or_documents_used': True})
    junit = ET.parse(RESULT / 'keeper-junit.xml').getroot()
    require(junit.attrib['tests'] == '18' and junit.attrib['failures'] == '0', 'native regressions failed')
    write('regressions.json', {'status': 'SCOPED_REGRESSIONS_QUALIFIED', 'native': {'tests': 18, 'failures': 0,
        'junit': ref(RESULT / 'keeper-junit.xml'), 'command': ref(RESULT / 'keeper-command.json'), 'log': ref(RESULT / 'keeper-tests.log'),
        'profile_mib': [32, 96, 8]}, 'new_actual_area': ref(RESULT / 'native-area-budget-comparison.json'),
        'actual_constructor_limits': ref(RESULT / 'effective-budget.json'),
        'configuration_and_guard_python': ref(RESULT / 'offline-python-final.log'), 'original_aa_Node10': ref(RESULT / 'offline-node.log'),
        'boundary120': ref(RESULT / 'boundary-final.log'), 'diff_check': ref(RESULT / 'diff-check.log'),
        'newly_observed_unrelated_build_failure': {'test': 'canvas_semantic_object_store_differential_test',
            'source': 'runtime/semantic/tests/object_store_differential_test.cpp:18',
            'reason': 'Existing switch omits kSection/kTable under -Werror; accidentally invoked untargeted all-build. No dependency source build or semantic edit.',
            'log': ref(RESULT / 'keeper-build.log'), 'followup': 'Exact scoped targets compiled and passed18/18; broader repository build remains a separate blocker.'},
        'first_new_stroke_test_oracle_basis_error_preserved': ref(RESULT / 'native-stroke-band-correction.json'),
        'no_broad_ctest': True})
    native = read(RESULT / 'native-area-budget-comparison.json')
    rawroot = OUTPUT / 'native-area-final'
    native_files = [ref(p) for p in sorted(rawroot.rglob('*')) if p.is_file()]
    write('native-artifacts.json', {'files': native_files, 'pixel_rows': sum(p.name.endswith('-pixels.json') for p in rawroot.rglob('*')),
        'initial_basis_error_files': [ref(p) for p in sorted((OUTPUT / 'native-area').rglob('*')) if p.is_file()],
        'no_raw_deletion': True})
    freeze = read(window / 'freeze.json')
    spec = importlib.util.spec_from_file_location('runner', ROOT / 'verification/runners/g5/desktop_render_budget_runner.py')
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    require(module.inputs(read(OUTPUT / 'request.json')) == freeze['inputs'], 'post-window source or SDK changed')
    preserved = read(ROOT / '.aegis/results/G5-UR2d-harness-completion-r1/preserved-evidence-before.json')
    for name, digest in preserved.items():
        require(sha(ROOT / name) == digest, 'historic UR2d evidence changed')
    oldmanifest = read(ROOT / '.aegis/results/G5-UR2d-harness-completion-r1/artifact-manifest.json')
    for old in oldmanifest['files']:
        require(sha(ROOT / old['path']) == old['sha256'], 'UR2d completed evidence changed')
    for name in ('semantic-sdk.lock.json', 'r1-full-skia-sdk.lock.json'):
        expected = next(x for x in read(package_file.parent / 'authority.lock.json')['current_authority'] if x['path'] == name)
        require(sha(ROOT / name) == expected['sha256'], 'SDK lock changed')
    demo = read(RESULT / 'demo.json')
    for old in demo['http_bytes']:
        with urllib.request.urlopen(demo['url'].replace('example.html', old['file']), timeout=15) as response:
            require(hashlib.sha256(response.read()).hexdigest() == old['sha256'], 'new demo HTTP changed')
    guard = ledger['attempts'][0]['guard']
    write('cleanup.json', {'status': 'CLEAN_WITH_USER_DEMOS_PRESERVED', 'new_browser_starts': 1, 'charged_seconds': ledger['charged_seconds'],
        'guard': ref(RESULT / 'local-web/G5MB1-AREA-COMPARE.worker.json'), 'cleanup_complete': guard['cleanup_complete'],
        'remaining_owned_pids': guard['remaining_owned_pids'], 'preserved_UR2d_frozen_refs': len(preserved),
        'new_demo': ref(RESULT / 'demo.json'), 'new_demo_pid_preserved': demo['pid'],
        'old_demo4316_sdk_exact': True, 'user_pages_4315_4298_4297_preserved': True,
        'live_server_log_excluded': demo['live_log'], 'source_SDK_frozen_through_window': True,
        'all_raw_attempts_retained': True, 'commit_push': False, 'index_empty': not subprocess.check_output(['git', 'diff', '--cached', '--name-only'], cwd=ROOT, text=True).strip()})
    status = 'READY_FOR_CONTROL_REVIEW_WITH_PERFORMANCE_FINDINGS'
    write('findings.json', {'classification': 'LOCAL_SCOPED_CORRECTNESS_QUALIFIED_NOT_PERFORMANCE_PASS',
        'desktop_budget_actual': [256, 512, 64], 'mobile_conservative': [32, 96, 8],
        'area_summary': 'All four native nonink kinds reuse static content at fullscreen DPR2 with desktop budget. Stroke active remains its original nonstatic path, reusing prepared/cached body. Thirty explicit-profile cases and210 pixel comparisons passed.',
        'web_pairs': paired, 'remaining': ['New user erase CPU saturation and next-stroke wait require independent diagnosis; no erase production change in this budget package.',
            'One local process and synthetic short moves do not establish physical input-to-photon, global performance SLO, mobile platform or G5/P34 PASS.',
            'Untargeted native semantic differential test compile failure is preserved separately.'],
        'official_gate_p34_performance_pass': False})
    allfiles = [p for p in sorted(RESULT.rglob('*')) if p.is_file() and p.name not in ('artifact-manifest.json', 'result.json') and p.suffix != '.tmp' and '__pycache__' not in p.parts]
    write('artifact-manifest.json', {'schema': 'g5-desktop-budget-artifacts-v1', 'files': [ref(p) for p in allfiles],
        'native_external_raw_manifest': ref(RESULT / 'native-artifacts.json'), 'new_SDK_manifest': ref(RESULT / 'normal-artifacts.json'),
        'excluded': ['live out/G5-desktop-render-budget-r1/demo-server.log', '__pycache__/', '*.tmp', 'artifact-manifest.json self', 'result.json callback hash owner']})
    required = read(package_file.parent / 'evidence-contract.json')['required']
    require(all((RESULT / name).is_file() for name in required if name != 'result.json'), 'missing required evidence')
    write('result.json', {'schema': 'g5-desktop-render-budget-result-v1', 'status': status, 'package': ref(package_file),
        'result_revision': subprocess.check_output(['git', 'rev-parse', 'HEAD'], cwd=ROOT, text=True).strip(),
        'materialized_ref': {'provider': 'shared-controlled-worktree', 'repository': 'Mostorm-Labs/axiom', 'root': str(ROOT), 'path': str(RESULT.relative_to(ROOT))},
        'source': ref(RESULT / 'source-manifest.json'), 'delta': ref(RESULT / 'source-delta.patch'),
        'manifest': ref(RESULT / 'artifact-manifest.json'), 'SDK': ref(RESULT / 'normal-artifacts.json'),
        'driver': ref(ROOT / 'verification/runners/g5/desktop_render_budget.mjs'),
        'runner': ref(ROOT / 'verification/runners/g5/desktop_render_budget_runner.py'),
        'evidence_input_refs': [ref(RESULT / name) for name in required if name not in ('result.json', 'artifact-manifest.json', 'source-manifest.json')],
        'functional': {'actual_constructor_profiles': ref(RESULT / 'effective-budget.json'), 'native_area_cases': len(native['rows']),
            'native_pixel_rows': len(list(rawroot.rglob('*-pixels.json'))), 'Web_cases': len(rows), 'native_regressions': 18},
        'new_normalDemoURL': demo['url'], 'prior_normalDemoURL': 'http://127.0.0.1:4316/example.html',
        'findings': ref(RESULT / 'findings.json'), 'ledger': ref(RESULT / 'sampling-ledger.json'),
        'no_commit_push': True, 'official_gate_p34_performance_pass': False, 'next_owner': 'Control review / separately scoped erase CPU diagnosis'})
    for value in read(RESULT / 'artifact-manifest.json')['files']:
        require(sha(ROOT / value['path']) == value['sha256'], 'final manifest mismatch')
    print(json.dumps({name: ref(RESULT / name) for name in ('result.json', 'source-manifest.json', 'source-delta.patch', 'artifact-manifest.json', 'normal-artifacts.json', 'sampling-ledger.json')}, indent=2))


if __name__ == '__main__':
    main()
