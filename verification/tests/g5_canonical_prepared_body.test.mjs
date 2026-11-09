import test from 'node:test';import assert from 'node:assert/strict';
import {summarizeCanonicalPrepared,qualifyCaseConsumers} from '../runners/g5/canonical_prepared_body.mjs';
const work=(purpose,event_index,service='one')=>({purpose,event_index,service,canonical_queries:purpose==='canonical'?1:0,direct_queries:purpose==='edit'?1:0,job_admissions:0,advances:0,support_knots:0,distance_inputs:0,compile_inputs:0,path_primitives:0,max_advance_knots:32});
const execution=(purpose,event_index)=>({purpose,event_index,canonical_prepared_draws:purpose===0?1:0,direct_prepared_draws:purpose===1?1:0,actual_gpu_tiles:1,actual_cpu_tiles:0});
test('requires actual canonical and Direct draws on the same owner service',()=>{
 assert.equal(summarizeCanonicalPrepared([work('canonical',1),work('edit',4)],[execution(0,3),execution(1,6)]).status,'OBSERVED');
 assert.equal(summarizeCanonicalPrepared([work('canonical',1),work('edit',4,'other')],[execution(0,3),execution(1,6)]).status,'MISSING');
});
test('lookup-only facade and navigation/ink observations cannot prove Direct consumption',()=>{
 assert.equal(summarizeCanonicalPrepared([work('canonical',1),work('edit',4)],[execution(0,3),{...execution(1,6),direct_prepared_draws:0}]).status,'MISSING');
 assert.equal(summarizeCanonicalPrepared([work('navigation',1),work('edit',4)],[execution(0,3),execution(1,6)]).status,'MISSING');
});
test('refuses oversized or invalid source-work quanta',()=>{
 for(const value of [33,-1,NaN])assert.equal(summarizeCanonicalPrepared([{...work('canonical',1),max_advance_knots:value},work('edit',4)],[execution(0,3),execution(1,6)]).status,'MISSING');
});
test('a Raster scope is never proof of GPU prepared execution',()=>{
 assert.equal(summarizeCanonicalPrepared([work('canonical',1),work('edit',4)],[{...execution(0,3),actual_gpu_tiles:0,actual_cpu_tiles:1},execution(1,6)]).status,'MISSING');
});
test('work before a later scope cannot be paired backwards',()=>{
 assert.equal(summarizeCanonicalPrepared([work('canonical',5),work('edit',7)],[execution(0,3),execution(1,9)]).status,'MISSING');
});
test('reports unique preparation separately from visible replay and validation',()=>{
 const rows=[{...work('canonical',1),job_admissions:1,advances:2,support_knots:32,compile_inputs:32},work('edit',4)];
 const summary=summarizeCanonicalPrepared(rows,[execution(0,3),execution(1,6)]);
 assert.equal(summary.work_totals.compile_inputs,32);assert.equal(summary.work_totals.advances,2);assert.equal(summary.work_totals.max_advance_knots,32);
});

test('cold canonical cases require canonical consumption while editing requires both',()=>{
 const w=[work('canonical',1)],e=[execution(0,3)];
 assert.equal(qualifyCaseConsumers('S0-canonical',w,e).status,'OBSERVED');
 assert.equal(qualifyCaseConsumers('S0-drag',w,e).status,'MISSING');
});
