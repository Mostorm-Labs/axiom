from pathlib import Path
import collections
import datetime
import hashlib
import json
import math
import gzip
import struct
import subprocess

C = Path('/Users/qing/.codex/worktrees/g5-exposure-checkpoint/axiom')
W = Path('/Users/qing/.codex/worktrees/g5-settled-navigation-recovery')
B = Path('/Users/qing/.codex/worktrees/g5-erase-feedback-frame')
P = C / '.aegis/packages/G5-settled-navigation-recovery-r1'
E = W / '.aegis/results/G5-settled-navigation-recovery-r1'
O = W / 'out/G5-settled-navigation-recovery-r1'
A = Path(__file__).resolve().parent
failures = []
cache = {}

def read(p):
    return json.loads(Path(p).read_text())

def digest(p):
    p = Path(p)
    if str(p) not in cache:
        h = hashlib.sha256()
        with p.open('rb') as f:
            while b := f.read(4 * 1024 * 1024):
                h.update(b)
        cache[str(p)] = {'bytes': p.stat().st_size, 'sha256': h.hexdigest()}
    return cache[str(p)]

def check(ok, reason):
    if not ok:
        failures.append(reason)

def ref(r, base, group):
    p = Path(r['path'])
    if not p.is_absolute():
        p = base / p
    try:
        got = digest(p)
        check(got['sha256'] == r['sha256'] and ('bytes' not in r or got['bytes'] == r['bytes']), group + ': ' + str(p))
    except OSError as ex:
        failures.append(group + ': ' + str(p) + ': ' + str(ex))

def refs(obj, base, group):
    if isinstance(obj, dict):
        if isinstance(obj.get('path'), str) and isinstance(obj.get('sha256'), str):
            ref(obj, base, group)
        for v in obj.values():
            refs(v, base, group)
    elif isinstance(obj, list):
        for v in obj:
            refs(v, base, group)

def sha_map(m, base, group):
    for n, expected in m.items():
        p = Path(n) if Path(n).is_absolute() else base / n
        try:
            check(digest(p)['sha256'] == expected, group + ': ' + str(p))
        except OSError as ex:
            failures.append(group + ': ' + str(ex))

def git(root, *args):
    return subprocess.check_output(['git', '-C', str(root), *args], text=True).strip()

result = read(E / 'result.json')
package = read(P / 'package.json')
repos = {}
for root in (C, W, B):
    repos[str(root)] = {
        'HEAD': git(root, 'rev-parse', 'HEAD'),
        'branch': git(root, 'branch', '--show-current'),
        'origin': git(root, 'remote', 'get-url', 'origin'),
        'trackedStatus': git(root, 'status', '--porcelain', '--untracked-files=no'),
        'indexPaths': git(root, 'diff', '--cached', '--name-only'),
    }
    check('Mostorm-Labs/axiom' in repos[str(root)]['origin'], 'repository identity ' + str(root))
check(repos[str(W)]['HEAD'] == result['repository']['baseHEAD'] == 'c3a84c289d5b3dcb258fcbe7f481febf227df0c0', 'diagnostic HEAD')
check(not repos[str(W)]['indexPaths'], 'diagnostic index nonempty')
check(not repos[str(C)]['trackedStatus'], 'control tracked source changed')
check(not repos[str(B)]['trackedStatus'], 'baseline tracked source changed')
subprocess.run(['git', '-C', str(W), 'merge-base', '--is-ancestor', package['task_anchor']['revision'], repos[str(W)]['HEAD']], check=True)
refs(package, P, 'package')
for p in P.iterdir():
    if p.suffix == '.json':
        refs(read(p), P, p.name)
        check(p.read_bytes() == (W / '.aegis/packages/G5-settled-navigation-recovery-r1' / p.name).read_bytes(), 'package copy ' + p.name)
refs(result, E, 'result')
manifest = read(E / 'artifact-manifest.json')
refs(manifest, E, 'manifest')
freeze_counts = {}
for p in sorted(E.glob('freeze-SNR-D-B-*.json')):
    d = read(p)
    for name, v in d['files'].items():
        ref({'path': name, **v}, E, p.name)
    freeze_counts[p.name] = len(d['files'])
for name in ['source-C.json', 'C-normal-SDK.json']:
    d = read(B / '.aegis/results/G5-erase-feedback-frame-r1' / name)
    sha_map(d['files'], Path(d['root']), name)
