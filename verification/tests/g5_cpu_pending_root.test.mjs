import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';import vm from 'node:vm';
const root=fs.readFileSync(new URL('../../sdk/web/axiom.mjs',import.meta.url),'utf8');
test('actual close preflight Busy preserves active Worker and frame pump',async()=>{
 const start=root.indexOf('      async close(){'),end=root.indexOf('\n    };',start),method=root.slice(start,end).trim();
 let terminated=0,unbound=0;const context={AxiomError:class extends Error{constructor(code){super(code);this.code=code;}},unbindCpuWorkerOwner:()=>++unbound,call:(command,args)=>{if(command==='cpuPending'&&args.action==='closePreflight'){const e=new Error('BUSY');e.code='BUSY';throw e;}throw Error('unexpectedpostpreflight');},schedule:()=>{},cleanup:()=>{},tail:Promise.resolve(),text:{finish:async()=>true,dispose:()=>{}},cpuWorkers:new Set([{close:async()=>++terminated}]),closed:false,closing:false,closeBarrier:false,pendingRestart:null,pendingReplacement:null,module:{}};
 vm.createContext(context);vm.runInContext('const api={'+method+'};globalThis.api=api;',context);await assert.rejects(context.api.close(),/BUSY/);assert.equal(terminated,0);assert.equal(unbound,0);assert.equal(context.closing,false);assert.equal(context.closeBarrier,false);
});

test('actual root suspends owner requests while delayed executor initializes and leaves frame scheduling independent',async()=>{
 const start=root.indexOf('  const pumpPending=()=>{'),end=root.indexOf('  const listeners=',start),source=root.slice(start,end).trim();
 let resolveReady,calls=[],schedules=0;const replacement={observation:()=>({executor:'2',closed:false})};
 const c={pendingWorker:{observation:()=>({closed:true})},pendingTask:null,pendingRestart:null,closed:false,pendingReplacement:null,pendingRestarts:0,pendingFailed:'0',pendingRoot:{},pendingFactory:()=>new Promise(r=>resolveReady=r),call:(cmd,args)=>{calls.push([cmd,args]);if(cmd==='state')return{value:{surface:{available:true}}};if(args?.action==='bind')return{};return{failed:'0',state:'FrozenAwaitingPreparation',requestReady:false};},emitError:()=>{},AxiomError:Error,schedule:()=>++schedules};
 vm.createContext(c);vm.runInContext(source+';globalThis.pump=pumpPending;',c);c.pump();assert.ok(c.pendingRestart);assert.equal(calls.filter(x=>x[1]?.action==='suspend').length,1);assert.equal(c.pendingRestarts,1);c.pump();assert.equal(calls.filter(x=>x[1]?.action==='take').length,0);resolveReady(replacement);await c.pendingRestart;c.pump();assert.equal(c.pendingWorker,replacement);assert.equal(c.pendingReplacement,null);assert.equal(calls.filter(x=>x[1]?.action==='bind').length,1);assert.ok(schedules);
});

test('actual root bounded second fatal marks owner unavailable without restart loop',()=>{
 const start=root.indexOf('  const pumpPending=()=>{'),end=root.indexOf('  const listeners=',start);let actions=[];
 const c={pendingWorker:{observation:()=>({closed:true})},pendingTask:null,pendingRestart:null,pendingReplacement:null,closed:false,pendingRestarts:1,pendingFailed:'0',call:(cmd,a)=>{if(a)actions.push(a.action);return{failed:'0',state:'FrozenAwaitingPreparation'};},emitError(){},AxiomError:Error};vm.createContext(c);vm.runInContext(root.slice(start,end)+';pumpPending();',c);assert.deepEqual(actions,['suspend','unavailable']);
});
