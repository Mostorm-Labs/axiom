import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';import {execFileSync} from 'node:child_process';
import {compareExpectedROI} from '../runners/g5/table_continuity_proof_contract.mjs';
const root=new URL('../../',import.meta.url);
test('actual canonical mixed geometry and shared SnapResolver cover all predecessor drags',()=>{
 const fixture=new URL('.aegis/results/G5-exact-history-path-residency-r1/local-web/G5HP1-CONTINUITY/growth-mixed-d2/initial.axsnap',root).pathname;
 const binary=new URL('out/G5-crossframe-history-pressure-r1/feedback-pose',root).pathname;
 for(const [kind,expected]of [['sticky',[[8,4],[40,16],[80,30]]],['table',[[8,4],[40,16],[80,32]]],['image',[[8,0],[40,16],[80,30]]]]){
  const r=JSON.parse(execFileSync(binary,[fixture,kind],{encoding:'utf8',timeout:30000}));assert.equal(r.kind,kind);assert.deepEqual(r.steps.map(v=>v.effectiveDelta),expected);
  if(kind==='image'){assert.equal(r.steps[0].targets[0].kind,11);assert.deepEqual(r.steps[0].targets[0].worldBounds,[929.5,781.5,1170.5,878.5]);assert.equal(r.steps[0].y.correction,-4);assert.equal(r.steps[1].targets.length,0);assert.equal(r.steps[2].targets.length,0);}
 }
});
test('snap-aware material witness rejects unsnapped old/white/HUD and keeps <=2 threshold',()=>{
 const w=JSON.parse(fs.readFileSync(new URL('.aegis/results/G5-crossframe-history-pressure-r1/snap-aware-witness.json',root)));
 assert.equal(compareExpectedROI(w.rgba,w.rgba,w).positive,true);
 assert.equal(compareExpectedROI(Array(w.rgba.length).fill(255),w.rgba,w).positive,false);
 const hud=Array.from({length:w.rgba.length},(_,i)=>[96,85,255,255][i%4]);assert.equal(compareExpectedROI(hud,w.rgba,w).positive,false);
 const wrong=[...w.rgba];wrong[w.requiredPixels[0]*4]+=3;assert.equal(compareExpectedROI(wrong,w.rgba,w).positive,false);
 const d=w.discriminator;assert.equal(compareExpectedROI(Array(d.rgba.length).fill(255),d.rgba,d).positive,false);
});
