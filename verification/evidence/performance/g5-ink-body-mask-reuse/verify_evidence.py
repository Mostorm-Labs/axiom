"""Validate the archived blocked engineering result, never issue Gate PASS."""
from pathlib import Path
import gzip,hashlib,json,xml.etree.ElementTree as ET
R=Path(__file__).resolve().parent
read=lambda p:json.loads(gzip.decompress(p.read_bytes()) if p.suffix=='.gz' else p.read_bytes())
index=read(R/'SHA256.json');files={str(p.relative_to(R)) for p in R.rglob('*') if p.is_file() and p.name!='SHA256.json'}
assert files==set(index)
for n,h in index.items():assert hashlib.sha256((R/n).read_bytes()).hexdigest()==h,n
result=read(R/'result.json');summary=read(R/'summary.json');packages=read(R/'packages.json');outer=read(R/'outer-after.json')
revision='ce5425e08d37384189eef298a7227126ff21ce17'
assert result['status']=='BLOCKED_ENDPOINT' and not result['issueSolved'] and not result['officialGatePassClaimed']
assert result['code_revision']==revision and result['defaultBudgetMiB']=={'cache':32,'shared':96}
assert outer['source']['commit']==revision and not outer['source']['dirty']
prior=read(R/'checks/prior-native-comparison.json');native=read(R/'checks/native-comparison.json');x=ET.fromstring(gzip.decompress((R/'checks/native-results.xml.gz').read_bytes()))
cases=list(x.iter('testcase'));failed=sorted(c.attrib['name'] for c in cases if c.find('failure') is not None or c.find('skipped') is not None or c.attrib.get('status') in ('fail','notrun'))
assert native['sourceRevision']==revision and sorted(native['failed_names'])==failed==sorted(prior['failed_names'])
assert native['total']==len(cases)==1084 and native['passed']==len(cases)-len(failed)==1018
assert not native['new_failed_names'] and not native['binariesChangedDuringRun']
a=read(R/'checks/native-binaries-before.json');b=read(R/'checks/native-binaries-after.json');assert a==b and len(a)==native['binaryCount']
assert '# pass 42' in (R/'checks/node.log').read_text() and '# fail 0' in (R/'checks/node.log').read_text()
assert '119 contract files' in (R/'checks/boundaries.log').read_text()
probe=None
for mode in ['body-mask-only','regional-mask-only','live-erase-gpu-miss-only','v4-gpu-only','masked-progress-only','masked-mixed-only','affine-erased-only']:
 d=read(R/('checks/gpu-'+mode+'.json'));assert d['status']=='PASS' and d['result']['code']==0 and not d['pageErrors'] and d['gl']
 current={k:d[k] for k in ['g5_coverage_probe.js','g5_coverage_probe.wasm']}
 if probe is None:probe=current
 assert current==probe
