import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs';
import {compareExpectedROI} from '../runners/g5/table_continuity_proof_contract.mjs';
const root=new URL('../../',import.meta.url);
const result=new URL('.aegis/results/G5-table-continuity-proof-r1/',root);
const out=new URL('out/G5-table-continuity-proof-r1/',root);
const policy=JSON.parse(fs.readFileSync(new URL('snap-component-policy.json',result)));
const crop=(b,q)=>Buffer.concat(Array.from({length:q.height},(_,y)=>b.subarray(((q.y+y)*2862+q.x)*4,((q.y+y)*2862+q.x+q.width)*4)));
test('canonical snapshot shared SnapResolver predicts image bottom attraction before checking pixels',()=>{
 for(const id of ['oldbab6-mixed-d2','current58f-mixed-d2','current58f-table-localize-d2']){
  const r=JSON.parse(fs.readFileSync(new URL(id+'-snap.json',result)));
  assert.deepEqual(r.steps.map(v=>v.effectiveDelta),[[8,4],[40,16],[80,32]]);
  const last=r.steps.at(-1);assert.equal(last.targets.length,1);
  assert.equal(last.y.target,last.targets[0].id);assert.equal(last.y.sourceFeature,1);assert.equal(last.y.targetFeature,2);assert.equal(last.y.correction,2);
  assert.deepEqual(last.targets[0].worldBounds,[1150,750,1230,830]);assert.deepEqual(last.transform,[1,0,0,1,930,782]);
 }
});
test('snap target component rejects old/white/HUD/material and unsnapped translation using unchanged tolerance',()=>{
 const q=policy.translationDiscriminator;
 const correct=crop(fs.readFileSync(new URL('current58f-mixed-d2-snap-table.rgba',out)),q.rect);
 const old=crop(fs.readFileSync(new URL('old.rgba',out)),q.rect);
 const unsnapped=crop(fs.readFileSync(new URL('new.rgba',out)),q.rect);
 assert.equal(compareExpectedROI(correct,correct,q).positive,true);
 for(const negative of [old,unsnapped,Buffer.alloc(correct.length,255),Buffer.from(Array.from({length:4},()=>[96,85,255,255]).flat()),Buffer.from(Array.from({length:4},()=>[255,0,0,255]).flat())])assert.equal(compareExpectedROI(negative,correct,q).positive,false);
 for(const [win,id]of [['G5TP1-PAIR','oldbab6-mixed-d2'],['G5TP1-PAIR','current58f-mixed-d2'],['G5TP1-LOCALIZE','current58f-table-localize-d2']]){
  const actual=fs.readFileSync(new URL('local-web/'+win+'/'+id+'/table-active-missing.rgba',result));
  assert.equal(compareExpectedROI(crop(actual,q.rect),correct,q).positive,true);
  const primary=crop(actual,policy.rect),expected=crop(fs.readFileSync(new URL('current58f-mixed-d2-snap-table.rgba',out)),policy.rect);
  assert.equal(compareExpectedROI(primary,expected,policy).positive,true);
 }
});
