// Recompute durable raw observations with the exact measured metric source.
import fs from 'node:fs/promises';import path from 'node:path';import {gunzipSync}from'node:zlib';import assert from'node:assert/strict';
import {summarizePhase}from'./navigation_metrics.mjs';import{counterbalancedEffect,compareBlockEffects}from'./feedback_metrics.mjs';
const root=process.argv[2],read=async n=>{try{return JSON.parse(await fs.readFile(path.join(root,n),'utf8'));}catch(error){if(error.code!=='ENOENT')throw error;return JSON.parse(gunzipSync(await fs.readFile(path.join(root,n+'.gz'))));}};
const matrix=await read('matrix.json');let phases=0;const endpoints=new Map();
for(const row of matrix.results){const record=await read(row.id+'/wheel.json');const p=[];
 for(const phase of record.phases){const rows=record.rows.filter(r=>r.phase===phase.id);const computed=summarizePhase(phase.raw,rows.filter(r=>r.kind==='navigation').map(r=>r.value),rows.filter(r=>r.kind==='frame').map(r=>r.value));assert.deepEqual(computed,phase.summary,row.id+'/'+phase.id);++phases;
 p.push({first:computed.runtimeSubmitV2.firstAcceptedSubmitMs,steady:computed.runtimeSubmitV2.steadyAcceptedGapMs.max,receipt:computed.feedbackV2.lastInputToLastCoarseReceiptMs,low:computed.navigation.quality.timeWeightedLowResolutionFraction});}
 endpoints.set(row.id,Object.fromEntries(Object.keys(p[0]).map(k=>[k,Math.max(...p.map(v=>v[k]))])));
}
for(const kind of['AA','AB'])for(const c of['pan','zoom'])for(const key of['first','steady','receipt','low']){
 const centered=[];for(let b=0;b<2;++b){const rows=matrix.results.filter(r=>r.kind===kind&&r.case===c&&r.block===b).sort((a,b)=>a.position-b.position);const block=counterbalancedEffect(rows.map(r=>endpoints.get(r.id)[key]));centered.push(block.effect-(key==='low'?.02:Math.max(16.7,.1*block.baseline)));}
 assert.equal(compareBlockEffects(centered).status,'INCONCLUSIVE');
}
console.log('Recomputed',phases,'raw phase summaries and retained INCONCLUSIVE for two-block pilots');
