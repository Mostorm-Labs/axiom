import test from'node:test';import assert from'node:assert/strict';
import{caseRoster,classifyTiming,qualifyFeedback}from'../runners/g5/post_erase_interaction_readiness.mjs';
import{observeCorrectFeedback}from'../runners/g5/erase_exact_canonical_contract.mjs';
test('candidate operation roster and three baseline families are complete',()=>{
 const c=caseRoster();assert.equal(c.filter(x=>x.variant==='old').length,3);
 for(const kind of['erase','pan','zoom','rectangle','sticky','table','image'])assert(c.some(x=>x.variant==='candidate'&&x.kind===kind));
 assert(c.some(x=>x.kind==='ready'&&x.variant==='candidate'));
});
test('feedback observer records delayed correct pixels instead of failing a fixed early probe',async()=>{
 let t=0;
 const r=await observeCorrectFeedback({probe:()=>({positive:t>=160?16:0}),pause:async()=>{t+=16;},now:()=>t,timeoutMs:15000});
 assert.equal(r.status,'QUALIFIED');assert.equal(r.firstCorrectAt,160);assert.equal(r.observations[0].positive,0);
 t=0;const missing=await observeCorrectFeedback({probe:()=>({positive:0}),pause:async()=>{t+=16;},now:()=>t,timeoutMs:32});assert.equal(missing.status,'MISSING');
});
test('T0 includes blocked Up; timings remain findings while actual positive pixels are required',()=>{
 assert.equal(classifyTiming(100,4200).category,'3to5s');assert.equal(classifyTiming(100,6200).category,'over5s');
 for(const value of[NaN,Infinity,99])assert.throws(()=>classifyTiming(100,value));
 assert.equal(qualifyFeedback({admitted:true,positivePixels:0,correctPose:true}).status,'MISSING');
 assert.equal(qualifyFeedback({admitted:true,positivePixels:5,correctPose:true}).status,'QUALIFIED');
 assert.equal(qualifyFeedback({admitted:false,positivePixels:5,correctPose:true}).status,'FAILED');
});
