"""Verify an intentionally blocked engineering result; never issue Gate PASS."""
from pathlib import Path
import hashlib,gzip,json,xml.etree.ElementTree as ET
R=Path(__file__).resolve().parent
read=lambda p:json.loads(gzip.decompress(p.read_bytes())if p.suffix=='.gz'else p.read_bytes())
i=read(R/'SHA256.json');files={str(p.relative_to(R))for p in R.rglob('*')if p.is_file()and p.name!='SHA256.json'}
assert files==set(i)
for n,h in i.items():assert hashlib.sha256((R/n).read_bytes()).hexdigest()==h,n
r=read(R/'result.json');assert r['status']=='BLOCKED_ENDPOINT'and r['issueSolved']is False and r['officialGatePassClaimed']is False
n=read(R/'checks/native-comparison.json');p=read(R/'checks/prior-native-comparison.json')
x=ET.fromstring(gzip.decompress((R/'checks/native-results.xml.gz').read_bytes()));failed=sorted(t.attrib['name']for t in x.findall('testcase')if t.find('failure')is not None or t.find('skipped')is not None)
assert failed==n['failed_names']and int(x.attrib['tests'])==n['total']and n['passed']==n['total']-len(failed)
assert not n['new_failed_names']and n['failed_names']==p['failed_names']and not n['binariesChangedDuringRun']
assert n['passed']==p['passed']+1 and n['total']==p['total']+1
assert r['native']['passed']==n['passed']and r['native']['total']==n['total']
summary=read(R/'summary.json')
for name in ['red','off']:
 folder=R/'raw'/name;w=read(folder/'worker.json');b=read(folder/'outer-before.json.gz');a=read(folder/'outer-after.json.gz');inputs=read(folder/'inputs.json.gz');setup=read(folder/'setup.json.gz')
 assert a==b and not b['source']['dirty']and b['source']['commit']==r['fixture_revision']
 assert inputs['sourceBefore']==b['source']and inputs['artifactBefore']==b['artifacts']
 assert inputs['rounds']==10 and inputs['fixture']['erasePoints']==128 and inputs['profiled']is False
 assert setup['state']['objects']==16 and setup['build']['dirty']is False
 assert setup['build']['sourceRevision']==(r['baseline_code_revision']if name=='red'else r['code_revision'])
 assert w['cleanup_complete']and not w['remaining_owned_pids']and w['peak_process_family_rss_kib']<=w['rss_limit_kib']
 assert w['reason']in ['timeout','nonzero_exit']
 rows=summary['rows'][name];assert len(rows)<10 or any(x['p95_frame_ms']>=150 for x in rows)
 assert rows and len(rows)<=10
 for row in rows:
  v=read(folder/('erase-'+str(row['round']-1)+'.json.gz'));values=sorted(s['frameMs']for s in v['samples'])
  assert len(values)==128 and values[int(len(values)*.95)]==row['p95_frame_ms']
  assert all(value for key,value in v['checks'].items()if key!='oneCommit')
for mode in ['live-erase-gpu-miss','regional-mask','v4-gpu','masked-progress','masked-mixed','affine-erased']:
 d=read(R/'checks'/(mode+'-gpu.json'));assert d['status']=='PASS'and d['result']['code']==0 and not d['pageErrors']
d=read(R/'checks/new-case-red.json');assert d['status']=='FAIL'and any('v4 live erase cache miss silently used CPU coverage'in e for e in d['result']['errors'])
for name in ['unqualified-gpu-matrix.json','unqualified-gpu-v3-focus.json']:
 d=read(R/'checks'/name);assert d['status']=='FAIL'and any('stale erase target was partially reused'in e for e in d['result']['errors'])
v=read(R/'navigation-regression/observation.json.gz');w=read(R/'navigation-regression/worker.json')
assert v['sourceStable']and v['artifactStable']and not v['sourceBefore']['dirty']and v['status']=='MEASURED'
assert v['setup']['build']['sourceRevision']==r['code_revision']and not v['errors']
assert all(all(p['raw']['checks'].values())for p in v['phases']+v['erasures'])and v['close']['closed']
assert w['reason']=='completed'and w['cleanup_complete']
assert '# pass 42'in(R/'checks/node.log').read_text()and '# fail 0'in(R/'checks/node.log').read_text()
assert '119 contract files'in(R/'checks/boundary.log').read_text()
assert all(x['unchangedFromAnchor']for x in read(R/'execution-inputs.json').values())
assert all(len(x['files'])==36 for x in read(R/'packages.json').values())
print(json.dumps({'status':'VERIFIED_BLOCKED_ENGINEERING_RESULT','files':len(i),'native_passed':n['passed'],'native_total':n['total'],'completed_rounds':{k:len(v)for k,v in summary['rows'].items()},'issueSolved':False,'officialGatePassClaimed':False}))
