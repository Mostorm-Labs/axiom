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
