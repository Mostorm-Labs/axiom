import test from'node:test';import assert from'node:assert/strict';import vm from'node:vm';
import{SEEDS,entropyStream,installEntropy,emptyViewPoint,requireCanonicalPair}from'../runners/g5/exact_fixed_scene_contract.mjs';
test('isolated entropy reproduces full stream, resets and distinct seeds differ',()=>{
 const sample=seed=>{const r=entropyStream(seed),a=new Uint32Array(4),b=new Uint8Array(7);assert.equal(r.fill(a),a);r.fill(b);return{bytes:[...new Uint8Array(a.buffer),...b],transcript:r.transcript()};};
 assert.deepEqual(sample(SEEDS[0]),sample(SEEDS[0]));assert.notDeepEqual(sample(SEEDS[0]).bytes,sample(SEEDS[1]).bytes);
 assert.deepEqual(sample(SEEDS[0]).transcript.call_lengths,[16,7]);assert.throws(()=>entropyStream(0));assert.throws(()=>entropyStream(1).fill(new Float32Array(1)));
});
test('hook scope has reversible untouched fallback and no native API patch',()=>{
 const original=view=>{view.fill(42);return view;};const sandbox={crypto:{getRandomValues:original},ArrayBuffer,Uint8Array,Uint32Array,Float32Array,Float64Array,DataView};sandbox.globalThis=sandbox;
 vm.runInNewContext(entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(123);',sandbox);
 const first=new Uint8Array(4);sandbox.crypto.getRandomValues(first);assert.notDeepEqual([...first],[42,42,42,42]);sandbox.exactFixtureEntropy.restore();const second=new Uint8Array(4);sandbox.crypto.getRandomValues(second);assert.deepEqual([...second],[42,42,42,42]);
});
test('cleanup is proven outside world content and canonical forks cannot qualify',()=>{
 const v=emptyViewPoint({scale:Math.exp(.24),x:-162.75,y:-108.5});assert(v.world[1]>840&&v.world[0]<800);assert(v.view[0]>0&&v.view[0]<1431&&v.view[1]>0&&v.view[1]<1010);assert.throws(()=>emptyViewPoint({scale:NaN,x:0,y:0}));
 const p={objects_equal:true,manifest_background_equal:true,document_digest_equal:true};assert(requireCanonicalPair(p,p));for(const key of Object.keys(p))assert.throws(()=>requireCanonicalPair({...p,[key]:false},p));
});
