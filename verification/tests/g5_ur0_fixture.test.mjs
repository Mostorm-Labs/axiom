import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
const path=new URL('../runners/g5/ur0/fixture_plan.mjs',import.meta.url);
let api={};try{api=await import(path);}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
const literal=JSON.parse(await fs.readFile(new URL('../../docs/planning/g5-unified-render/ur0a/fixtures/continuous-30s-v1.json',import.meta.url)));
test('realtime batching retains every literal sample and separates terminal contacts',()=>{
 assert.equal(typeof api.realtimePlan,'function','fixed fixture planner unavailable');
 for(const hz of [60,120,240]){
  const plan=api.realtimePlan(literal,hz);const samples=plan.flatMap(b=>b.samples);
  assert.deepEqual(samples,literal.samples);assert.equal(plan[0].samples.length,1);assert.equal(plan.at(-1).samples.length,1);
  assert.equal(plan.at(-1).planned_offset_ns,'30000000000');
  assert.ok(plan.every(b=>b.samples.length<=240/hz));
 }
});
test('clock brackets reject incompatible domains rather than manufacture queue time',()=>{
 assert.equal(typeof api.calibrate,'function','clock qualifier unavailable');
 const a=Array.from({length:16},(_,i)=>({before_ns:String(i*10000),native_ns:String(i*10000+5000),after_ns:String(i*10000+200)}));
 const good=api.calibrate(a,a);assert.equal(good.status,'QUALIFIED');assert.equal(good.offset_ns,'4900');
 const bad=a.map(x=>({...x,native_ns:String(BigInt(x.native_ns)+10000000n)}));assert.equal(api.calibrate(a,bad).status,'MISSING');
});
test('overhead gate retains all six pairs and rejects critical loss',()=>{
 assert.equal(typeof api.qualifyOverhead,'function','overhead qualifier unavailable');
 const pairs=Array.from({length:6},()=>({off:{p95_ns:'2000000',wall_ns:'2000000000',critical_loss:0},on:{p95_ns:'2100000',wall_ns:'2010000000',critical_loss:0}}));
 assert.equal(api.qualifyOverhead(pairs).status,'QUALIFIED');pairs[2].on.critical_loss=1;assert.equal(api.qualifyOverhead(pairs).status,'FAILED');
 assert.equal(api.qualifyOverhead(pairs.slice(0,5)).status,'MISSING');
});
test('missing dispatch samples cannot qualify as a zero-cost overhead pair',()=>{
 const pairs=Array.from({length:6},()=>({off:{p95_ns:'2000000',wall_ns:'2000000000',critical_loss:0},on:{p95_ns:'2100000',wall_ns:'2010000000',critical_loss:0}}));
 pairs[1].on.p95_ns=null;
 assert.equal(api.qualifyOverhead(pairs).status,'MISSING');
});
test('legacy input remains an unquantized 28.796 second anchor',()=>{
 assert.equal(typeof api.legacyCorpus,'function','legacy anchor planner unavailable');
 const f=api.legacyCorpus();assert.equal(f.samples.length,7200);assert.equal(f.samples[0][2],'1004000000');assert.equal(f.samples.at(-1)[2],'29800000000');
 assert.equal(f.divisor,1);assert.ok(Math.abs(f.samples[1][0]-366.549)<.001);assert.notEqual(f.samples[1][0],366.549);
});
