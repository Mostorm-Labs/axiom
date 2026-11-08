import test from 'node:test';
import assert from 'node:assert/strict';
let inspectBoundary,resolveExports;
try{({inspectBoundary,resolveExports}=await import('../runners/g5/ur0/boundary_audit.mjs'));}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
const privateNames=['axiom_g5_trace_control','axiom_g5_trace_bytes','axiom_g5_trace_size'];
test('private observation exports must be exactly the qualifier-only delta',()=>{
 assert.equal(typeof inspectBoundary,'function','boundary verifier unavailable');
 const before=['memory','axiom_js_call'];
 assert.deepEqual(inspectBoundary(before,[...before,...privateNames],['js/axiom.mjs']).errors,[]);
 assert.ok(inspectBoundary([...before,privateNames[0]],[...before,...privateNames],['js/axiom.mjs']).errors.some(x=>x.includes('normal')));
 assert.ok(inspectBoundary(before,[...before,...privateNames,'mutable_runtime'],['js/axiom.mjs']).errors.some(x=>x.includes('delta')));
});
test('release minified exports resolve through actual generated JS bindings',()=>{
 assert.equal(typeof resolveExports,'function','minified export resolver unavailable');
 const js='Module["_axiom_js_call"]=wasmExports["de"];Module["_axiom_g5_trace_control"]=wasmExports["ee"];';
 assert.deepEqual(resolveExports(['de','ee'],js).names,['axiom_g5_trace_control','axiom_js_call']);
 assert.throws(()=>resolveExports(['de'],js),/absent/);
});
test('normal install rejects internal archives and private observation headers',()=>{
 assert.equal(typeof inspectBoundary,'function','boundary verifier unavailable');
 const x=inspectBoundary(['memory'],['memory',...privateNames],['lib/runtime.a','include/canvas/foundation/internal/g5_observation.hpp']);
 assert.equal(x.errors.length,2);
});
