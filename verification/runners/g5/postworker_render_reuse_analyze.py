from pathlib import Path
import json,gzip,hashlib,math
root=Path.cwd();out=root/'out/G5-postworker-render-reuse-r1';res=root/'.aegis/results/G5-postworker-render-reuse-r1'
def sha(b):return hashlib.sha256(b).hexdigest()
def read_raw(p):return json.loads(gzip.decompress(p.read_bytes())if p.suffix=='.gz'else p.read_bytes())
rows=[]
for d in sorted((out/'web').glob('RR-D[14]-*')):
 if not d.is_dir():continue
 p=d/'raw.json';p=p if p.exists()else d/'raw.json.gz'
 if not p.exists():continue
 r=read_raw(p);ev=r.get('events',[]);a_up=next((e for e in ev if e.get('phase')=='up'),None)
 afterA=next((i for i,e in enumerate(ev)if e.get('phase')=='up'),None);b_down=ev[afterA+1]if afterA is not None and len(ev)>afterA+1 else None
 c_up=r.get('stages',{}).get('C-Up',{}).get('at');c_end=r.get('stages',{}).get('followup-erase',{}).get('at')
 worker=r.get('worker',{});jobs=worker.get('jobs',[])
 row={'id':d.name,'status':r.get('status'),'input_down_to_up_ms':r.get('A_down_to_up_ms'),'AB_up_to_drain_ms':r.get('up_to_drain_ms'),'C_down_to_drain_ms':r.get('followup_erase_ms'),'C_up_to_drain_ms':c_end-c_up if c_up and c_end else None,'A_Up_handler_ms':a_up['end']-a_up['at']if a_up else None,'B_Down_handler_ms':b_down['end']-b_down['at']if b_down else None,'B_Down_dispatch_after_A_Up_ms':b_down['at']-a_up['end']if b_down and a_up else None,'first_A_canonical_notification_ms':r.get('first_A_present_ms'),'first_AB_canonical_notification_ms':r.get('first_AB_present_ms'),'operation_notification_A_ms':r.get('first_A_commit_ms'),'masked_selection_ms':r.get('masked_selection_ms'),'Worker_jobs':len(jobs),'errors':r.get('errors',[]),'closed':r.get('closed'),'native_span_available':False,'raw':str(p.relative_to(root)),'sha256':sha(p.read_bytes()),'feedback_qualification':'onFrame canonical presented-generation notification; no earliest subsequent displayed-frame pixel witness in primary'}
 for name in ['AB','undo','redo','followup']:
  snap=d/(name+'.axsnap.gz');row.setdefault('snapshot_SHA256',{})[name]=sha(gzip.decompress(snap.read_bytes()))if snap.exists()else None
 rows.append(row)
result={'classification':'OBSERVATIONS_PENDING'if len(rows)<3 else'FINITE_MATCHED_OBSERVATIONS','rows':rows,'cautions':['Input variation is real, do not treat relative CPU/GPU work as wall performance without matched evidence.','onOperation is a notification, not atomic commit timestamp. Ordinary build has no native spans.','C_down_to_drain differs from C_up_to_drain.','EarlyGL is proxy, no physical input-to-photon claim.']}
if any(r['id']=='RR-D4-BASELINE'for r in rows)and any(r['id']=='RR-D4-CANDIDATE'for r in rows):
 b=next(r for r in rows if r['id']=='RR-D4-BASELINE');c=next(r for r in rows if r['id']=='RR-D4-CANDIDATE');result['paired_D4']={k:{'baseline':b[k],'candidate':c[k],'change_percent':(c[k]/b[k]-1)*100 if b[k]else None}for k in ['input_down_to_up_ms','AB_up_to_drain_ms','C_down_to_drain_ms','C_up_to_drain_ms']if b.get(k)is not None and c.get(k)is not None}
(res/'paired-performance.json').write_text(json.dumps(result,indent=2)+'\n');print(json.dumps(result,indent=2))