normal_source = read(B / '.aegis/results/G5-erase-feedback-frame-r1/source-C.json')['files']
sha_map(normal_source, B, 'baseline current source')
sha_map(read(E / 'source-B-progress.json'), W, 'probe current source')
d = read(E / 'B-progress-SDK.json')
sha_map(d['files'], Path(d['root']), 'probe SDK')
preservation = read(E / 'preservation-before.json')
for name, d in preservation.items():
    if isinstance(d, dict):
        sha_map(d, W, 'preservation ' + name)

def stat(v):
    return {'count': len(v), 'max': max(v, default=None), 'sum': sum(v)}

raw_rows = []
lifecycle_rows = []
progress_rows = []
for mode in ['ORDER', 'ISOLATED', 'REVERSE', 'PROGRESS-CORRECTIVE']:
    for path in sorted((O / ('web/SNR-D-B-' + mode)).glob('DPR2/**/raw.json')):
        raw = read(path)
        check(raw['status'] == 'OBSERVED' and raw.get('closed'), 'raw not complete ' + str(path))
        check(not raw['errors'], 'SDK errors ' + str(path))
        j = raw['scheduling']
        check(j['dropped'] == 0 and not j['pending'], 'journal drop/pending ' + str(path))
        life = j['lifecycle']
        workers = collections.Counter(v['kind'] for v in life)
        check(workers['worker-create'] == workers['worker-terminate'], 'worker balance ' + str(path))
        callbacks = [v for v in j['raf'] if 'started' in v and 'ended' in v]
        after_close = []
        for i, v in enumerate(life):
            if v['kind'] == 'runtime-close-end':
                next_create = next((q['at'] for q in life[i+1:] if q['kind'] == 'runtime-create-begin'), float('inf'))
                after_close.extend(q for q in callbacks if v['at'] < q['started'] < next_create)
        check(not after_close, 'callback after close ' + str(path))
        lifecycle_rows.append({'path': str(path), 'workersCreated': workers['worker-create'], 'workersTerminated': workers['worker-terminate'], 'callbacksAfterClose': len(after_close), 'pendingAtEnd': j['pending'], 'physicalGpuRelease': 'unobservable'})
        for c in raw['cases']:
            route = c['route']
            primary = next(x for x in c['erases'] if x['label'] == 'primary-long-erase')
            check(sum(x['type'] == 'move' for x in primary['events']) == 480, 'primary input count ' + c['id'])
            check(len(route['inputs']) == 120, 'navigation input count ' + c['id'])
            check([x['index'] for x in route['inputs']] == list(range(1, 121)), 'navigation input order ' + c['id'])
            check(c['dropped'] == 0, 'native drops ' + c['id'])
            check(not c['captures'], 'diagnostic pixel readback ' + c['id'])
            up = primary['events'][-1]['at']
            tail = [x for x in callbacks if up-2000 <= x['started'] < up]
            recovery = [x for x in callbacks if route['inputEnded'] <= x['started'] <= route['stableAt']]
            row = {'mode': mode, 'case': c['id'], 'path': str(path), 'navigationMs': route['inputEnded']-route['plannedDown'], 'convergenceMs': route['stableAt']-route['inputEnded'], 'tailCallbacks': len(tail), 'tailQueueDelayMs': stat([x['started']-x['requestedAt'] for x in tail]), 'recoveryCallbacks': len(recovery), 'recoveryCallbackCpuMs': sum(x['ended']-x['started'] for x in recovery), 'generationBeforeNavigation': route['startState']['generation'], 'primaryMoves': 480, 'navigationMoves': 120}
            raw_rows.append(row)
            check(abs(row['convergenceMs']-route['metrics']['convergenceMs']) < 1e-6, 'stored convergence mismatch')
            if mode != 'PROGRESS-CORRECTIVE':
                continue
            recs = [v for v in c['diagnostic'] if v['marker'] == 'AXIOM_SNR_PROGRESS ' and route['inputEnded'] <= v['at'] <= route['stableAt']]
            pending = {}
            pairs = []
            resets = []
            basis_fields = ['cache', 'job', 'semantic', 'scene', 'camera', 'surface', 'metrics', 'provider', 'target', 'resource', 'target_epoch', 'transform']
            for v in recs:
                if v['phase'] == 'reset-before':
                    resets.append({k:v[k] for k in ['at','job','camera','cursor','reason']})
                if v['phase'] == 'prepare-before':
                    key = (v['cache'], v['job'])
                    check(key not in pending, 'overlapping prepare')
                    pending[key] = v
                if v['phase'] == 'prepare-after':
                    key = (v['cache'], v['job'])
                    a = pending.pop(key, None)
                    check(a is not None, 'unpaired prepare after')
                    if a is None:
                        continue
                    check(all(a[k] == v[k] for k in basis_fields), 'basis changed inside preparation')
                    delta = {k:v[k]-a[k] for k in ['coverage_steps','body_builds','body_tiles','mask_updates','raster_groups']}
                    useful = v['cursor'] > a['cursor'] or any(n > 0 for n in delta.values()) or (v['pending_raster'] == a['pending_raster'] != '0' and [v[k] for k in ['primitive','knot','tile_x','tile_y']] != [a[k] for k in ['primitive','knot','tile_x','tile_y']])
                    complete = a['cursor'] == v['cursor'] == 40 and v['residents'] == 40
                    check(useful or complete, 'unexplained zero-progress ' + c['id'])
                    pairs.append({'job': v['job'], 'camera': v['camera'], 'cursorBefore': a['cursor'], 'cursorAfter': v['cursor'], 'delta': delta, 'useful': useful, 'alreadyComplete': complete})
            check(not pending, 'unpaired prepare before')
            progress_rows.append({'case':c['id'], 'pairs':pairs, 'resets':resets, 'attempts':len(pairs), 'useful':sum(p['useful'] for p in pairs), 'alreadyComplete':sum(p['alreadyComplete'] and not p['useful'] for p in pairs), 'gpuEpochMax':max((x['gpu_epochs'] for x in recs),default=None), 'limitation':'Observed basis and work only; raw does not expose every body/mask pointee identity, cannot reconstruct old failed job.'})

