import test from 'node:test';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
let api={};try{api=await import('../runners/g5/ur0/metrics_compiler.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
let proofApi={};try{proofApi=await import('../runners/g5/ur0/canonical_chain.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
const meta={run_id:'MODEL-r1',row_id:'MODEL',repetition:1,fixture_sha256:'a'.repeat(64),record_origin:'SCHEMA_EXAMPLE'};
const base=()=>({status:'OBSERVED',source_stable:true,artifacts_stable:true,source_before:{revision:'1'.repeat(40)},artifacts_before:{},init:{environment:{reported_resolution_ns:'1'}},trace:{loss:{critical:0,statistical:0}},calibration:{status:'QUALIFIED',offset_ns:'0',error_bound_ns:'0'},measurement:{origin_ns:'100',end_ns:'1000',input_end_ns:'600',quiescence_ns:'900',before:{generation:'8'},after:{generation:'9',activeContacts:0},planned:[{event_id:'E0-0',interaction_id:'E0',kind:'pointer',phase:'down',diagnostic_event:'1',planned_ns:'100',producer_ns:'110'}],handlers:[{event_id:'E0-0',enter_ns:'120',exit_ns:'180'}],errors:[],rafTimes:[]}});
const row=(kind,tag,values,event='1',job='10',time='150')=>({kind,tag,values:values.map(String),event,job,basis:job,attempt:job,route:1,purpose:2,seq:'1',at_ns:time});
test('actual metric document preserves rejected feedback and validates frozen shape/semantics',()=>{
 assert.equal(typeof api.compileMetrics,'function','full actual metric compiler missing');
 const doc=api.compileMetrics(base(),[row(4,28,[0,0])],meta).document;
 assert.equal(doc.interactions.find(x=>x.interaction_id==='E0').events[0].admission,'REJECTED');
 assert.equal(doc.metrics.find(x=>x.metric_id==='FIRST_FEEDBACK'&&x.event_id==='E0-0').status,'FAILED');
 assert.notEqual(doc.run_outcome,'COMPLETE');
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(doc)});
});
test('refresh callbacks before measurement origin do not create negative metric timestamps',()=>{
 const run=base();run.measurement.rafTimes=[{observed_ns:'90'},{observed_ns:'110'},{observed_ns:'200'}];
 const doc=api.compileMetrics(run,[],meta).document;
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(doc)});
 assert.equal(doc.metrics.filter(m=>m.metric_id==='RAF_INTERVAL').length,1);
});
test('post-commit input error keeps the operation and does not invent exact publication time',()=>{
 const run=base();run.measurement.planned[0].phase='up';run.measurement.handlers[0].exit_ns='500';
 const rows=[row(5,1,[11,1,2,0,0,4,7,1]),row(6,27,[0,0,1,0,8,9,4,6],'1','10','300'),row(16,28,[0,2,1,1,9],'1','10','400')];
 const compiled=api.compileMetrics(run,rows,meta),i=compiled.document.interactions.find(x=>x.interaction_id==='E0');
 assert.equal(i.commit.operation_id,'01000000000000000000000000000000');assert.equal(i.commit.return_class,'POST_COMMIT_ERROR');
 const up=compiled.document.metrics.find(x=>x.metric_id==='UP_TO_COMMIT');assert.equal(up.status,'MISSING');assert.equal(up.value_ns,null);
 assert.match(up.reason,/upper.bound|publication/);
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(compiled.document)});
});
test('same canonical demand groups different frame attempts without changing their actual results',()=>{
 const rows=[];
 for(const [job,frame,code] of [['20','23',2],['21','24',0]])rows.push(row(5,1,[11,1,2,0,0,4,7,1],'0',job),row(2,10,[job,2,0,200],'0',job,'200'),
  row(5,26,[1,frame,9,44,4,2,3,8,67108864,201326592],'0',job,'220'),row(3,10,[job,300,100],'0',job,'300'),row(16,10,[job,1,code],'0','0','310'));
 const jobs=api.compileMetrics(base(),rows,meta).document.jobs;
 assert.equal(jobs.length,1);assert.equal(jobs[0].attempts.length,2);assert.equal(jobs[0].attempts[0].outcome,'Pending');
 assert.equal(jobs[0].attempts[1].outcome,'Submitted');assert.equal(jobs[0].final_state,'RIGHT_CENSORED');
});
function repeatedCanonical(){
 const run=base();run.measurement.planned[0].phase='up';
 const rows=[row(5,1,[11,1,2,0,0,4,7,1]),row(6,27,[0,0,1,0,8,9,4,6],'1','10','170'),row(7,9,[9,9,44],'1','10','169')];
 for(let n=0;n<24;++n){const job=String(20+n),start=200+n*10;
  rows.push(row(5,1,[11,1,2,0,0,4,7,1],'0',job,String(start)),row(2,10,[job,2,0,start],'0',job,String(start)),
   row(5,26,[1,23,9,44,4,2,3,8,67108864,201326592],'0',job,String(start+1)));
  if(n===23)rows.push(row(8,24,[500,1,2,1],'0',job,String(start+2)),row(9,24,[500,1,222,2,128,128,4],'0',job,String(start+3)),row(11,19,[1,23,9,44,2,3,0],'0',job,String(start+4)));
  rows.push(row(3,10,[job,start+6,6],'0',job,String(start+6)),row(16,10,[job,0,n===23?1:0],'0','0',String(start+7)));
 }
 rows.push(row(5,26,[10,11,1,2,0,0,4,6,7,44,4],'0','10','450'),row(11,29,[10,1,23,9,9,2,3],'0','10','451'),row(12,29,[1,10,11,1,2,1,23,9,9,2,3],'0','10','452'));
 rows.forEach((r,n)=>r.seq=String(n+1));
 return{run,rows};
}
test('repeated-frame canonical metrics keep all attempts and only the actual successful receipt',()=>{
 const {run,rows}=repeatedCanonical(),doc=api.compileMetrics(run,rows,meta).document,i=doc.interactions.find(i=>i.interaction_id==='E0'),job=doc.jobs.find(j=>j.purpose==='canonical');
 assert.equal(i.submits.length,1);assert.equal(i.canonical_receipts.length,1);assert.equal(job.final_state,'Completed');
 const calls=job.attempts.filter(a=>a.attempt_id.startsWith('call-'));assert.equal(calls.length,24);assert.equal(calls.filter(a=>a.outcome==='Submitted').length,1);
 assert.equal(calls.filter(a=>a.outcome==='MISSING').length,23,'false boolean returns cannot be relabeled Pending');
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(doc)});
});
test('canonical admission proof reconciles real raw completions, decoded links and metric receipts',()=>{
 assert.equal(typeof proofApi.canonicalChainProof,'function','canonical positive-chain admission proof missing');
 const {run,rows}=repeatedCanonical(),compiled=api.compileMetrics(run,rows,meta);
 const good=proofApi.canonicalChainProof(rows,compiled.decoded,compiled.document);
 assert.equal(good.status,'QUALIFIED');assert.equal(good.accepted_raw_count,1);assert.equal(good.qualified_decoded_count,1);assert.equal(good.metric_receipt_count,1);
 assert.equal(good.chains[0].draw_job,'43');assert.equal(good.chains[0].attempt_jobs.length,24);
 const dropped=structuredClone(compiled.decoded);dropped.qualified_completions=[];
 assert.equal(proofApi.canonicalChainProof(rows,dropped,compiled.document).status,'MISSING');
 const foreign=structuredClone(compiled.document);foreign.interactions.find(i=>i.canonical_receipts.length).canonical_receipts[0].basis.attachment_epoch='99';
 assert.equal(proofApi.canonicalChainProof(rows,compiled.decoded,foreign).status,'MISSING');
 const noReceipt=structuredClone(compiled.document);for(const i of noReceipt.interactions)i.canonical_receipts=[];
 assert.equal(proofApi.canonicalChainProof(rows,compiled.decoded,noReceipt).status,'MISSING');
});
test('transient feedback uses its own trace demand and full provider submit',()=>{
 const run=base();run.measurement.handlers[0].exit_ns='300';
 const rows=[row(5,1,[11,1,2,0,0,4,7,1]),row(4,28,[1,0]),row(1,37,[7,1,4,8,44,2,3],'1','10','200'),
  row(5,1,[11,1,2,0,0,4,7,1],'0','30'),row(2,11,[30,3,0,250],'0','30','250'),row(5,37,[7,1,4,8,44,2,3],'0','30','255'),row(5,11,[1,15,8,44,4,2,3,8],'0','30','260'),
  row(8,24,[500,6,2,1],'0','30','270'),row(9,24,[500,6,222,2,128,128,4],'0','30','271'),row(11,19,[1,15,8,44,2,3,0],'0','30','400'),row(3,11,[30,450,200],'0','30','450')];
 const doc=api.compileMetrics(run,rows,meta).document;
 assert.equal(doc.jobs.filter(j=>j.purpose==='edit_feedback').length,1);
 assert.equal(doc.metrics.find(m=>m.metric_id==='FIRST_FEEDBACK'&&m.event_id==='E0-0').value_ns,'280');
 assert.equal(doc.interactions.find(i=>i.interaction_id==='E0').canonical_receipts.length,0);
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(doc)});
});
test('native navigation feedback has pose/source/region proof and no canonical receipt',()=>{
 const run=base(),scale='4607182418800017408';
 const rows=[row(5,1,[11,1,2,0,0,4,7,1]),row(1,41,[4,8,44,2,3,scale,0,0],'0','10','200'),
  row(5,1,[11,1,2,0,0,4,7,1],'0','40'),row(5,41,[4,8,44,2,3,scale,0,0],'0','40','250'),row(5,41,[1,8,44,2,3,8,scale,0,0],'0','40','260'),
  row(8,24,[501,12,2,1],'0','40','270'),row(9,24,[501,12,223,2,128,128,4],'0','40','271'),row(11,12,[1,44,8,2,3,scale,0,0,0,1],'0','40','400')];
 const doc=api.compileMetrics(run,rows,meta).document,nav=doc.interactions.find(i=>i.kind==='navigation'&&i.submits.length);
 assert.ok(nav,'actual navigation source join missing');assert.equal(nav.submits[0].qualification,'EFFECTIVE');assert.equal(nav.canonical_receipts.length,0);
 assert.ok(doc.metrics.some(m=>m.metric_id==='EFFECTIVE_SUBMIT_GAP'&&m.value_ns==='200'));
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(doc)});
});
test('superseded viewport demands form one busy interval and stale pose cannot shorten it',()=>{
 const run=base(),scale='4607182418800017408',rows=[row(5,1,[11,1,2,0,0,4,7,1])];
 const demand=(camera,at)=>rows.push(row(1,41,[camera,8,44,2,3,scale,0,0],'0','10',at));
 const submit=(camera,job,at)=>rows.push(row(5,1,[11,1,2,0,0,4,7,1],'0',job),
  row(5,41,[camera,8,44,2,3,scale,0,0],'0',job,'350'),row(5,41,[1,8,44,2,3,8,scale,0,0],'0',job,'360'),
  row(8,24,[501,12,2,1],'0',job,'370'),row(9,24,[501,12,223,2,128,128,4],'0',job,'371'),row(11,12,[1,44,8,2,3,scale,0,0,0,1],'0',job,at));
 demand(4,'200');demand(5,'300');submit(4,'40','400');submit(5,'41','500');demand(6,'600');
 rows.forEach((r,n)=>r.seq=String(n+1));
 const compiled=api.compileMetrics(run,rows,meta),gaps=compiled.document.metrics.filter(m=>m.metric_id==='EFFECTIVE_SUBMIT_GAP');
 assert.equal(gaps.length,2,'superseded camera requests must merge, including the unfulfilled tail');
 assert.equal(gaps[0].start_ns,'100');assert.equal(gaps[0].end_ns,'400');assert.equal(gaps[0].value_ns,'300');
 assert.equal(gaps[1].start_ns,'500');assert.equal(gaps[1].status,'RIGHT_CENSORED');assert.equal(gaps[1].lower_bound_ns,'400');
 assert.equal(compiled.document.interactions.find(i=>i.kind==='navigation'&&i.submits.length).submits.length,2,'busy accounting must not invent extra physical submissions');
 assert.equal(compiled.document.interactions.find(i=>i.kind==='navigation'&&i.submits.length).submits[0].qualification,'STALE');
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(compiled.document)});
});
test('actual metrics retain separate Runtime owners for simultaneous navigation demands',()=>{
 const run=base(),rows=[],scale='4607182418800017408';
 for(const [runtime,job] of [[11,'40'],[99,'41']])rows.push(row(5,1,[runtime,1,2,0,0,4,7,1],'0',job),row(1,41,[4,8,44,2,3,scale,0,0],'0',job,'200'),
  row(5,41,[4,8,44,2,3,scale,0,0],'0',job,'250'),row(5,41,[1,8,44,2,3,8,scale,0,0],'0',job,'260'),
  row(8,24,[Number(job)+100,12,2,1],'0',job,'270'),row(9,24,[Number(job)+100,12,223,2,128,128,4],'0',job,'271'),row(11,12,[1,44,8,2,3,scale,0,0,0,1],'0',job,'400'));
 rows.forEach((r,n)=>r.seq=String(n+1));
 const compiled=api.compileMetrics(run,rows,meta),owners=compiled.document.interactions.filter(i=>i.kind==='navigation'&&i.submits.length);
 assert.equal(owners.length,2);assert.deepEqual(owners.map(i=>i.basis.runtime_instance).sort(),['11','99']);
 assert.equal(compiled.document.metrics.filter(m=>m.metric_id==='EFFECTIVE_SUBMIT_GAP'&&m.status==='COMPLETE').length,2);
 execFileSync('python3.12',['verification/runners/g5/ur0/validate_metrics.py'],{input:JSON.stringify(compiled.document)});
});
