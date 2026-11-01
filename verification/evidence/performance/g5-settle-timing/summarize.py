"""Sum exclusive stages only: inclusive parents are useful drill-down, not totals."""
import collections,json,sys
from pathlib import Path
results=[]
for folder in sys.argv[1:]:
 p=Path(folder);o=json.loads((p/'observation.json').read_text());rows=[r for r in o['timingRows'] if r['settle']];total=sum(r['stages']['attempt']['inclusive_ns'] for r in rows);stages=collections.defaultdict(lambda:{'inclusive_ns':0,'exclusive_ns':0,'calls':0});counts=collections.Counter()
 for r in rows:
  for name,v in r['stages'].items():
   for key,value in v.items():stages[name][key]+=value
  counts.update(r['counts'])
 first=rows[0] if rows else None;last=rows[-1] if rows else None
 span=(last['start_ns']+last['stages']['attempt']['inclusive_ns']-first['start_ns'])/1e6 if rows else 0
 summary={'preparation_span_ms':span,'between_attempts_ms':span-total/1e6,'cpu_coverage_percent':100*stages['cpu_coverage']['exclusive_ns']/total if total else 0,'count':o['count'],'dpr':o['dpr'],'reverse':o['reverse'],'attempts':len(rows),'retries':sum(r['outcome']=='retry' for r in rows),'accepted':sum(r['outcome']=='accepted' for r in rows),'cpu_attempt_ms':total/1e6,'exact_receipt_delay_ms':o['exactReceiptDelayMs'],'counts':dict(counts),'stages':{n:{'inclusive_ms':v['inclusive_ns']/1e6,'exclusive_ms':v['exclusive_ns']/1e6,'exclusive_percent':100*v['exclusive_ns']/total if total else 0,'calls':v['calls']} for n,v in sorted(stages.items(),key=lambda x:-x[1]['exclusive_ns'])},'image':o['image']}
 assert sum(v['exclusive_ns'] for v in stages.values())==total,'nested accounting mismatch'
 (p/'summary.json').write_text(json.dumps(summary,indent=2)+'\n');results.append(summary)
print(json.dumps(results,indent=2))