assert probe==result['testedGpuProbe']
red=read(R/'checks/gpu-epoch-red.json');assert red['status']=='FAIL' and 'std::bad_alloc' in red['result']['errors']
assert 'rebuilt immutable body geometry' in (R/'checks/body-sharing-red.log').read_text()
assert 'repeated historical mask subtraction' in (R/'checks/exact-up-red.log').read_text()
assert 'rebuilt unchanged mask paths' in (R/'checks/exact-path-cache-red.log').read_text()
red=read(R/'checks/live-regional-navigation-red.json');assert red['status']=='FAIL' and 'local erasures discarded unchanged wide-view pixels before first navigation' in red['result']['errors']
red=read(R/'checks/live-reclaim-red.json');assert red['status']=='FAIL' and 'live resource denial did not request navigation reclamation' in red['result']['errors']
assert read(R/'checks/unqualified-gpu-v3-focus.json')['status']=='FAIL'
sequences=[]
for name in ['off-32-96','on-32-96','off-32-192','off-64-192']:
 folder=R/'raw'/name;inputs=read(folder/'inputs.json.gz');setup=read(folder/'setup.json.gz');w=read(folder/'worker.json');row=summary['rows'][name]
 assert inputs['sourceBefore']==outer['source'] and inputs['sourceBefore']['commit']==setup['build']['sourceRevision']==revision
 assert not setup['build']['dirty'] and not inputs['sourceBefore']['dirty']
 assert inputs['rounds']==10 and inputs['fixture']['erasePoints']==128 and inputs['fixture']['rawSamples']==7200 and inputs['fixture']['duplicates']==15 and inputs['dpr']==2 and not inputs['profiled']
 assert setup['state']['objects']==16 and setup['config']['family']==1 and setup['config']['roundMode']==2
 assert inputs['artifactBefore']==packages[name]['files']==outer['artifacts'][name]
 assert w==row['worker'] and w['cleanup_complete'] and not w['remaining_owned_pids'] and w['peak_process_family_rss_kib']<=w['rss_limit_kib']
 rows=row['rows'];assert len(rows)==row['rounds']
 ids=[]
 for n,v in enumerate(rows):
  raw=read(folder/('erase-'+str(n)+'.json.gz'));values=sorted(s['frameMs'] for s in raw['samples'])
  assert len(values)==128 and v['p95FrameMs']==values[int(.95*len(values))] and v['maxFrameMs']==values[-1] and v['upMs']==raw['upMs']
  assert all(t for key,t in raw['checks'].items() if key!='oneCommit')
  ids.append(raw['after']['lastCommit']['operationId'])
 assert row['maxP95FrameMs']==max(v['p95FrameMs'] for v in rows) and row['maxFrameMs']==max(v['maxFrameMs'] for v in rows) and row['maxUpMs']==max(v['upMs'] for v in rows)
 sequences.append(ids)
 if row['missingFinalObservation']:
  assert not (folder/'observation.json.gz').exists() and w['reason']=='timeout' and len(rows)<10 and row['status']=='BLOCKED_ENDPOINT' and row['runnerStatus'] is None
 else:
  d=read(folder/'observation.json.gz');assert d['sourceStable'] and d['artifactStable'] and not d['errors'] and d['close']['closed'] and not d['close']['errors']
  assert d['sourceBefore']==d['sourceAfter']==outer['source'] and d['artifactBefore']==d['artifactAfter']==packages[name]['files']
  assert not d['captureSnapshots'] and not d['profiled'] and len(d['erasures'])==len(rows)==10
  assert all(all(v['raw']['checks'].values()) for v in d['phases'])
  expected='MEASURED' if all(v['p95FrameMs']<150 for v in rows) else 'BLOCKED_ENDPOINT'
  assert d['status']==row['status']==row['runnerStatus']==expected
  assert (w['reason']=='completed')==(expected=='MEASURED')
  assert w['elapsed_s']<180
 for v in row['rows']:assert v['checks']
assert all(ids==sequences[-1][:len(ids)] for ids in sequences)
assert summary['rows']['off-64-192']['status']=='MEASURED' and summary['rows']['off-32-96']['status']=='BLOCKED_ENDPOINT'
d=read(R/'navigation-regression/observation.json.gz');w=read(R/'navigation-regression/worker.json')
assert d['sourceStable'] and d['artifactStable'] and not d['sourceBefore']['dirty'] and not d['setup']['build']['dirty'] and d['setup']['build']['sourceRevision']==revision
assert d['status']=='MEASURED' and d['fixture']['erasePoints']==25 and len(d['erasures'])==2 and not d['errors']
assert all(all(v['raw']['checks'].values()) for v in d['phases']+d['erasures']) and w['reason']=='completed' and w['cleanup_complete']
assert all(v['feedback']['contentGapMs']['max']<300 for v in d['phases'][1:])
assert all(f['unchangedFromAnchor'] for f in read(R/'execution-inputs.json').values())
# Preserve earlier failures separately; never upgrade them using final-code data.
assert read(R/'prior-a620/nav/observation.json.gz')['status']=='BLOCKED_ENDPOINT'
assert read(R/'prior-a497/off-32-96/worker.json')['reason']=='timeout'
print(json.dumps({'status':'VERIFIED_BLOCKED_ENGINEERING_RESULT','files':len(index),'native':[native['passed'],native['total']],'qualifiedGpuModes':7,'profiles':{k:{'status':v['status'],'rounds':v['rounds']} for k,v in summary['rows'].items()},'officialGatePassClaimed':False}))
