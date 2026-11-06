#!/usr/bin/env python3
"""Bounded retrospective audit;never reconstructs unknown event timestamps."""
import argparse,gzip,json,hashlib
from pathlib import Path

def main():
 p=argparse.ArgumentParser();p.add_argument('evidence',type=Path);p.add_argument('output',type=Path);a=p.parse_args();rows=[]
 for source in sorted((a.evidence/'runs').glob('*/wheel.json.gz')):
  raw=json.loads(gzip.decompress(source.read_bytes()))
  for phase in raw['phases']:
   v=phase['raw'];shown=[x for x in v['displayTimes']if v['started']<=x<=v['inputEnded']]
   times=[v['started'],*shown,v['inputEnded']];start,end=max(zip(times,times[1:]),key=lambda x:x[1]-x[0])
   earliest=v['samples'][0]['at']-v['samples'][0]['interval']if v['samples']else None
   initial=start==v['started'];overlap=sum(max(0,min(end,x['start']+x['duration'])-max(start,x['start']))for x in v['longTasks'])
   rows.append({'run':source.parent.name,'phase':phase['id'],'legacyMaxGapMs':end-start,'gapStart':start,'gapEnd':end,
    'isPhasePrefix':initial,'preInputWaitLowerBoundMs':None if earliest is None else max(0,earliest-v['started']),
    'firstColorResponseUpperBoundMs':None if not initial or earliest is None else max(0,end-earliest),
    'longTaskOverlapMs':overlap,'exactInputTimestampAvailable':False,'canonicalWaitMs':phase['summary']['settle_ms'],
    'nearbyRafSamples':[x for x in v['samples']if start-25<x['at']<end+25]})
 result={'scope':'Read-only audit of immutable legacy evidence;bounds only,not corrected v2 measurements or changed verdicts.',
 'sourceManifestSha256':hashlib.sha256((a.evidence/'manifest.json').read_bytes()).hexdigest(),'rows':rows}
 a.output.write_text(json.dumps(result,indent=2)+'\n')
if __name__=='__main__':main()
