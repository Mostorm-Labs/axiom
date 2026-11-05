"""Derive readable paired results without trimming phases or tails."""
import gzip,json
from pathlib import Path
base=Path(__file__).resolve().parent
out=Path('out')
result={}
for mode in ['on','off','optical']:
    directory=out/('g5-nav-quality-matrix-'+mode)
    if not (directory/'comparison.json').exists():continue
    comparison=json.loads((directory/'comparison.json').read_text())
    cases=[]
    for c in comparison['comparisons']:
        case={'case':c['case'],'initialPixelsEqual':c.get('initialPixelsEqual'),'objects':c.get('objects'),'selectedKinds':c.get('selectedKinds'),'phases':[]}
        for p in c.get('phases',[]):
            row={'id':p['id'],'pixelsEqual':p['pixelsEqual']}
            for side in ['before','after']:
                s=p[side];nav=s['navigation']
                row[side]={'inputGapP95':s['input_display_submission_gap_ms']['p95'],'inputGapMax':s['input_display_submission_gap_ms']['max'],
                    'settleMs':s['settle_ms'],'exactCpuMs':s['exactStages'].get('attempt',{}).get('inclusive_ms'),
                    'navigationCpuP95':nav['cpu_ms']['p95'] if nav else None,'quality':nav.get('quality') if nav else None}
            case['phases'].append(row)
        if mode=='optical':
            before=json.loads(gzip.decompress((directory/(c['case']+'-before')/'wheel.json.gz').read_bytes()))
            after=json.loads(gzip.decompress((directory/(c['case']+'-after')/'wheel.json.gz').read_bytes()))
            case['optical']=[{'id':a['id'],'samples':[{'index':x['index'],'sameCamera':x['camera']==y['camera'],
                'beforeEdgeRms':x['edgeRms'],'afterEdgeRms':y['edgeRms'],'sameChecksum':x['checksum']==y['checksum']}
                for x,y in zip(a['raw']['optical'],b['raw']['optical'])]} for a,b in zip(before['phases'],after['phases'])]
        cases.append(case)
    result[mode]=cases
(base/'paired-results.json').write_text(json.dumps(result,indent=2)+'\n')
for mode,cases in result.items():
    print(mode)
    for c in cases:
        print(c['case'],'objects',c['objects'],'initialPixels',c['initialPixelsEqual'])
        for p in c['phases']:
            a,b=p['before'],p['after'];q=b['quality'];print(' ',p['id'],'gapMax',round(a['inputGapMax'],1),round(b['inputGapMax'],1),
                'settle',round(a['settleMs'],1),round(b['settleMs'],1),'pixels',p['pixelsEqual'],
                'low',None if not q else round(q['timeWeightedLowResolutionFraction'] or 0,3))
