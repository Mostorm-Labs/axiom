"""Archive observed lifecycle runs, bind unchanged SDKs, retain all tails."""
import gzip
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[4]
OUT = Path(__file__).resolve().parent
RUNS = ROOT / 'out/g5-navigation-lifecycle'


def load(path):
    data = path.read_bytes()
    return json.loads(gzip.decompress(data) if path.suffix == '.gz' else data)


def archive(case):
    source = RUNS / case
    destination = OUT / case
    destination.mkdir(exist_ok=True)
    for path in sorted(source.iterdir()):
        data = path.read_bytes()
        target = destination / (path.name if path.suffix == '.gz' else path.name + '.gz')
        target.write_bytes(data if path.suffix == '.gz' else gzip.compress(data, mtime=0))
    return load(source / 'summary.json')


analysis = {'kind': 'investigation-only; render stutter is not fixed', 'runs': {}}
for case in ['off', 'on', 'selected-control', 'unselected-control']:
    summary = archive(case)
    assert summary['status'] == 'MEASURED'
    records = []
    for result in summary['results']:
        raw = load(RUNS / case / (result['mode'] + '.json.gz'))
        phases = []
        for phase in raw['phases']:
            s = phase['summary']
            rows = [r['value'] for r in raw['rows'] if r['kind'] == 'navigation'
                    and r['phase'] == phase['id'] and r['segment'] == 'input']
            phases.append({'id': phase['id'], 'raf_max_ms': s['raf_ms']['max'],
                           'submission_gap_max_ms': s['input_display_submission_gap_ms']['max'],
                           'updated_inputs': s['updatedInputs'],
                           'post_input_completion_ms': s['settle_ms'],
                           'navigation_calls': len(rows) if rows else None,
                           'not_presented_calls': sum(not r['presented'] for r in rows) if rows else None,
                           'incomplete_coverage_calls': sum(not r['coverage'] for r in rows) if rows else None})
        records.append({'mode': raw['mode'], 'selected': bool(raw['setup']['state']['selected']),
                        'objects': raw['setup']['state']['objects'], 'checks': raw['checks'],
                        'unique_final_pixel_hashes': len({p['raw']['pixels']['sha256'] for p in raw['phases']}),
                        'idle_raf_delta': raw['idleAfter']['page']['stats']['callbacks'] - raw['idleBefore']['page']['stats']['callbacks'],
                        'idle_draw_delta': raw['idleAfter']['page']['stats']['draws'] - raw['idleBefore']['page']['stats']['draws'],
                        'phases': phases})
    analysis['runs'][case] = {'source_tree_sha256': summary['sourceBefore']['tree_sha256'],
                             'source_stable': summary['sourceStable'], 'artifacts_stable': summary['artifactStable'],
                             'idle_ms': summary['idleMs'], 'records': records}

archive('compatibility')
prior = load(OUT.parent / 'g5-prepared-geometry/default/summary.json.gz')
current = load(RUNS / 'off/summary.json')
analysis['default_sdk_matches_prepared_geometry_evidence'] = all(prior['artifacts'].get(k) == v for k, v in current['artifacts'].items())
analysis['prior_build_only_files_excluded_from_consumer_install'] = sorted(set(prior['artifacts']) - set(current['artifacts']))
assert analysis['default_sdk_matches_prepared_geometry_evidence']
assert load(RUNS / 'selected-control/summary.json')['sourceBefore']['tree_sha256'] == load(RUNS / 'unselected-control/summary.json')['sourceBefore']['tree_sha256']
(OUT / 'analysis.json').write_text(json.dumps(analysis, indent=2) + '\n')

for name in ['node-tests.log', 'npm-test.log']:
    (OUT / (name + '.gz')).write_bytes(gzip.compress((ROOT / 'out/g5-soak-investigation' / name).read_bytes(), mtime=0))
(OUT / 'sha256.json').write_text(json.dumps({str(p.relative_to(OUT)): hashlib.sha256(p.read_bytes()).hexdigest()
                                           for p in sorted(OUT.rglob('*')) if p.is_file() and p.name != 'sha256.json'}, indent=2) + '\n')
