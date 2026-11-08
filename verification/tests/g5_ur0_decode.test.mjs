import test from 'node:test';import assert from 'node:assert/strict';
import fs from 'node:fs/promises';import os from 'node:os';import path from 'node:path';import {execFileSync} from 'node:child_process';
let api={};try{api=await import('../runners/g5/ur0/decode_trace.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
const raw=()=>{
 const r=(kind,tag,values,job='10')=>({kind,tag,values:values.map(String),job,basis:job,attempt:job,event:'1',purpose:2,route:1,at_ns:'100',seq:'1'});
 return [
  r(5,1,[11,1,2,0,0,4,7,1]),
  r(6,27,[0,0,1,0,8,9,4,6]),
  r(7,9,[9,9,44]),
  r(5,1,[11,1,2,0,0,4,7,1],'20'),
  r(5,26,[1,23,9,44,4,2,3,8,67108864,201326592],'20'),
  r(11,19,[1,23,9,44,2,3,0],'20'),
  r(5,26,[10,11,1,2,0,0,4,6,7,44,4]),
  r(11,29,[10,1,23,9,9,2,3]),
  r(12,29,[1,10,11,1,2,1,23,9,9,2,3]),
 ];
};
const repeatedFrame=()=>{
 const original=raw(),rows=original.slice(0,3);
 for(let n=0;n<24;++n){const job=String(20+n);
  rows.push({...original[3],job,basis:job,values:[...original[3].values]},
   {...original[4],job,basis:job,values:[...original[4].values]});
 }
 rows.push({...original[5],job:'43',basis:'43',values:[...original[5].values]},...original.slice(6));
 rows.forEach((r,n)=>{r.seq=String(n+1);r.at_ns=String(100+n);});return rows;
};
test('same-frame retries select the unique accepted provider job and preserve every attempt',()=>{
 const rows=repeatedFrame(),d=api.decodeTrace(rows,{critical:0,statistical:0});
 assert.equal(d.qualified_completions.length,1,'24 preparations and one accepted submit lost the real completion');
 assert.equal(d.render_frames.length,24);assert.equal(d.frames[0].job,'43');assert.equal(d.frames[0].attempt_jobs.length,24);
 assert.ok(d.frames[0].provenance.includes(rows.find(r=>r.kind===11&&r.tag===19).seq));
});
test('an unrelated later attempt never replaces the job that actually submitted',()=>{
 const rows=repeatedFrame(),capture=rows.findIndex(r=>r.kind===11&&r.tag===29),original=raw();
 rows.splice(capture,0,{...original[3],job:'99',basis:'99',values:[...original[3].values]},
  {...original[4],job:'99',basis:'99',values:[...original[4].values]});
 rows.forEach((r,n)=>{r.seq=String(n+1);r.at_ns=String(100+n);});
 const d=api.decodeTrace(rows,{critical:0,statistical:0});assert.equal(d.qualified_completions.length,1);assert.equal(d.frames[0].job,'43');
});
test('provider job, basis, camera, Scene and chronology mismatches remain unqualified',()=>{
 for(const change of [
  rows=>rows.find(r=>r.kind===11&&r.tag===19).job='999',
  rows=>rows.find(r=>r.kind===11&&r.tag===19).basis='999',
  rows=>rows.find(r=>r.kind===5&&r.tag===26&&r.job==='43').values[4]='99',
  rows=>rows.find(r=>r.kind===11&&r.tag===19).values[3]='99',
  rows=>rows.find(r=>r.kind===11&&r.tag===19).values[1]='99',
  rows=>rows.find(r=>r.kind===11&&r.tag===19).values[4]='99',
  rows=>rows.find(r=>r.kind===11&&r.tag===19).values[5]='99',
  rows=>rows.find(r=>r.kind===5&&r.tag===26&&r.job==='43').at_ns='9999',
  rows=>rows.find(r=>r.kind===11&&r.tag===19).at_ns='9999',
  rows=>rows.at(-1).at_ns='99',
 ]){const rows=repeatedFrame();change(rows);assert.equal(api.decodeTrace(rows,{critical:0,statistical:0}).qualified_completions.length,0);}
});
test('duplicate accepted submits and ambiguous draws within the successful job fail closed',()=>{
 for(const kind of ['submit','draw']){
  const rows=repeatedFrame(),match=rows.find(r=>kind==='submit'?r.kind===11&&r.tag===19:r.kind===5&&r.tag===26&&r.job==='43');
  rows.splice(rows.indexOf(match)+1,0,{...match,values:[...match.values]});rows.forEach((r,n)=>{r.seq=String(n+1);r.at_ns=String(100+n);});
  const d=api.decodeTrace(rows,{critical:0,statistical:0});assert.equal(d.qualified_completions.length,0);
 }
});
test('decodes actual observed commit Scene submit and completion fields',()=>{
 assert.equal(typeof api.decodeTrace,'function','raw trace decoder unavailable');
 const d=api.decodeTrace(raw(),{critical:0,statistical:0});assert.equal(d.errors.length,0);assert.equal(d.qualified_completions.length,1);
 assert.equal(d.qualified_completions[0].work,'10');assert.equal(d.qualified_completions[0].resource,'7');assert.equal(d.frames[0].gpu_resource_epoch,'8');
});
test('foreign completion or mismatched Scene cannot be repaired by copying expected fields',()=>{
 assert.equal(typeof api.decodeTrace,'function','raw trace decoder unavailable');
 const foreign=raw();foreign.at(-1).values[3]='99';assert.ok(api.decodeTrace(foreign,{critical:0,statistical:0}).errors.length);
 const stale=raw();stale[2].values[1]='8';assert.ok(api.decodeTrace(stale,{critical:0,statistical:0}).errors.length);
});
test('Scene intent publication can precede the callsite observation of its returned commit record',()=>{
 const rows=raw();[rows[1],rows[2]]=[rows[2],rows[1]];rows[1].at_ns='98';
 const d=api.decodeTrace(rows,{critical:0,statistical:0});assert.equal(d.errors.length,0);assert.equal(d.qualified_completions.length,1);
 assert.equal(d.scenes[0].commit_observed_after_publication,true);
});
test('scope exclusive time and GPU callback ordering preserve raw facts',()=>{
 assert.equal(typeof api.decodeTrace,'function','raw trace decoder unavailable');
 const base={seq:'1',job:'1',basis:'1',attempt:'1',event:'0',purpose:2,route:1};
 const rows=[{...base,kind:2,tag:10,values:['1','2','0'],at_ns:'0'},{...base,kind:2,tag:18,values:['2','0','1'],at_ns:'2'},{...base,kind:3,tag:18,values:['2'],at_ns:'7'},{...base,kind:3,tag:10,values:['1'],at_ns:'10'},
 {...base,kind:14,tag:0,values:['5','7','0','0'],at_ns:'11'},{...base,kind:13,tag:0,values:['5','7','3','1'],at_ns:'12'},{...base,kind:15,tag:0,values:['5','3'],at_ns:'14'}];
 const d=api.decodeTrace(rows,{critical:0,statistical:0});assert.equal(d.spans.find(s=>s.id==='1').exclusive_ns,'5');assert.equal(d.gpu[0].callback_before_submit_return,true);
 assert.equal(d.gpu[0].pure_gpu_duration_ns,null);
});
test('actual compiler rejects another Runtime committing the same generation',async()=>{
 const rows=raw();
 rows.splice(2,0,{...rows[0],job:'99',basis:'99',values:['99','1','2','0','0','4','7','1']},
   {...rows[1],job:'99',basis:'99',values:['0','0','2','0','8','9','4','6']});
 rows.find(r=>r.kind===5&&r.tag===1&&r.job==='20').values[0]='99';
 const tmp=await fs.mkdtemp(path.join(os.tmpdir(),'ur0-foreign-'));
 try{
  await fs.writeFile(path.join(tmp,'worker-result.json'),JSON.stringify({status:'OBSERVED',trace:{loss:{critical:0,statistical:0}}}));
  await fs.writeFile(path.join(tmp,'raw-trace.jsonl'),rows.map(r=>JSON.stringify(r)).join('\n'));
  execFileSync(process.execPath,['verification/runners/g5/ur0/compile_run.mjs',tmp]);
  const actual=JSON.parse(await fs.readFile(path.join(tmp,'decoded.json')));
  assert.equal(actual.qualified_completions.length,0,'foreign Scene/operation was qualified by actual compiler');
  assert.ok(actual.errors.length||actual.missing.length,'unproved owner silently accepted');
 }finally{await fs.rm(tmp,{recursive:true,force:true});}
});
test('foreign same-generation noise cannot replace the actual matching operation',()=>{
 const rows=raw();rows.splice(2,0,{...rows[0],job:'99',basis:'99',values:['99','1','2','0','0','4','7','1']},
  {...rows[1],job:'99',basis:'99',values:['0','0','2','0','8','9','4','6']});
 const d=api.decodeTrace(rows,{critical:0,statistical:0});
 assert.equal(d.qualified_completions.length,1);assert.equal(d.qualified_completions[0].operation,'01000000000000000000000000000000');
});
test('lifetime attachment resource missing or ambiguous render owners fail closed',()=>{
 for(const field of [1,2,6]){
  const rows=raw();rows.find(r=>r.kind===5&&r.tag===1&&r.job==='20').values[field]='99';
  assert.equal(api.decodeTrace(rows,{critical:0,statistical:0}).qualified_completions.length,0);
 }
 const absent=raw().filter(r=>!(r.kind===5&&r.tag===1&&r.job==='20'));
 assert.equal(api.decodeTrace(absent,{critical:0,statistical:0}).qualified_completions.length,0);
 const ambiguous=raw();ambiguous.push({...ambiguous[0],values:['99','1','2','0','0','4','7','1']});
 assert.equal(api.decodeTrace(ambiguous,{critical:0,statistical:0}).qualified_completions.length,0);
});
test('compact lease lifecycles restore all actual identities, times and ordered events',()=>{
 assert.equal(typeof api.expandLeaseCycles,'function','lossless lease decoder unavailable');
 const packed={seq:'10',at_ns:'100',job:'9',basis:'9',attempt:'9',event:'4',route:1,purpose:2,kind:10,tag:43,
  values:['6','91','7','0','20','15','0','80','100','120','140','95','95','10','12','14']};
 const middle={...packed,seq:'11',at_ns:'110',tag:20,values:['1','12']};
 const rows=api.expandLeaseCycles([middle,packed]);
 assert.deepEqual(rows.map(r=>r.seq),['10','11','12','14']);
 const leases=rows.filter(r=>r.tag===43);
 assert.deepEqual(leases.map(r=>r.values[0]),['1','2','3']);
 assert.deepEqual(leases.map(r=>r.at_ns),['100','120','140']);
 assert.deepEqual(leases.map(r=>r.values[7]),['80','95','95']);
 assert.ok(leases.every(r=>r.job==='9'&&r.values[1]==='91'&&r.values[2]==='7'&&r.values[5]==='15'));
 assert.throws(()=>api.expandLeaseCycles([{...packed,values:packed.values.slice(0,15)}]),/lifecycle/);
 assert.throws(()=>api.expandLeaseCycles([packed,{...middle,seq:'12'}]),/sequence/);
});
