import test from 'node:test';import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
import {TerminalDeliveryGate} from '../runners/g5/pending_navigation_held_worker_gate.mjs';
const digest=async bytes=>createHash('sha256').update(bytes).digest('hex');
test('holds original terminal event and receiver; exact same bytes delivered once',async()=>{
 const gate=new TerminalDeliveryGate(digest);const bytes=new Uint8Array([1,3,5,7]).buffer;
 const event={data:{type:'terminal',status:'Ready',key:'1/2/3',bytes}},receiver={},calls=[];
 const callback=function(value){calls.push({sameEvent:value===event,sameReceiver:this===receiver,sameBuffer:value.data.bytes===bytes});};
 gate.deliver(event,callback,receiver);assert.equal(calls.length,0);assert.equal((await gate.held()).sha256,await digest(new Uint8Array(bytes)));
 assert.equal(await gate.release(),true);assert.equal(await gate.release(),false);assert.equal(calls.length,1);assert.deepEqual(calls[0],{sameEvent:true,sameReceiver:true,sameBuffer:true});
 gate.deliver({data:{type:'terminal',key:'later',status:'Ready',bytes:new ArrayBuffer(0)}},()=>calls.push('next'),receiver);assert.equal(calls.length,2);
});
test('nonterminal messages pass unchanged; alteration rejects delivery',async()=>{
 const gate=new TerminalDeliveryGate(digest);const calls=[];const started={data:{type:'started'}};gate.deliver(started,value=>calls.push(value),null);assert.equal(calls[0],started);
 const bytes=new Uint8Array([1,2]).buffer;gate.deliver({data:{type:'terminal',status:'Ready',key:'real',bytes}},()=>calls.push('terminal'),null);await gate.held();new Uint8Array(bytes)[0]=9;await assert.rejects(gate.release(),/custody/);assert.equal(calls.length,1);
});