check(len(raw_rows) == 16, 'completed case count')
producer_rows = result['observations']['settledPanDiagnosticCases']
for row in raw_rows:
    other = next(x for x in producer_rows if x['run'] == 'SNR-D-B-' + row['mode'] and x['case'] == row['case'])
    for field in ['navigationMs', 'convergenceMs', 'recoveryCallbacks', 'tailCallbacks']:
        check(abs(row[field] - other[field]) < 1e-6, 'producer/raw mismatch ' + field)

changed_controllers = []
for p in sorted((O / 'controller-v2').iterdir()):
    old = O / p.name
    if old.is_file() and p.is_file() and old.read_bytes() != p.read_bytes():
        changed_controllers.append(p.name)
        check(p.name == 'web.mjs' and p.read_text() == old.read_text().replace('JSON.stringify(v,null,2)', 'JSON.stringify(v)'), 'corrective changed behavior ' + p.name)
failed = O / 'web/SNR-D-B-PROGRESS'
failed_guard = read(failed / 'guard.worker.json')
failed_life = read(failed / 'lifecycle.json')
check(failed_guard['returncode'] == 1 and failed_guard['cleanup_complete'], 'failed guard classification')
check('diagnostic trace byte cap' in json.dumps(failed_life), 'failed trace-cap cause')
partial = read(failed / 'DPR2/raw.json')
check(partial['status'] != 'OBSERVED', 'failed run relabelled')
corrective_bytes = (O / 'web/SNR-D-B-PROGRESS-CORRECTIVE/DPR2/raw.json').stat().st_size
check(corrective_bytes < 64*1024*1024, 'corrective over trace cap')
ledger = read(E / 'sampling-ledger.json')
check(len(ledger['attempts']) == 5 and sum(math.ceil(v['guard']['elapsed_s']) for v in ledger['attempts']) == ledger['chargedSeconds'] == 590, 'sampling charge/count')
for row in ledger['attempts']:
    check(row['guard']['cleanup_complete'] and not row['guard']['remaining_owned_pids'], 'owned cleanup ' + row['id'])
    check(row['guard']['peak_process_family_rss_kib'] <= 6144*1024, 'RSS cap ' + row['id'])
storage = read(E / 'storage-ledger.json')
storage_mismatches = []
storage_classification = collections.Counter()
for name, size in storage['paths'].items():
    p = Path(name)
    if not p.is_file():
        if name.endswith('.partial') and p.with_suffix('').is_file():
            storage_classification['historicalPartialHighwaterRetained'] += 1
        else:
            storage_mismatches.append(name)
    elif name.endswith('.gz'):
        with p.open('rb') as f:
            f.seek(-4, 2)
            decoded = struct.unpack('<I', f.read(4))[0]
        if size < decoded:
            storage_mismatches.append(name)
        else:
            storage_classification['gzipChargedDecodedLength'] += 1
    elif p.stat().st_size > size:
        storage_mismatches.append(name)
    else:
        storage_classification['existingFileWithinHistoricalHighwater'] += 1
