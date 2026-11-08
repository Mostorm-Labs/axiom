import test from 'node:test';import assert from 'node:assert/strict';
let api={};try{api=await import('../runners/g5/ur0/pixel_oracle.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
test('independent pixel comparator allows only reference geometric AA edges',()=>{
 assert.equal(typeof api.comparePixels,'function','independent pixel integration missing');
 const expected=Uint8Array.from({length:64},()=>128),actual=expected.slice(),edges=new Uint8Array(16);edges[0]=1;actual[0]=255;
 assert.equal(api.comparePixels(actual,expected,edges).status,'QUALIFIED');actual[8]=255;
 const result=api.comparePixels(actual,expected,edges);assert.equal(result.status,'FAILED');assert.equal(result.interior_mismatched_pixels,1);
});
test('pixel comparator rejects missing/corrupt artifacts instead of counting them as zero',()=>{
 assert.equal(typeof api.comparePixels,'function','independent pixel integration missing');
 assert.throws(()=>api.comparePixels(new Uint8Array(5),new Uint8Array(5),new Uint8Array(1)),/shape/);
 assert.throws(()=>api.comparePixels(new Uint8Array(4),new Uint8Array(4),Uint8Array.of(2)),/boundary/);
});
