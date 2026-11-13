import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';import vm from 'node:vm';
const root=fs.readFileSync(new URL('../../sdk/web/axiom.mjs',import.meta.url),'utf8');
test('actual root private asset binding uses normalized relative assets',()=>{
 const line=root.split('\n').find(l=>l.includes('bindCpuWorkerOwner(api,'));let owner;
 vm.runInNewContext(line,{api:{},assets:new URL('../assets/','http://localhost/app/axiom.mjs'),assetBaseURL:'../assets/',URL,bindCpuWorkerOwner:(_,v)=>owner=v,cpuWorkers:new Set()});
 assert.equal(owner.assetBaseURL.href,'http://localhost/assets/internal/');
});
test('actual root close retains private binding after publication-pending and retires only on successful retry',async()=>{
 const start=root.indexOf('      async close(){'),end=root.indexOf('\n    };',start);const method=root.slice(start,end).trim();
 let unbound=0,attempts=0;const context={AxiomError:class extends Error{constructor(code){super(code);this.code=code;}},unbindCpuWorkerOwner:()=>++unbound,call:(command)=>{if(command!=='close')return {};if(++attempts===1){const e=new Error('PUBLICATION_PENDING');e.code='PUBLICATION_PENDING';throw e;}},schedule:()=>{},cleanup:()=>{},tail:Promise.resolve(),text:{finish:async()=>true,dispose:()=>{}},cpuWorkers:new Set(),disposeInput:undefined,closed:false,closing:false,closeBarrier:false,pendingRestart:null,pendingReplacement:null,module:{}};
 vm.createContext(context);vm.runInContext('const api={'+method+'};globalThis.api=api;',context);await assert.rejects(context.api.close(),/PUBLICATION_PENDING/);assert.equal(unbound,0);assert.equal(context.closed,false);assert.equal(context.closing,false);await context.api.close();assert.equal(unbound,1);assert.equal(context.closed,true);
});