check(not storage_mismatches, 'storage path accounting')
check(sum(storage['paths'].values()) == storage['logicalBytes'], 'storage logical sum')
check(storage['peakRetainedBytes'] <= 4*1024**3 and storage['logicalBytes'] <= 8*1024**3, 'storage caps')
buffer_checks = []
for v in read(E / 'buffer-audit.json')['files']:
    p = Path(v['gzipPath'])
    got = digest(p)
    check(got['bytes'] == v['gzipBytes'] and got['sha256'] == v['gzipSHA256'], 'compressed buffer ' + str(p))
    h = hashlib.sha256()
    n = 0
    with gzip.open(p, 'rb') as f:
        while b := f.read(1024 * 1024):
            n += len(b)
            h.update(b)
    check(n == v['rawBytes'] and h.hexdigest() == v['rawSHA256'], 'decoded buffer ' + str(p))
    buffer_checks.append({'path':str(p), 'decodedBytes':n, 'decodedSHA256':h.hexdigest()})

old_path = B / 'out/G5-erase-feedback-frame-r1/web/EFF-W-C1/DPR2/raw.json'
old = read(old_path)
c = next(x for x in old['cases'] if x['id'] == 'settled-pan')
route = c['route']
old_callbacks = route['observation']['rafCallbacks']
old_recovery = [x for x in old_callbacks if route['inputEnded'] <= x['started'] <= route['stableAt']]
primary = next(x for x in c['erases'] if x['label'] == 'primary-long-erase')
up = primary['events'][-1]['at']
old_tail = [x for x in old_callbacks if up-2000 <= x['started'] < up]
old_fact = {'path':str(old_path), **digest(old_path), 'navigationMs':route['inputEnded']-route['plannedDown'], 'convergenceMs':route['stableAt']-route['inputEnded'], 'recoveryCallbacks':len(old_recovery), 'tailCallbacks':len(old_tail), 'generationBeforeNavigation':route['startState']['generation'], 'status':'ORIGINAL_UNMET_UNCHANGED'}

audit = {'at':datetime.datetime.now(datetime.timezone.utc).isoformat(), 'reviewOwner':'CONTROL_REVIEW independent of producer', 'repository':repos, 'package':{'path':str(P/'package.json'),**digest(P/'package.json')}, 'result':{'path':str(E/'result.json'),**digest(E/'result.json')}, 'manifestFileCount':len(manifest['files']), 'manifest':{'path':str(E/'artifact-manifest.json'),**digest(E/'artifact-manifest.json')}, 'freezeCounts':freeze_counts, 'uniqueHashedFiles':len(cache), 'rawRecomputation':raw_rows, 'progress':progress_rows, 'lifecycle':lifecycle_rows, 'harnessCorrection':{'changedFiles':changed_controllers,'onlyLosslessIndentationRemoval':changed_controllers==['web.mjs'],'failedPartialRetained':True,'correctiveBytes':corrective_bytes,'failedStartReturncode':failed_guard['returncode']}, 'sampling':{'starts':5,'chargedSeconds':ledger['chargedSeconds'],'performanceRetries':0,'newControlSampling':0}, 'storage':{k:storage[k] for k in ['peakRetainedBytes','logicalBytes','actualRetainedBytes','retainedCapBytes','logicalCapBytes']}, 'storagePathMismatches':storage_mismatches, 'oldFailure':old_fact, 'failures':failures, 'productionFixExists':False,'originalPerformanceClosed':False}
audit['storageAccounting'] = {'classification':dict(storage_classification),'semantics':'Historical decoded-byte highwater is not a current retained-file inventory; .partial highwater conservatively persists after rename, gzip uses decoded length. No undercharge or cap breach found.', 'bufferChecks':buffer_checks}
(A/'independent-audit.json').write_text(json.dumps(audit,ensure_ascii=False,indent=2)+'\n')
print(json.dumps({'failures':failures,'manifestFiles':audit['manifestFileCount'],'hashedFiles':len(cache),'rawCases':len(raw_rows),'stablePan':[{k:r[k] for k in ['mode','navigationMs','convergenceMs','recoveryCallbacks']}for r in raw_rows if r['case']=='settled-pan'],'progress':[{k:r[k]for k in ['case','attempts','useful','alreadyComplete']}for r in progress_rows],'oldFailure':old_fact},ensure_ascii=False,indent=2))
