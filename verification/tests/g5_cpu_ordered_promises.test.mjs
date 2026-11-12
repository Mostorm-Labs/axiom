import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';import vm from 'node:vm';
const root=fs.readFileSync(new URL('../../sdk/web/axiom.mjs',import.meta.url),'utf8');
test('actual admission tail continues navigation while mutation promise awaits exact owner terminal',async()=>{
 const start=root.indexOf('  const terminalWaiters='),end=root.indexOf('  const cleanup =',start);let applied=false,schedules=0;
 const context={Promise,Map,AxiomError:class extends Error{},closing:false,closed:false,closeBarrier:false,tail:Promise.resolve(),text:{finish:async()=>{}},changed(){},schedule(){++schedules;},call(command,token){assert.equal(command,'pendingReceipt');assert.equal(token.serial,'7');return{ok:true,done:applied,generation:'19'};}};
 vm.createContext(context);vm.runInContext(root.slice(start,end)+';globalThis.submit=enqueue;globalThis.poll=resolveTerminals;',context);
 let done=false;const waiting=context.submit(()=>({ok:true,pendingToken:{runtimeInstance:'1',lifetimeGeneration:'4',serial:'7'}})).then(()=>done=true);
 await context.submit(()=>({navigation:true}));assert.equal(done,false);context.poll();assert.equal(done,false);applied=true;context.poll();await waiting;assert.equal(done,true);assert.ok(schedules);
});
test('actual terminal failure rejects original promise without blocking an unrelated admission',async()=>{
 const start=root.indexOf('  const terminalWaiters='),end=root.indexOf('  const cleanup =',start);
 const context={Promise,Map,AxiomError:class extends Error{},closing:false,closed:false,closeBarrier:false,tail:Promise.resolve(),text:{finish:async()=>{}},changed(){},schedule(){},call(){throw Error('INTENT_FAILED');}};vm.createContext(context);vm.runInContext(root.slice(start,end)+';globalThis.submit=enqueue;globalThis.poll=resolveTerminals;',context);
 const waiting=context.submit(()=>({ok:true,pendingToken:{runtimeInstance:'1',lifetimeGeneration:'4',serial:'7'}}));const observed=assert.rejects(waiting,/INTENT_FAILED/);await context.submit(()=>({ok:true}));context.poll();await observed;
});
