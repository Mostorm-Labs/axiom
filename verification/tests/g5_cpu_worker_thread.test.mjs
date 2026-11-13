import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';import vm from 'node:vm';
test('actual Worker code rejects cancelled timer after a successor starts',async()=>{
 const source=fs.readFileSync(new URL('../../sdk/web/internal/cpu-worker-thread.mjs',import.meta.url),'utf8');const timers=[],messages=[],stepped=[];let serial=0;
 const m={HEAPU8:new Uint8Array(67108864),UTF8ToString:()=> 'a'.repeat(32),_axiom_cpu_build:()=>0,_malloc:()=>128,_free:()=>{},_axiom_cpu_create:()=>++serial,_axiom_cpu_cursor:()=>0,_axiom_cpu_step:h=>{stepped.push(h);return 1;},_axiom_cpu_release:()=>1,_axiom_cpu_cancel:()=>1};
 class MessageChannel{constructor(){this.port1={onmessage:null,close(){}};this.port2={postMessage:data=>timers.push(()=>this.port1.onmessage?.({data})),close(){}};}}
 const context=vm.createContext({MessageChannel,Uint8Array,ArrayBuffer,performance,setTimeout:fn=>timers.push(fn),postMessage:v=>messages.push(v),close:()=>{},onmessage:null});
 const kernel=new vm.SyntheticModule(['default'],function(){this.setExport('default',async()=>m);},{context}),build=new vm.SyntheticModule(['kernelId','limits'],function(){this.setExport('kernelId','a'.repeat(32));this.setExport('limits',{stepSegments:8,stepMicroseconds:2000,heapBytes:67108864,inputBytes:1048576,outputBytes:1048576});},{context});for(const x of[kernel,build]){await x.link(()=>{});await x.evaluate();}
 const code=new vm.SourceTextModule(source,{context});await code.link(n=>n.includes('kernel')?kernel:build);await code.evaluate();
 await context.onmessage({data:{type:'init'}});await context.onmessage({data:{type:'run',key:'a',bytes:new ArrayBuffer(128)}});await context.onmessage({data:{type:'cancel',key:'a'}});await context.onmessage({data:{type:'run',key:'b',bytes:new ArrayBuffer(128)}});
 timers.shift()();assert.deepEqual(stepped,[]);timers.shift()();assert.deepEqual(stepped,[2]);assert.equal(messages.filter(v=>v.key==='a'&&v.type==='terminal').length,1);
});
