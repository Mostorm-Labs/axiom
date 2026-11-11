import test from'node:test';import assert from'node:assert/strict';import vm from'node:vm';
import{webConsumer}from'../runners/g5/exact_owner_residency.mjs';
import{entropyStream,installEntropy,emptyViewPoint}from'../runners/g5/exact_fixed_scene_contract.mjs';
test('new owner verification retains fixed40 and valid isolated entropy init',()=>{
 new vm.Script('('+webConsumer.toString()+')');
 const sandbox={crypto:{getRandomValues:v=>v.fill(42)},ArrayBuffer,Uint8Array,Uint32Array,Float32Array,Float64Array,DataView};sandbox.globalThis=sandbox;
 vm.runInNewContext(entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(123);',sandbox);const view=new Uint8Array(16);sandbox.crypto.getRandomValues(view);assert.equal(sandbox.exactFixtureEntropy.transcript().byte_count,16);
 const point=emptyViewPoint({scale:Math.exp(.24),x:600-Math.exp(.24)*600,y:400-Math.exp(.24)*400});assert(point.world[1]>840&&point.view[1]<1010);
});
