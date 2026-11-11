import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {compareExpectedROI} from '../runners/g5/table_continuity_proof_contract.mjs';
const root=new URL('../../out/G5-table-continuity-proof-r1/',import.meta.url);
const witness=JSON.parse(fs.readFileSync(new URL('witness.json',root)));
test('native uncached table target qualifies eight non-AA core pixels',()=>{
 assert.equal(compareExpectedROI(witness.rgba,witness.rgba,witness).positive,true);
 for(const name of ['oldRGBA'])assert.equal(compareExpectedROI(witness[name],witness.rgba,witness).positive,false);
 const white=Array(64).fill(255);
 const hud=white.map((v,i)=>i%4===3?255:[38,128,255][i%4]);
 const wrongMaterial=white.map((v,i)=>i%4===3?255:[255,0,0][i%4]);
 for(const pixels of [white,hud,wrongMaterial])assert.equal(compareExpectedROI(pixels,witness.rgba,witness).positive,false);
 const outsideTolerance=[...witness.rgba];outsideTolerance[16]+=3;
 assert.equal(compareExpectedROI(outsideTolerance,witness.rgba,witness).positive,false);
});
test('original four-pixel witness is independently valid in native reference',()=>{
 const width=2862,p=fs.readFileSync(new URL('new.rgba',root)),old=fs.readFileSync(new URL('old.rgba',root));
 const {x,y}=witness.originalRect;
 for(let k=0;k<4;++k){const off=(y*width+x+k)*4;
  assert.deepEqual([...p.subarray(off,off+4)],witness.originalColor);
  assert.deepEqual([...old.subarray(off,off+4)],[255,255,255,255]);
 }
 // A wrong translated row at +2 CSS pixels has white pixels at the frozen band.
 const shifted=[];
 for(let dy=0;dy<4;++dy)for(let dx=0;dx<4;++dx){const off=((witness.rect.y+dy+4)*width+witness.rect.x+dx)*4;shifted.push(...p.subarray(off,off+4));}
 assert.equal(compareExpectedROI(shifted,witness.rgba,witness).positive,false);
});
