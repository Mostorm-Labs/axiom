"""Verify this engineering result; never issue a Gate or platform PASS."""
from pathlib import Path
import gzip, hashlib, json, subprocess, xml.etree.ElementTree as ET

R = Path(__file__).resolve().parent
read = lambda p: json.loads(gzip.decompress(p.read_bytes()) if p.suffix == '.gz' else p.read_bytes())
index = read(R / 'SHA256.json')
files = {str(p.relative_to(R)) for p in R.rglob('*') if p.is_file() and p.name != 'SHA256.json'}
assert files == set(index)
for n, h in index.items():
    assert hashlib.sha256((R / n).read_bytes()).hexdigest() == h, n
result = read(R / 'result.json')
revision = 'aff81fc12a6e6ab994d30a5f1e11049c3e9e5535'
assert result['code_revision'] == revision and not result['officialGatePassClaimed']
assert result['defaultBudgetMiB'] == {'cache': 32, 'shared': 96}
repository = subprocess.check_output(['git', 'rev-parse', '--show-toplevel'], text=True).strip()
for name, digest in read(R / 'execution-inputs.json').items():
    content = subprocess.check_output(['git', '-C', repository, 'show', revision + ':' + name])
    assert hashlib.sha256(content).hexdigest() == digest, name
native = read(R / 'checks/native-comparison.json')
prior = read(R / 'checks/prior-native-comparison.json')
cases = list(ET.fromstring(gzip.decompress((R / 'checks/native-results.xml.gz').read_bytes())).iter('testcase'))
failed = sorted(c.attrib['name'] for c in cases if c.find('failure') is not None or c.find('skipped') is not None or c.attrib.get('status') in ('fail', 'notrun'))
assert native['sourceRevision'] == revision
assert native['total'] == len(cases) == 1086 and native['passed'] == 1020
assert failed == sorted(native['failed_names']) == sorted(prior['failed_names'])
assert len(failed) == 66 and not native['new_failed_names'] and not native['binariesChangedDuringRun']
assert read(R / 'checks/native-binaries-before.json') == read(R / 'checks/native-binaries-after.json')
assert '# pass 42' in (R / 'checks/node.log').read_text() and '# fail 0' in (R / 'checks/node.log').read_text()
assert '119 contract files' in (R / 'checks/boundaries.log').read_text()
matrix = read(R / 'checks/gpu-matrix.json')
assert matrix['before'] == matrix['after']
assert len(matrix['rows']) == 9
for row in matrix['rows']:
    d = read(R / ('checks/gpu-' + row['mode'] + '.json'))
    assert row['status'] == d['status'] == 'PASS' and row['exit'] == d['result']['code'] == 0
    assert not d['pageErrors'] and d['gl']
    assert all(d[n] == h for n, h in matrix['before'].items())
assert 'live resident-miss changed device geometry or opacity' in read(R / 'red/gpu-live-dpr.json')['result']['errors']
assert 'translated drag bypassed unchanged caches or rerasterized more than its moved source' in read(R / 'red/gpu-translated.json')['result']['errors']
assert 'affine cached/direct pixels differ' in (R / 'red/cpu-qualification.log').read_text()

packages = read(R / 'packages.json')
for name in ['64-192', '256-768']:
    p = packages[name]
    assert p['count'] == len(p['files']) == 36 and not p['forbiddenFiles']
    assert list(p['exports']) == ['.']
    a, b = name.split('-')
    assert p['buildFacts'] == {'AXIOM_ENABLE_FRAME_TIMING': 'OFF', 'AXIOM_INSTALL_DEVELOPER_COMPONENTS': 'OFF', 'AXIOM_RENDER_CACHE_MIB': a, 'AXIOM_RENDER_SHARED_MIB': b}

def worker(folder):
    w = read(folder / 'worker.json')
    assert w['reason'] == 'completed' and w['returncode'] == 0
    assert w['cleanup_complete'] and not w['remaining_owned_pids']
    assert w['elapsed_s'] < w['timeout_s'] == 180 and w['peak_process_family_rss_kib'] <= w['rss_limit_kib'] == 3072 * 1024
    return w

def observation(folder, package, expected_revision):
    d = read(folder / 'observation.json.gz')
    assert d['status'] == 'MEASURED' and d['sourceStable'] and d['artifactStable']
    assert d['sourceBefore'] == d['sourceAfter'] and not d['sourceBefore']['dirty']
    assert d['sourceBefore']['commit'] == revision
    assert d['artifactBefore'] == d['artifactAfter'] == packages[package]['files']
    assert d['setup']['build']['sourceRevision'] == expected_revision and not d['setup']['build']['dirty']
    assert d['setup']['state']['objects'] == 9 and d['close']['closed'] and not d['close']['errors'] and not d['errors']
    return d

red = read(R / 'red/root-pixels/observation.json.gz')
assert red['optical']['interior'] == red['optical']['lost'] == 10506 and not red['optical']['ok']
for dpr in [1, 2, 4]:
    folder = R / f'pixels/dpr{dpr}'
    d = observation(folder, '64-192', revision); worker(folder)
    assert d['kind'] == 'pixel' and d['dpr'] == dpr and d['fixture']['rawSamples'] == 128
    assert d['erase']['during']['activeContacts'] == 1 and d['erase']['before']['generation'] == d['erase']['during']['generation']
    assert d['optical']['ok'] and d['optical']['interior'] > 100
    assert d['optical']['lost'] == d['optical']['changed'] == d['optical']['maxDelta'] == 0

summary = read(R / 'summary.json')
for row in summary['dragRows']:
    folder = R / 'drag' / row['label']
    d = observation(folder, row['package'], row['sourceRevision']); w = worker(folder)
    assert d['kind'] == 'drag' and not d['profiled'] and d['dpr'] == 2
    assert d['fixture']['rawSamples'] == 7200 and d['fixture']['duplicates'] == 8
    assert len(d['drag']['samples']) == 48 and all(d['drag']['checks'].values())
    values = sorted(s['frameMs'] for s in d['drag']['samples'])
    assert row['summary'] == d['summary'] and row['worker'] == w
    assert d['summary']['p95FrameMs'] == values[int(.95 * len(values))] and d['summary']['maxFrameMs'] == values[-1]
    assert d['summary']['downMs'] == d['drag']['downMs'] and d['summary']['upMs'] == d['drag']['upMs']
    assert d['drag']['after']['activeContacts'] == 0
assert len(summary['dragRows']) >= 3
assert summary['fixedBudgetFinalPixelsEqual']
dense = R / 'corroborative/dense-final-pixels'
d = observation(dense, '64-192', revision); worker(dense)
assert d['diagnosticSourceSha256'] == hashlib.sha256((dense / 'fixture.mjs').read_bytes()).hexdigest()
assert not d['denseOracle']['state']['selected']
assert d['denseOracle']['selected']['generation'] == d['denseOracle']['state']['generation']
assert d['denseOracle']['selected']['lastCommit']['operationId'] == d['denseOracle']['state']['lastCommit']['operationId']
comparison = read(dense / 'comparison.json')
assert comparison['flat5x5Pixels'] > 1500000 and comparison['flat5x5DeltaOver2'] == 0
assert comparison['flat5x5MaxDelta'] <= 2
print(json.dumps({'status': 'VERIFIED_ENGINEERING_EVIDENCE', 'files': len(index), 'native': [1020, 1086], 'newFailures': [], 'gpuModes': 9, 'pixelDpr': [1, 2, 4], 'dragRows': len(summary['dragRows']), 'officialGatePassClaimed': False}))
