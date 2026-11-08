import test from 'node:test';
import assert from 'node:assert/strict';
const module = await import('../runners/g5/ur0/trace_validation.mjs').catch(()=>null);
const validate = value => { assert.equal(typeof module?.validateTimeline,'function','dynamic identity validator is unavailable');return module.validateTimeline(value); };
const basis={runtime:'11',document:'99999999999999999999999999999999',lifetime:'1',attachment:'2',documentEpoch:'4',resource:'7',view:'1',frame:'23',surface:'2',metrics:'3',sceneGeneration:'9',semanticGeneration:'9',sceneRevision:'44',camera:'4',work:'10',operation:'11111111111111111111111111111111',commitRevision:'6'};
const fixture=()=>({commits:[{...basis,beforeGeneration:'8',afterGeneration:'9',at_ns:'100'}],scenes:[{...basis,at_ns:'102'}],frames:[{...basis,purpose:'canonical',at_ns:'104'}],completions:[{...basis,qualification:'QUALIFIED',at_ns:'110'}],decisions:[{id:'d1',job:'j1',basis:'b1',origin:'CURRENT_BRANCH',outcome:'PreparedGpuDraw'}],leaves:[{decision:'d1',job:'j1',basis:'b1',route:'CAN-PREP-V4',target:'GPU'}],loss:{critical:0,unknown:0}});
test('real commit/Scene/frame basis qualifies without confusing scene revision and semantic generation',()=>assert.equal(validate(fixture()).valid,true));
test('foreign or stale completion never qualifies a different execution',()=>{
 for(const field of ['runtime','document','lifetime','attachment','documentEpoch','resource','view','frame','surface','metrics','work']){
  const f=fixture();f.completions[0][field]=field==='document'?'0'.repeat(32):'90';assert.equal(validate(f).valid,false,field);
 }
});
test('Scene generation must match committed generation, not merely the static schema shape',()=>{
 const f=fixture();f.scenes[0].sceneGeneration='8';assert.equal(validate(f).valid,false);
});
test('requested GPU and matching numerical IDs cannot hide wrong leaf or dropped identity',()=>{
 const cpu=fixture();cpu.leaves[0].target='CPU';assert.equal(validate(cpu).valid,false);
 const foreign=fixture();foreign.leaves[0].basis='other';assert.equal(validate(foreign).valid,false);
 const dropped=fixture();dropped.loss.critical=1;assert.equal(validate(dropped).valid,false);
});
