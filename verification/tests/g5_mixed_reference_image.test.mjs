import test from'node:test';import assert from'node:assert/strict';
import{comparePixels}from'../runners/g5/ur0/pixel_oracle.mjs';
test('remaining image color difference cannot acquire material AA exemption',()=>{
 const expected=Uint8Array.from([168,219,166,255]),wrong=Uint8Array.from([53,35,213,255]);
 const result=comparePixels(wrong,expected,Uint8Array.of(0));assert.equal(result.status,'FAILED');assert.equal(result.interior_mismatched_pixels,1);
 assert.equal(comparePixels(expected,expected,Uint8Array.of(0)).status,'QUALIFIED');
});
