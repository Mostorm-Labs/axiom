import test from 'node:test';import assert from 'node:assert/strict';
const m=await import('../runners/g5/ur0/schedule.mjs').catch(()=>null);
const api=()=>{assert.equal(typeof m?.deliver,'function','fixed-plan driver is unavailable');return m;};
test('a blocked Down never shifts the next original planned time or drops Up',async()=>{
 let now=0n,yields=0;const sent=[];const events=Array.from({length:11},(_,i)=>({event_id:String(i),kind:'pointer',phase:i===0?'down':i===10?'up':'move',planned_offset_ns:String(i*1000000)}));
 const rows=await api().deliver(events,{originNs:0n,nowNs:()=>now,sleep:async ms=>{now+=BigInt(Math.round(ms*1e6));},yieldTask:async()=>{++yields;},dispatch:e=>{sent.push(e.phase);if(e.phase==='down')now+=20000000n;}});
 assert.equal(rows.length,11);assert.deepEqual(rows.map(r=>r.planned_ns),events.map(e=>e.planned_offset_ns));assert.equal(rows[1].producer_ns,'20000000');assert.equal(rows.at(-1).phase,'up');assert.equal(sent.filter(x=>x==='up').length,1);assert.equal(yields,1);
});
test('rejects a reversed plan and retains thrown dispatch as failure',async()=>{
 let now=0n;await assert.rejects(()=>api().deliver([{planned_offset_ns:'2'},{planned_offset_ns:'1'}],{originNs:0n,nowNs:()=>now,sleep:async()=>{},dispatch:()=>{}}),/order/);
 const r=await api().deliver([{event_id:'e',phase:'down',planned_offset_ns:'0'}],{originNs:0n,nowNs:()=>now,sleep:async()=>{},dispatch:()=>{throw Error('real rejection');}});assert.equal(r[0].status,'FAILED');assert.match(r[0].error,/real rejection/);
});
test('failure/censoring stays in the denominator and large ns values remain exact',()=>{
 const {summarize}=api();const a=summarize([{status:'COMPLETE',value_ns:'9007199254740993'},{status:'COMPLETE',value_ns:'9007199254740995'},{status:'FAILED'},{status:'RIGHT_CENSORED',lower_bound_ns:'99'}]);
 assert.equal(a.planned_count,4);assert.equal(a.p50_ns,'9007199254740993');assert.equal(a.p95_ns,'9007199254740995');assert.equal(a.failed_count,1);assert.equal(a.censored_count,1);assert.equal(a.tail_status,'INCOMPLETE');assert.equal(summarize([{status:'MISSING'}]).p99_ns,null);
});
