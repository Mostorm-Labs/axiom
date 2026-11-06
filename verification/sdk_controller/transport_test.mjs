import assert from 'node:assert/strict';
import test from 'node:test';
const module=await import('../../sdk/api/internal/transport.mjs').catch(e=>{if(e.code==='ERR_MODULE_NOT_FOUND')return {};throw e;});

test('Native controller delegates controls and history through the existing wire vocabulary',async()=>{
  assert.equal(typeof module.createTransportController,'function');
  const requests=[];
  const c=module.createTransportController({owner:'opaque-session',version:'0.3.0',
    async request(command,args={},bytes){requests.push({command,args,bytes});return {ok:true,value:command==='state'?{generation:'9007199254740993',selectionRevision:'18446744073709551615'}:null};},
    subscribe(){return ()=>{};},async close(){}});
  await c.selectBrush('watercolor-lite-v1',3);await c.selectTool('partialEraser');await c.undo();
  assert.deepEqual(requests.filter(r=>r.command==='control').map(r=>r.args),[
    {kind:'preset',profileId:'watercolor-lite-v1',revision:3},{kind:'tool',value:2},{kind:'eraser',mode:2},{kind:'undo'}]);
  assert.equal((await c.readState()).selectionRevision,'18446744073709551615');
  assert(!('request' in c));await assert.rejects(c.selectTool('foreign'),e=>e.code==='INVALID_ARGUMENT');
});

test('Foreign tokens are rejected before native dispatch, bytes copied before queue',async()=>{
  assert.equal(typeof module.createTransportController,'function');
  const requests=[];let resolve;const barrier=new Promise(r=>resolve=r);
  const c=module.createTransportController({owner:'A',version:'0.3.0',
    async request(command,args,bytes){await barrier;requests.push({command,args,bytes});return {ok:true};},subscribe(){return ()=>{};},async close(){}});
  await assert.rejects(c.resources.publish({owner:'B'},new Uint8Array([1])),e=>e.code==='WRONG_RUNTIME');
  assert.equal(requests.length,0);
  const bytes=new Uint8Array([9]);const pending=c.applyRemoteOperation(bytes);bytes[0]=3;resolve();await pending;
  assert.equal(requests[0].bytes[0],9);
});

test('Transport validates queued byte types and gates subscriptions after close',async()=>{
 const c=module.createTransportController({owner:'A',version:'0.3.0',async request(){return {ok:true};},subscribe(){return ()=>{};},async close(){}});
 await assert.rejects(c.applyRemoteOperation([1,2]),e=>e.code==='INVALID_ARGUMENT');
 await c.close();assert.throws(()=>c.subscribeState(()=>{}),e=>e.code==='CLOSED');
});

test('Native table commands capture generation and selection mutations carry both stamps',async()=>{
 const requests=[];
 const c=module.createTransportController({owner:'A',version:'0.3.0',async request(command,args){requests.push({command,args});return {ok:true,value:{generation:'9007199254740993',selectionRevision:'18446744073709551615'}};},subscribe(){return()=>{};},async close(){}});
 await c.table.create({rows:2,columns:2});await c.deleteSelection();
 const table=requests.find(r=>r.command==='table');assert.equal(table.args.generation,'9007199254740993');
 const control=requests.find(r=>r.command==='control');assert.equal(control.args.expectedGeneration,'9007199254740993');assert.equal(control.args.expectedSelectionRevision,'18446744073709551615');
});

test('Portable native controller works without structuredClone in Hermes',async()=>{
 const original=globalThis.structuredClone;globalThis.structuredClone=undefined;
 try{
  const requests=[];
  const c=module.createTransportController({owner:'A',version:'0.3.0',async request(command,args,bytes){requests.push({command,args,bytes});return {ok:true,value:{generation:'1'}};},subscribe(){return()=>{};},async close(){}});
  const input=new Uint8Array([8,4]);const pending=c.applyRemoteOperation(input);input[0]=99;await pending;
  assert.equal(requests[0].bytes[0],8);await c.close();
 }finally{globalThis.structuredClone=original;}
});

