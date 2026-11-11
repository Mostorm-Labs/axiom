import test from'node:test';import assert from'node:assert/strict';
import{probeUntilContent}from'../runners/g5/table_continuity_proof_contract.mjs';
test('timeout saves complete failing basis/state/lastRGBA before throwing or cleanup',async()=>{
 let time=0,saved=[];const basis={typed:{grid:{width:1},rows:[48,48]},selection:{primary:'table'}};
 const result=await probeUntilContent({basis,now:()=>time,pause:async()=>{time+=16;},guardMs:32,probe:()=>({metadata:{status:'VALID'},rgba:[255,255,255,255],state:{activeContacts:1,generation:'18'},positive:false}),persist:async row=>saved.push(structuredClone(row))});
 assert.equal(result.status,'MISSING');assert.equal(saved.at(-1).status,'MISSING');assert.deepEqual(saved.at(-1).basis,basis);assert.deepEqual(saved.at(-1).last.rgba,[255,255,255,255]);assert.equal(saved.at(-1).last.state.activeContacts,1);assert(saved.length>=3);
});
test('probe exception preserves error and prior pixels; bounded ring never exceeds eight',async()=>{
 let time=0,n=0,saved=[];const result=await probeUntilContent({basis:{kind:'table'},now:()=>time,pause:async()=>time+=1,guardMs:100,probe:()=>{if(++n===12)throw Error('readfailed');return{metadata:{status:'VALID'},rgba:[0,0,0,255],state:{activeContacts:1},positive:false};},persist:async r=>saved.push(structuredClone(r))});
 assert.equal(result.status,'ERROR');assert.match(result.error,/readfailed/);assert.equal(result.recent.length,8);assert.deepEqual(result.last.rgba,[0,0,0,255]);assert.equal(saved.at(-1).status,'ERROR');
});
