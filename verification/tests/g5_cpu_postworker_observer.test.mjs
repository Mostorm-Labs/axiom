import test from'node:test';import assert from'node:assert/strict';
import vm from'node:vm';import fs from'node:fs';
test('postWorker observer records only native-tagged diagnostics and existing RAF without scheduling extra work',async()=>{
 const source=fs.readFileSync(new URL('../runners/g5/cpu_postworker_web.mjs',import.meta.url),'utf8');
 const start=source.indexOf('export function installPostworkerObserver()');const end=source.indexOf('\nexport async function consumer',start);
 const code=source.slice(start,end).replace('export ','');let callbacks=[],clock=0;
 class PerformanceObserver{observe(){}disconnect(){}}
 const console=Object.fromEntries(['error','warn','log','info'].map(k=>[k,()=>{}]));
 const context=vm.createContext({PerformanceObserver,console,performance:{now:()=>++clock},requestAnimationFrame:f=>{callbacks.push(f);return callbacks.length;}});context.window=context;
 vm.runInContext(code+';installPostworkerObserver();',context);assert.equal(callbacks.length,0);
 context.postworkerObservation.enable();context.requestAnimationFrame(()=>{clock+=2});assert.equal(callbacks.length,1);callbacks.shift()(20);
 context.console.log('AXIOM_POSTWORKER_COMMIT {"serial":"1"}');context.console.warn('AXIOM_GPU_RETIREMENT {}');context.console.log('other');
 const result=context.postworkerObservation.finish();assert.equal(result.raf.length,1);assert.equal(result.native.length,2);assert.equal(result.loss,0);assert.equal(callbacks.length,0);
});
