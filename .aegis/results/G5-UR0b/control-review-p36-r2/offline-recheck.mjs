// Independent control recheck: reads immutable recordings, writes no raw data.
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import assert from 'node:assert/strict';
const root=process.cwd(),module=n=>import(pathToFileURL(path.join(root,'verification/runners/g5/ur0',n)));
const {decodeTrace}=await module('decode_trace.mjs');
const {compileMetrics}=await module('metrics_compiler.mjs');
const {canonicalChainProof}=await module('canonical_chain.mjs');
const read=async p=>JSON.parse(await fs.readFile(p,'utf8'));
const a=(await read('.aegis/results/G5-UR0b/p36-r1/result.json')).materialized_ref.path;
const b=(await read('.aegis/results/G5-UR0b/p36-r2/result.json')).materialized_ref.path;
const rows=[];
for(const name of ['diagnostic-OFF','diagnostic-ON','qualification-p1-A','qualification-p1-B','qualification-p2-B']){
 const input=path.join(a,'execution/raw-runs',name),output=path.join(b,'offline-reverification',name);
 const raw=(await fs.readFile(path.join(input,'raw-trace.jsonl'),'utf8')).split('\n').filter(Boolean).map(JSON.parse);
 const run=await read(path.join(input,'worker-result.json')),inputs=await read(path.join(input,'run-inputs.json'));
 const decoded=decodeTrace(raw,run.trace.loss);
 const compiled=compileMetrics(run,raw,{run_id:name,row_id:name.replace(/-r\d+$/,''),repetition:Number(/-r(\d+)$/.exec(name)?.[1]??1),fixture_sha256:inputs.fixture_sha256});
 const proof=canonicalChainProof(raw,decoded,compiled.document);
 assert.deepEqual(compiled.document,await read(path.join(output,'metrics.json')));
 assert.deepEqual(proof,await read(path.join(output,'canonical-chain.json')));
 const accepted=raw.filter(r=>r.kind===12&&r.tag===29&&r.values[0]==='1');
 assert.equal(decoded.errors.length,0);
 assert.equal(proof.accepted_raw_count,accepted.length);
 assert.equal(proof.qualified_decoded_count,accepted.length);
 assert.equal(proof.metric_receipt_count,accepted.length);
 const index=new Map(raw.map(r=>[r.seq,r]));
 const successfulJobs=[];
 for(const c of proof.chains){
  const draw=index.get(c.draw_seq),submit=index.get(c.provider_seq),capture=index.get(c.capture_seq),completion=index.get(c.completion_seq);
  assert.equal(submit.kind,11);assert.equal(submit.tag,19);assert.equal(submit.values[6],'0');
  assert.equal(draw.job,submit.job);assert.equal(draw.basis,submit.basis);
  assert.equal(c.draw_job,submit.job);assert.equal(c.draw_basis,submit.basis);
  assert.equal(capture.values[0],completion.values[1]);assert.ok(c.attempt_jobs.includes(draw.job));
  assert.ok([draw,submit,capture,completion].every((r,i,rs)=>i===0||BigInt(rs[i-1].seq)<BigInt(r.seq)));
  successfulJobs.push({job:c.draw_job,basis:c.draw_basis,attempt_jobs:c.attempt_jobs.length,completion_seq:c.completion_seq});
 }
 const before=await read(path.join(input,'compiled-summary.json')),after=await read(path.join(output,'compiled-summary.json'));
 for(const f of ['common','terminal','final_snapshot_sha256','product_errors','artifact_identity'])assert.deepEqual(before[f],after[f]);
 assert.equal(after.analysis.mode,'OFFLINE_REVERIFICATION');
 rows.push({run:name,status:proof.status,raw_accepted:accepted.length,decoded:proof.qualified_decoded_count,receipts:proof.metric_receipt_count,successful_jobs:successfulJobs,metrics_equal_archive:true,recorded_behavior_unchanged:true});
}
const result={scope:'P36-r2 offline compilation only',rows,total_accepted:rows.reduce((s,r)=>s+r.raw_accepted,0),new_browser_workers:0,primary_timing_adopted:false};
await fs.writeFile('.aegis/results/G5-UR0b/control-review-p36-r2/offline-check.json',JSON.stringify(result,null,2)+'\n');
console.log(JSON.stringify(result));
