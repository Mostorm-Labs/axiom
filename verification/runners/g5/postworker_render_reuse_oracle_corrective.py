from pathlib import Path
import json,hashlib,gzip,importlib.util,time
root=Path.cwd();out=root/'out/G5-postworker-render-reuse-r1';res=root/'.aegis/results/G5-postworker-render-reuse-r1';oracle=root/'out/G5-CPUW3-complete-interaction-r2/cpu-reference';correction=root/'.aegis/results/G5-postworker-render-reuse-control-r1/oracle-driver-correction.json'
sha=lambda p:hashlib.sha256(Path(p).read_bytes()).hexdigest()
if (res/'oracle-corrective-ledger.json').exists():raise SystemExit('corrective starts already recorded; no repeat allowed')
assert sha(oracle)=='c48035bb2c522c3a854c8552e30d147113c75065f8f2fde6315b0e2cda181603'
spec=importlib.util.spec_from_file_location('bounded',root/'verification/runners/g5/bounded_worker.py');module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)
ledger={'correction':str(correction.relative_to(root)),'correction_sha256':sha(correction),'prior_attempts':'oracle-ledger.json (unchanged)','starts_max':2,'seconds_each':180,'seconds_total':360,'RSS_MiB':2048,'attempts':[]};ledgerpath=res/'oracle-corrective-ledger.json'
for row in ['append-zoom-out-immediate','partial-erase-zoom-out-immediate']:
 d=out/'oracle-corrective'/row;d.mkdir(parents=True,exist_ok=True);original=out/'oracle'/row/'pixels-request.json';q=json.loads(original.read_text());inputpath=d/'input.axsnap';z=Path(q['snapshot']+'.gz');data=gzip.decompress(z.read_bytes());inputpath.write_bytes(data);assert sha(inputpath)==hashlib.sha256(data).hexdigest()
 q['snapshot']=str(inputpath);request=d/'request.json';request.write_text(json.dumps(q,indent=2)+'\n');attempt={'row':row,'status':'STARTED','original_request':str(original.relative_to(root)),'original_request_sha256':sha(original),'request':str(request.relative_to(root)),'request_sha256':sha(request),'rehydrated_snapshot_sha256':sha(inputpath),'basis_change':'pathonly/samebytes; all otherrequestvalues unchanged'};ledger['attempts'].append(attempt);ledgerpath.write_text(json.dumps(ledger,indent=2)+'\n')
 guard=module.run([str(oracle),str(request),str(d/'original')],d/'reference',180,2048*1024);attempt.update(status='COMPLETED'if guard['reason']=='completed'and guard['cleanup_complete']else'STOPPED',guard=guard,output_prefix=str((d/'original').relative_to(root)))
 ledgerpath.write_text(json.dumps(ledger,indent=2)+'\n');print(json.dumps(attempt),flush=True)
 if attempt['status']!='COMPLETED':break
