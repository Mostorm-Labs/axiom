import test from 'node:test';import assert from 'node:assert/strict';
import fs from 'node:fs';import vm from 'node:vm';
import {completeContent} from '../runners/g5/pending_navigation_held_worker_r2_predicate.mjs';
test('actual r1 predicate wrongly rejects content when reserved intent requests retry',()=>{
 const source=fs.readFileSync(new URL('../runners/g5/pending_navigation_held_worker_consumer.mjs',import.meta.url),'utf8');
 const line=source.match(/const completeContent=\(\)=>[^;]+;/)[0];
 const context={phase:{colorOps:[{kind:'content'}]},window:{idleObservation:{read:()=>({calls:{values:[{result:{retry:true}}]}})}}};
 vm.createContext(context);vm.runInContext(line+';globalThis.answer=completeContent();',context);assert.equal(context.answer,false);
 assert.equal(completeContent(context.phase,{retry:true}),true);
});
test('r2 uses actual content independently of retry and rejects missing content',()=>{
 assert.equal(completeContent({colorOps:[{kind:'content'}]},{retry:true}),true);
 assert.equal(completeContent({colorOps:[{kind:'content'}]},{retry:false}),true);
 assert.equal(completeContent({colorOps:[]},{retry:false}),false);
 assert.equal(completeContent({colorOps:[{kind:'clear'},{kind:'masked'}]},{retry:true}),false);
 assert.doesNotMatch(completeContent.toString(),/retry/);
});
test('r2 consumer imports tested predicate and drains pending before close',()=>{
 const source=fs.readFileSync(new URL('../runners/g5/pending_navigation_held_worker_r2_consumer.mjs',import.meta.url),'utf8');
 assert.match(source,/import \{completeContent\} from .*r2_predicate/);
 assert.doesNotMatch(source,/retry\s*!==\s*true/);
 assert.match(source,/await wait\(\(\)=>completeContent\(phase\)/);
 assert.ok(source.indexOf('cleanup pending did not drain')<source.indexOf('await sdk.close()'));
});
