import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';

const original=process.env.G5_PACING_OLD==='1';
async function oldGesture(h){
  // Execute the actual pre-repair inline functions, not a model of their loop.
  const source=await fs.readFile(new URL('../runners/g5/prepared_body_index.mjs',import.meta.url),'utf8');
  const until=source.match(/const until=async t=>\{[^\n]+\};/)[0];
  const moves=source.match(/const moves=async\(p,count,position\)=>\{[\s\S]+?(?=\n  const digest=)/)[0];
  const run=new Function('performance','setTimeout','state','pointer','check',until+'\n'+moves+'\nreturn moves;');
  const execute=run({now:h.now},h.timer,()=>({generation:'0'}),(phase,x,y)=>h.move(x,y),assert.ok);
  h.down();await execute({schedule:h.schedule},32,i=>[i,i*i]);h.up();
}
const subject=original?{runFunctionalErase:oldGesture}:await import('../runners/g5/prepared_body_index_reverify_pacing.mjs');
function world(coldMs=0){
  let clock=0,pending=false,first=true,active=false,frames=0;
  const events=[],schedule=[],opportunities=[];
  const display=async()=>{clock+=16.666667;if(pending&&active){if(first){clock+=coldMs;first=false;}++frames;pending=false;}};
  const h={now:()=>clock,timer:(fn,delay)=>{const target=clock+delay;display();clock=Math.max(clock,target);queueMicrotask(fn);},
    until:async t=>{if(clock<t){await display();clock=Math.max(clock,t);}},display,
    position:i=>[i,i*i],schedule,opportunities,
    down(){active=true;pending=true;events.push(['down',0,0]);},move(x,y){pending=true;events.push(['move',x,y]);},
    up(){events.push(['up',32,1024]);active=false;},cancel(){events.push(['cancel']);active=false;},
    observed:()=>({clock,frames,active,events,schedule,opportunities})};return h;
}
for(const cold of [600,1200])test(`cold overrun ${cold}ms preserves later active display opportunities before Up`,async()=>{
  const h=world(cold);await subject.runFunctionalErase(h);const o=h.observed();
  console.log(JSON.stringify({subject:original?'original-inline':'repaired-functional',cold_ms:cold,active_frames:o.frames,moves:o.events.filter(x=>x[0]==='move').length,last_planned:o.schedule.at(-1)?.planned}));
  assert.ok(o.frames>=2,'cold overrun collapsed all later Move and Up before a second active display opportunity');
  assert.equal(o.events.at(-1)[0],'up');assert.equal(o.active,false);
});
test('functional pacing preserves positions/order/deadlines without a cold overrun',async()=>{
  const h=world();await subject.runFunctionalErase(h);const o=h.observed();
  assert.deepEqual(o.events,[['down',0,0],...Array.from({length:32},(_,i)=>['move',i+1,(i+1)**2]),['up',32,1024]]);
  assert.equal(o.schedule.length,32);assert.equal(o.schedule.at(-1).planned,32*16.666667);
  assert.equal(o.frames,33);assert.equal(o.opportunities.length,33);
});
test('abort cancels terminal contact and never sends Up',async()=>{
  const h=world(),controller=new AbortController(),move=h.move;h.signal=controller.signal;
  h.move=(x,y)=>{move(x,y);if(x===3)controller.abort();};
  await assert.rejects(subject.runFunctionalErase(h),/aborted/);
  assert.equal(h.observed().events.at(-1)[0],'cancel');assert.equal(h.observed().active,false);
  assert.equal(h.observed().events.some(x=>x[0]==='up'),false);
});
test('cold work beyond the local functional bound cancels instead of passing',async()=>{
  const h=world(600);h.budgetMs=100;
  await assert.rejects(subject.runFunctionalErase(h),/budget/);
  assert.deepEqual(h.observed().events.map(x=>x[0]),['down','cancel']);
});
test('display wait is bounded and cancels its queued RAF on timeout',async()=>{
  const cancelled=[];
  await assert.rejects(subject.boundedDisplayOpportunity({timeoutMs:3,request:()=>42,cancel:id=>cancelled.push(id),delay:fn=>{queueMicrotask(fn);return 9;},clear:()=>{}}),/display opportunity timeout/);
  assert.deepEqual(cancelled,[42]);
});
test('already aborted display wait never queues a RAF',async()=>{
  const c=new AbortController();c.abort();let requests=0;
  await assert.rejects(subject.boundedDisplayOpportunity({signal:c.signal,timeoutMs:3,request:()=>++requests,cancel:()=>{},delay:()=>{},clear:()=>{}}),/aborted/);
  assert.equal(requests,0);
});
