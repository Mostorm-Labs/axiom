import test from'node:test';import assert from'node:assert/strict';import fs from'node:fs';
import{gunzipSync}from'node:zlib';
import{comparePixels}from'../runners/g5/ur0/pixel_oracle.mjs';
const read=p=>fs.existsSync(p)?fs.readFileSync(p):gunzipSync(fs.readFileSync(p+'.gz'));
const negative=JSON.parse(fs.readFileSync('out/G5-mixed-reference-qualification-r1/material-negatives.json','utf8'));
for(const row of negative.artifacts)test('actual comparePixels rejects '+row.case,()=>{
 const actual=read(row.actual),expected=read(row.expected),band=Uint8Array.from(read(row.edge),x=>x?1:0);
 const comparison=comparePixels(actual,expected,band);assert.equal(comparison.status,'FAILED');assert.ok(comparison.interior_mismatched_pixels>100);
 assert.equal(comparePixels(expected,expected,band).status,'QUALIFIED');
});
