import test from'node:test';import assert from'node:assert/strict';
import{installContextObservation}from'../runners/g5/convergence_idle_up_context.mjs';
test('actual WebGL creates and deletes are forwarded and foreign owner is recorded',()=>{
 let calls=0;class GL{constructor(id){this.canvas={id,width:10,height:10};}createTexture(){++calls;return{};}deleteTexture(v){++calls;return v;}}
 globalThis.WebGL2RenderingContext=GL;installContextObservation();const a=new GL('canonical'),b=new GL('preview'),object=a.createTexture();assert.equal(a.deleteTexture(object),object);assert.equal(b.deleteTexture(object),object);const result=convergenceContextObservation.snapshot();assert.equal(calls,3);assert.equal(result.created,1);assert.equal(result.deleted,2);assert.equal(result.mismatches.length,1);assert.equal(result.mismatches[0].owner.canvas,'canonical');assert.equal(result.mismatches[0].current.canvas,'preview');assert(result.mismatches[0].stack);delete globalThis.WebGL2RenderingContext;delete globalThis.convergenceContextObservation;
});
