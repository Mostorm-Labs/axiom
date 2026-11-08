import test from 'node:test';import assert from 'node:assert/strict';
let api={};try{api=await import('../runners/g5/ur0/external_metrics.mjs');}catch(e){if(e.code!=='ERR_MODULE_NOT_FOUND')throw e;}
test('common OFF/ON metrics retain rejected feedback in the planned denominator',()=>{
 assert.equal(typeof api.externalMetrics,'function','external metric compiler unavailable');
 const input={origin_ns:'100',end_ns:'1000',input_end_ns:'600',quiescence_ns:null,planned:[{kind:'pointer',phase:'down',event_id:'E1-0',interaction_id:'E1',planned_ns:'100',producer_ns:'200'}],handlers:[{event_id:'E1-0',enter_ns:'210',exit_ns:'400'}],errors:[{event_id:'E1-0',error:'INPUT_REJECTED'}],rafTimes:[]};
 const result=api.externalMetrics(input);const q=result.metrics.find(m=>m.metric_id==='INPUT_QUEUE');assert.equal(q.value_ns,'110');
 const feedback=result.metrics.find(m=>m.metric_id==='FIRST_FEEDBACK');assert.equal(feedback.status,'FAILED');assert.equal(feedback.value_ns,null);
 assert.equal(result.summaries.find(s=>s.metric_id==='FIRST_FEEDBACK').failed_count,1);
});
test('an incomplete dispatch has a lower bound and is not a zero successful sample',()=>{
 const result=api.externalMetrics({origin_ns:'100',end_ns:'1000',input_end_ns:'600',quiescence_ns:null,planned:[{kind:'pointer',phase:'up',event_id:'E0-up',interaction_id:'E0',planned_ns:'100',producer_ns:'200'}],handlers:[{event_id:'E0-up',enter_ns:'210',exit_ns:null}],errors:[],rafTimes:[]});
 const m=result.metrics.find(m=>m.metric_id==='DISPATCH_UP');assert.equal(m.status,'RIGHT_CENSORED');assert.equal(m.lower_bound_ns,'790');
 assert.equal(result.common.p95_ns,null);
});