test('Native reads reject while close is waiting for its owner',async()=>{
 let release;const barrier=new Promise(r=>release=r);
 const c=module.createTransportController({owner:'A',version:'0.3.0',async request(){return {ok:true,value:{}};},subscribe(){return()=>{};},close:()=>barrier});
 const closing=c.close();await Promise.resolve();
 await assert.rejects(c.readState(),e=>e.code==='BUSY');release();await closing;
});

test('Clipboard controller returns owned bytes and complete owner-bound stamps',async()=>{
 const stamp=['18446744073709551615','9007199254740993','13','17','19','23','29','31'];
 const bytes=new Uint8Array([65,88,67,66]);const requests=[];
 const c=module.createTransportController({owner:'A',version:'test',async request(command,args){requests.push({command,args});return {ok:true,value:{version:1,stamp,roots:[[1,2,3,4]],objectCount:1},bytes};},subscribe(){return()=>{};},async close(){}});
 assert.equal(typeof c.copySelection,'function');
 for(const name of ['cutSelection','duplicateSelection','pasteObjects'])assert.equal(typeof c[name],'function');
 const result=await c.copySelection();assert.deepEqual(result.stamp,{owner:'A',token:stamp});assert.deepEqual(result.bytes,bytes);
 bytes[0]=0;stamp[3]='99';assert.equal(result.bytes[0],65);assert.equal(result.stamp.token[3],'17');
 assert.deepEqual(requests,[{command:'clipboard',args:{action:'copy'}}]);
 await assert.rejects(c.cutSelection({owner:'B',token:result.stamp.token}),e=>e.code==='WRONG_RUNTIME');assert.equal(requests.length,1);
});

test('Clipboard mutations retain captured inputs while queued and propagate stale receipts',async()=>{
 const stamp=['18446744073709551615','13','17','19','9007199254740993','29','31','37'];
 const requests=[];let release;const barrier=new Promise(r=>release=r);
 const c=module.createTransportController({owner:'A',version:'test',async request(command,args,bytes){
  if(command==='remote')await barrier;
  if(command==='multi')return {ok:true,value:{stamp}};
  requests.push({command,args,bytes});
  if(args.action==='cut')return {ok:false,code:'STALE_CLIPBOARD'};
  return {ok:true,changed:true};
 },subscribe(){return()=>{};},async close(){}});
 assert.equal(typeof c.pasteObjects,'function');const hold=c.applyRemoteOperation(new Uint8Array([1]));
 const bytes=new Uint8Array([65,88,67,66]);const offset=[24,32];const paste=c.pasteObjects(bytes,{offset});const duplicate=c.duplicateSelection();
 bytes[0]=0;offset[0]=99;await new Promise(r=>setImmediate(r));stamp[4]='999';release();await Promise.all([hold,paste,duplicate]);
 const mutations=requests.filter(r=>r.command==='clipboard');assert.deepEqual(mutations.map(r=>r.args.action),['paste','duplicate']);
 assert.equal(mutations[0].bytes[0],65);assert.deepEqual(mutations[0].args.offset,[24,32]);assert.equal(mutations[0].args.stamp[4],'9007199254740993');assert.deepEqual(mutations[1].args.stamp,mutations[0].args.stamp);
 await assert.rejects(c.cutSelection({owner:'A',token:stamp}),e=>e.code==='STALE_CLIPBOARD');
});

test('Clipboard rejects malformed options and oversized bytes without native dispatch',async()=>{
 let requests=0;const c=module.createTransportController({owner:'A',version:'test',async request(){++requests;return {ok:true};},subscribe(){return()=>{};},async close(){}});
 assert.equal(typeof c.pasteObjects,'function');
 for(const bytes of [[1,2],new Uint8Array(16777217)])await assert.rejects(c.pasteObjects(bytes),e=>e.code==='INVALID_ARGUMENT');
 for(const options of [{offset:[1]}, {offset:[1,Infinity]}, {internal:true},null]){
  await assert.rejects(c.duplicateSelection(options),e=>e.code==='INVALID_ARGUMENT');
  await assert.rejects(c.pasteObjects(new Uint8Array([1]),options),e=>e.code==='INVALID_ARGUMENT');
 }
 assert.equal(requests,0);
});
