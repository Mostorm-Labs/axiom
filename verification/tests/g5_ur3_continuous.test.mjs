import test from'node:test';import assert from'node:assert/strict';
import{physicalRect,cleanupPoint,afterWheel,typedPlan,positives,livePoint}from'../runners/g5/ur3_continuous_contract.mjs';
test('DPR aware rectangles, camera and in-canvas empty cleanup',()=>{
 for(const dpr of[1,2,4])assert.equal(physicalRect([200,100],dpr).x,200*dpr-2);
 assert.throws(()=>physicalRect([1200,100],4));assert.throws(()=>physicalRect([NaN,0],2));
 let c={scale:1,x:0,y:0};c=afterWheel(c,true,0,-4);c=afterWheel(c,false,8,4);assert(cleanupPoint(c).view[1]<760);
 for(const sequence of[[[false,8,4],[true,0,-4]],[[true,0,-4],[true,0,-4],[true,0,-4],[false,24,12],[true,0,4],[true,0,4],[true,0,4],[false,4,2]]]){
  let camera={scale:1,x:0,y:0};for(const step of sequence)camera=afterWheel(camera,...step);const p=cleanupPoint(camera);assert(p.world[0]<300&&p.world[1]>690);
 }
});
test('typed sticky/table dimensions and negative expected-color witness',()=>{
 const s={primary:'x',transform:[1,0,0,1,600,400],bounds:[599,399,801,601]};
 const p=typedPlan('sticky',s,{available:true,objectId:'x',width:200,height:200},4);assert.equal(p.rect.x,824*4-2);
 assert.equal(positives([255,255,255,255],p.color),0);assert.equal(positives([255,255,128,255],p.color),1);
 assert.throws(()=>typedPlan('sticky',s,{available:true,objectId:'x',width:0,height:0},4));assert(livePoint(719)[0]<1100);
});
