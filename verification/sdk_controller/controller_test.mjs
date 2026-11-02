import assert from 'node:assert/strict';
import test from 'node:test';
const api=await import('../../sdk/api/axiom.mjs').catch(e=>{if(e.code==='ERR_MODULE_NOT_FOUND')return {};throw e;});
import * as web from '../../sdk/web/axiom.mjs';

test('Web and native share public errors and an async controller factory',()=>{
  assert.equal(typeof web.createController,'function');
  assert.equal(web.AxiomError,api.AxiomError);
});

test('Controller copies queued arguments and preserves nested async reads',async()=>{
  const {adaptController}=await import('../../sdk/api/internal/controller.mjs');
  let release;const barrier=new Promise(r=>release=r);const seen=[];
  const engine={version:api.SDK_VERSION,async setInk(value){await barrier;seen.push(value);return {ok:true};},readState(){return {generation:'18446744073709551615'};},resources:{readBytes(){return new Uint8Array([7,8]);}},close:async()=>{}};
  const c=adaptController(engine);const color=[0,0,1,1];
  const task=c.setInk({color});color[0]=1;release();await task;
  assert.deepEqual(seen[0].color,[0,0,1,1]);
  assert(c.readState() instanceof Promise);assert.equal((await c.readState()).generation,'18446744073709551615');
  assert.deepEqual(await c.resources.readBytes({}),new Uint8Array([7,8]));
  assert(!('canvas' in c));assert(!('call' in c));
});

test('Unsubscribe and close gate queued event callbacks',async()=>{
  const {adaptController}=await import('../../sdk/api/internal/controller.mjs');
  let stateListener;let deliveries=0;
  const engine={version:api.SDK_VERSION,subscribeState(fn){stateListener=fn;return ()=>{};},close:async()=>{},readState(){return {};}};
  const c=adaptController(engine);const off=c.subscribeState(()=>++deliveries);
  queueMicrotask(()=>stateListener({generation:'1'}));off();await Promise.resolve();assert.equal(deliveries,0);
  c.subscribeState(()=>++deliveries);queueMicrotask(()=>stateListener({generation:'2'}));await c.close();await Promise.resolve();assert.equal(deliveries,0);
  await assert.rejects(c.readState(),e=>e.code==='CLOSED');
});

test('Failed close retains the controller so the data owner can release custody',async()=>{
  const {adaptController}=await import('../../sdk/api/internal/controller.mjs');
  let fail=true;
  const c=adaptController({version:api.SDK_VERSION,close:async()=>{if(fail)throw new api.AxiomError('PUBLICATION_PENDING');},data:{status(){return {pending:1};}}});
  await assert.rejects(c.close(),e=>e.code==='PUBLICATION_PENDING');assert.equal((await c.data.status()).pending,1);
  fail=false;await c.close();await c.close();
});

test('Close rejects new commands while the underlying owner is draining',async()=>{
 const {adaptController}=await import('../../sdk/api/internal/controller.mjs');
 let release;const wait=new Promise(r=>release=r);
 const c=adaptController({version:api.SDK_VERSION,close:()=>wait,undo:async()=>({ok:true})});
 const closing=c.close();await assert.rejects(c.undo(),e=>e.code==='BUSY');release();await closing;
});


test('Listener rejection propagates to SDK dispatch to retain the operation cursor',async()=>{
 const {adaptController}=await import('../../sdk/api/internal/controller.mjs');
 let listener;const c=adaptController({version:api.SDK_VERSION,subscribeOperations(fn){listener=fn;return ()=>{};},close:async()=>{}});
 const fault=new Error('transfer failed');c.subscribeOperations(()=>{throw fault;});
 assert.throws(()=>listener({bytes:new Uint8Array([1])}),e=>e===fault);
});
