// Recompile only immutable raw archives into review-owned outputs. No browser.
import fs from 'node:fs/promises';import path from 'node:path';import assert from 'node:assert/strict';import {execFileSync} from 'node:child_process';
import {canonicalChainProof} from '../../../../verification/runners/g5/ur0/canonical_chain.mjs';
const root=process.cwd(),here=path.resolve('.aegis/results/G5-UR0b/control-review-resume-r3-terminal');
const bundle=path.resolve('verification/evidence/performance/g5-unified-render-ur0b/dade34ea89c46625291c9aa39a8cbffb56b3df72fa12eac07f4048cb011af2a5/resume-r3');
const rawRoot=path.join(bundle,'execution/raw-runs'),names=(await fs.readdir(rawRoot)).sort(),results=[];
for(const name of names){
 const input=path.join(rawRoot,name),output=path.join(here,'recompiled',name);await fs.mkdir(output,{recursive:true});
 execFileSync(process.execPath,['verification/runners/g5/ur0/compile_run.mjs',input,output],{cwd:root,maxBuffer:16*1024*1024,stdio:'pipe'});
 const read=async(dir,n)=>JSON.parse(await fs.readFile(path.join(dir,n),'utf8'));
 const old=await read(input,'compiled-summary.json'),fresh=await read(output,'compiled-summary.json');
 for(const key of ['canonical_chain','schema_status','critical_loss','statistical_loss','identity_errors','actual_observations','common','final_snapshot_sha256','artifact_identity','terminal'])assert.deepEqual(fresh[key],old[key],name+'/'+key);
 const oldMetrics=await read(input,'metrics.json'),freshMetrics=await read(output,'metrics.json');assert.deepEqual(freshMetrics,oldMetrics,name+'/metrics');
 results.push({run_id:name,raw:fresh.canonical_chain.accepted_raw_count,decoded:fresh.canonical_chain.qualified_decoded_count,metrics:fresh.canonical_chain.metric_receipt_count,canonical_status:fresh.canonical_chain.status,schema:fresh.schema_status});
}
const pairs=JSON.parse(await fs.readFile(path.join(bundle,'execution/qualification-pairs.json'),'utf8'));
const {qualifyOverhead}=await import('../../../../verification/runners/g5/ur0/fixture_plan.mjs');
const overhead=qualifyOverhead(pairs),archived=JSON.parse(await fs.readFile(path.join(bundle,'execution/overhead-qualification.json'),'utf8'));assert.deepEqual(overhead,archived);
// Inspect the first frame directly, independent of the decoder's qualification.
const realtime=path.join(rawRoot,'realtime-60');
const raw=(await fs.readFile(path.join(realtime,'raw-trace.jsonl'),'utf8')).trim().split('\n').map(JSON.parse);
const record=seq=>{const found=raw.filter(r=>r.seq===seq);assert.equal(found.length,1);return found[0];};
const [draw,provider,capture,complete]=['15','32','42','47'].map(record);
assert.equal(draw.kind,5);assert.equal(draw.tag,26);assert.equal(provider.kind,11);assert.equal(provider.tag,19);assert.equal(provider.values[6],'0');assert.equal(capture.kind,11);assert.equal(capture.tag,29);assert.equal(complete.kind,12);assert.equal(complete.tag,29);assert.equal(complete.values[0],'1');
assert.deepEqual([draw.job,draw.basis],[provider.job,provider.basis]);assert.equal(draw.values[1],complete.values[6]);assert.equal(capture.values[0],complete.values[1]);
assert.ok([draw.values[2],draw.values[3],capture.values[3],capture.values[4],complete.values[7],complete.values[8]].every(x=>x==='0'));
const commits=raw.filter(r=>r.kind===6&&r.tag===27);assert.equal(commits.length,1);assert.ok(BigInt(complete.at_ns)<BigInt(commits[0].at_ns));
const ownerFor=r=>{const found=raw.filter(o=>o.kind===5&&o.tag===1&&o.job===r.job&&o.basis===r.basis);assert.equal(found.length,1);return found[0];};
const owners=[draw,provider,capture,complete].map(ownerFor);assert.ok(owners.every(o=>JSON.stringify(o.values)===JSON.stringify(owners[0].values)));
const worker=JSON.parse(await fs.readFile(path.join(realtime,'worker-result.json'),'utf8'));
assert.equal(worker.measurement.before.generation,'0');assert.equal(worker.measurement.before.objects,0);assert.equal(worker.measurement.after.generation,'1');assert.equal(worker.measurement.after.objects,1);
const witness={status:'REPRODUCED_UNACCOUNTED_INITIALIZATION_FRAME',draw_seq:draw.seq,provider_seq:provider.seq,capture_seq:capture.seq,completion_seq:complete.seq,successful_draw_job:draw.job,work_id:capture.values[0],generation:'0',scene_revision:'0',actual_operation_present:false,owner_values:owners[0].values,first_commit_seq:commits[0].seq,first_commit_at_ns:commits[0].at_ns,initial_completion_at_ns:complete.at_ns,before_state_generation:worker.measurement.before.generation,before_state_objects:0,after_state_generation:'1',after_state_objects:1,raw_completions_accounted_by_legacy_proof:results.find(r=>r.run_id==='realtime-60'),scope:'Read-only fact witness; does not newly qualify or omit the initialization frame'};
await fs.writeFile(path.join(here,'offline-check.json'),JSON.stringify({status:'OFFLINE_RESULTS_REPRODUCED',inputs:results,overhead,witness,new_browser_workers:0},null,2)+'\n');
console.log(JSON.stringify({inputs:results.length,overhead:overhead.status,initialization:witness.status,realtime:results.find(r=>r.run_id==='realtime-60')}));
