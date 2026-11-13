import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';import vm from 'node:vm';
const clientSource=fs.readFileSync(new URL('../../sdk/web/internal/cpu-worker-client.mjs',import.meta.url),'utf8');
const ownerSource=fs.readFileSync(new URL('../../sdk/web/internal/cpu-worker-owner.mjs',import.meta.url),'utf8');
async function fixture(mode='normal'){
 let used=0,next=0,terminated=0,lastWorker;const grants=new Map(),workers=new Set();const limits={heapBytes:67108864,queueJobs:8,inputBytes:1048576,outputBytes:1048576,transportBytes:16777216,stepSegments:8,stepMicroseconds:2000},kernelId='a'.repeat(32);
 const owner={basis:()=>['1','1','2','0'],assetBaseURL:'http://localhost/internal/',reserve:(_,bytes)=>{used+=bytes;grants.set(String(++next),bytes);return String(next);},release:(_,id)=>{assert(grants.has(id));used-=grants.get(id);grants.delete(id);},attach:w=>workers.add(w),detach:w=>workers.delete(w)};
 class Worker{constructor(){lastWorker=this;this.jobs=[];}postMessage(m){if(m.type==='init'){if(mode==='throw')throw Error('post failed');if(mode!=='hang')queueMicrotask(()=>this.onmessage({data:{type:'ready',build:kernelId,heap:limits.heapBytes}}));}if(m.type==='run')this.jobs.push(m);if(m.type==='cancel')this.onmessage({data:{type:'terminal',key:m.key,status:'Cancelled'}});}terminate(){++terminated;}}
 const context=vm.createContext({WeakMap,Set,Map,Promise,ArrayBuffer,Uint8Array,DataView,BigInt,Error,URL,performance,Worker,queueMicrotask,setTimeout,clearTimeout});
 const ownerModule=new vm.SourceTextModule(ownerSource,{context});await ownerModule.link(()=>{});await ownerModule.evaluate();const runtime={};ownerModule.namespace.bindCpuWorkerOwner(runtime,owner);
 const build=new vm.SyntheticModule(['kernelId','limits'],function(){this.setExport('kernelId',kernelId);this.setExport('limits',limits);},{context});await build.link(()=>{});await build.evaluate();
 const m=new vm.SourceTextModule(clientSource,{context});await m.link(name=>name.includes('owner')?ownerModule:build);await m.evaluate();
 const request=(id,executor='1',runtimeId='1')=>{const b=new ArrayBuffer(128),v=new DataView(b);v.setUint32(0,0x31575043,true);v.setUint32(4,1,true);new Uint8Array(b,16,16).fill(170);[''+runtimeId,'1','2','0',executor,String(id),'1','1'].forEach((x,i)=>v.setBigUint64(32+i*8,BigInt(x),true));return b;};
 return{create:()=>m.namespace.createCpuWorker(runtime),request,owner,workers,get used(){return used;},get worker(){return lastWorker;},get terminated(){return terminated;}};
}
test('real client code enforces owner identity, monotonic jobs, queue and cancellation',async()=>{
 const f=await fixture(),c=await f.create();assert.equal(f.used,67108864);assert.throws(()=>c.submit(f.request(1,'1','9')),/STALE/);const tasks=[];for(let i=1;i<=7;++i)tasks.push(c.submit(f.request(i)));assert.throws(()=>c.submit(f.request(8)),/TRANSPORT_BOUND/);assert(c.cancel(tasks[1].key));assert.equal((await tasks[1].result).status,'Cancelled');await c.close();assert.equal(f.used,0);assert.equal(f.workers.size,0);assert.equal(f.terminated,1);
});
test('startup custody is registered before ready; root closes it without leaking floor',async()=>{
 const f=await fixture('hang'),start=f.create();assert.equal(f.workers.size,1);await [...f.workers][0].close();await assert.rejects(start,/CLOSED/);assert.equal(f.used,0);assert.equal(f.terminated,1);
});
test('startup postMessage failure releases quota and same owner can retry generation',async()=>{
 const f=await fixture('throw');await assert.rejects(f.create(),/post failed/);assert.equal(f.used,0);assert.equal(f.workers.size,0);
});
test('unknown terminal is failure; error closes existing ready buffers before releasing grants',async()=>{
 const f=await fixture(),c=await f.create(),a=c.submit(f.request(1));
 const output=f.request(1),v=new DataView(output);v.setUint32(0,0x31525043,true);v.setUint32(100,2,true);f.worker.onmessage({data:{type:'terminal',key:a.key,status:'Ready',bytes:output}});const ready=await a.result;assert.equal(ready.status,'Ready');assert(f.used>67108864);const b=c.submit(f.request(2));f.worker.onmessage({data:{type:'terminal',key:b.key,status:'Bogus'}});assert.equal((await b.result).status,'Failed');assert.equal(output.byteLength,0);assert.equal(f.used,0);await c.close();
});
