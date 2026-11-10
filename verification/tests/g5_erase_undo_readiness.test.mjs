import test from 'node:test';import assert from'node:assert/strict';
import{localizeUndo,waitForTerminalGeneration}from'../runners/g5/erase_undo_readiness.mjs';
import fs from'node:fs';import vm from'node:vm';
test('separates blocked Up, stale DOM and real busy history without inventing readiness',()=>{
 const base={up:{start:100,end:12100},runtime:[{at:12100,canUndo:true}],changes:[{at:12105,canUndo:true}],dom:[{at:12107,enabled:true}],heartbeats:[{at:100},{at:12110}]};
 assert.equal(localizeUndo(base).classification,'SYNC_UP_BLOCK');
 assert.equal(localizeUndo({...base,up:{start:100,end:200},dom:[{at:12000,enabled:true}]}).classification,'DOM_NOTIFICATION_LAG');
 assert.equal(localizeUndo({...base,up:{start:100,end:200},runtime:[{at:12100,canUndo:false}],changes:[],dom:[]}).classification,'HISTORY_NOT_READY');
 assert.equal(localizeUndo({...base,runtime:[]}).firstRuntimeTrue,null);
});
test('browser wait helper executes without host imports or runtime dependencies',async()=>{
 const source=fs.readFileSync(new URL('../runners/g5/erase_exact_canonical_contract.mjs',import.meta.url),'utf8');
 assert(!/\bimport\s/.test(source));
 const browser=source.replaceAll('export ','')+';globalThis.wait=waitForTerminalGeneration;';const context=vm.createContext({});vm.runInContext(browser,context);
 const result=await context.wait({expected:'27',read:()=>({generation:'27',activeContacts:0,lastCommit:{presentedGeneration:'27'}}),pause:async()=>{},now:()=>0});
 assert.equal(result.generation,'27');
 const driver=fs.readFileSync(new URL('../runners/g5/erase_undo_readiness.mjs',import.meta.url),'utf8');
 assert(driver.includes("import('/g5/erase_exact_canonical_contract.mjs')"));
 assert(!driver.includes("import('/g5/erase_undo_readiness.mjs')"));
});
test('actual UI asynchronous Undo must advance generation before settle and capture metadata',async()=>{
 let current={generation:'26',activeContacts:0,lastCommit:{presentedGeneration:'26'}};let n=0;
 const result=await waitForTerminalGeneration({expected:'27',read:()=>current,
  pause:async()=>{++n;if(n===1)current={generation:'27',activeContacts:0,lastCommit:{presentedGeneration:'26'}};
    else current={generation:'27',activeContacts:0,lastCommit:{presentedGeneration:'27'}};},now:()=>n,timeoutMs:10});
 assert.equal(result.generation,'27');assert.equal(n,2);
 await assert.rejects(waitForTerminalGeneration({expected:'27',read:()=>({...current,generation:'28'}),pause:async()=>{},now:()=>0}),/unexpected/);
});
