"""Validate real Runtime settle trace; elapsed timers never serve as SLO tests."""
import json, sys
from pathlib import Path
rows=[]
for line in Path(sys.argv[1]).read_text().splitlines():
    if line.startswith('AXIOM_FRAME_TIMING '):
        rows.append(json.loads(line.removeprefix('AXIOM_FRAME_TIMING ')))
assert rows, 'no canonical frame timing records (instrumentation missing)'
settle=[r for r in rows if r['settle']]
assert len(settle)>1, 'real exact preparation retry was not observed'
assert any(r['outcome']=='retry' for r in settle), 'retry missing'
assert settle[-1]['outcome']=='accepted', 'final exact submit missing'
for r in rows:
    assert all(isinstance(r[k],str) and r[k].isdigit() for k in ['frame','semantic','scene','camera','surface','metrics'])
    stages=r['stages']
    assert 'attempt' in stages
    total=stages['attempt']['inclusive_ns']
    exclusive=sum(x['exclusive_ns'] for x in stages.values())
    assert exclusive==total, (exclusive,total)
    assert all(0<=x['exclusive_ns']<=x['inclusive_ns']<=total and x['calls']>0 for x in stages.values())
for r in settle:
    for name in ['visibility','reference_build','reference_encode','reference_digest','frame_plan','render','cache_prepare']:
        assert r['stages'].get(name,{}).get('calls',0)>0,(name,r['frame'])
    if r['outcome']=='retry':
        assert 'provider_present' not in r['stages'], 'partial work was presented'
assert settle[-1]['stages']['provider_present']['calls']==1
assert all(r['counts']['sources']==r['counts']['plan_entries'] for r in settle), 'source/plan copy observation mismatch'
assert sum(r['counts']['built_groups'] for r in settle)>0, 'no actual preparation work observed'
assert sum(r['counts']['coverage_tiles'] for r in settle)>0, 'no CPU coverage work measured'

print(f"{len(rows)} frame rows; {len(settle)} settle attempts; nested accounting and retry/present isolation valid")
