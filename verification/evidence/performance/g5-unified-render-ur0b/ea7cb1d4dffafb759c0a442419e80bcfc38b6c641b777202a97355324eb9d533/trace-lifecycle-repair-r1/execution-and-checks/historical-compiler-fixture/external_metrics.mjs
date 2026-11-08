import {summarize} from './schedule.mjs';
export function externalMetrics(input){
 const origin=BigInt(input.origin_ns),until=BigInt(input.end_ns),metrics=[];
 const relative=x=>x===null||x===undefined?null:String(BigInt(x)-origin);
 function add(metric_id,e,start,end,status=undefined,reason=null){
  if(status===undefined)status=start==null?'MISSING':end==null?'RIGHT_CENSORED':'COMPLETE';
  if(start!=null&&end!=null&&BigInt(end)<BigInt(start)){status='MISSING';reason='incompatible or reversed clock bounds';}
  const complete=status==='COMPLETE',censored=status==='RIGHT_CENSORED';
  metrics.push({observation_id:'external-'+metrics.length,metric_id,interaction_id:e.interaction_id,event_id:e.event_id??null,start_ns:relative(start),end_ns:complete?relative(end):null,status,value_ns:complete?String(BigInt(end)-BigInt(start)):null,lower_bound_ns:censored?String(until-BigInt(start)):null,reason:reason??(censored?'measurement deadline':status==='MISSING'?'actual handler/qualified feedback unavailable':null)});
 }
 for(const e of input.planned.filter(e=>e.kind==='pointer'||e.kind==='wheel')){
  const h=input.handlers.find(h=>h.event_id===e.event_id),failure=input.errors.find(x=>x.event_id===e.event_id);
  add('PRODUCER_LATE',e,e.planned_ns,e.producer_ns);
  add('INPUT_QUEUE',e,e.planned_ns,h?.enter_ns);
  add('DISPATCH_'+(e.kind==='wheel'?'MOVE':e.phase.toUpperCase()),e,h?.enter_ns,h?.exit_ns,undefined,e.kind==='wheel'?'wheel handler':null);
  if(e.phase==='down'||e.kind==='wheel'||failure)add('FIRST_FEEDBACK',e,h?.enter_ns,null,failure?'FAILED':'MISSING',failure?.error??'requires same-demand Runtime submit join');
 }
 const ticks=(input.rafTimes??[]).filter(r=>BigInt(r.observed_ns)>=origin&&BigInt(r.observed_ns)<=until);
 for(let i=1;i<ticks.length;++i)add('RAF_INTERVAL',{interaction_id:'RAF'},ticks[i-1].observed_ns,ticks[i].observed_ns);
 add('QUIESCENCE',{interaction_id:'POST_INPUT'},input.input_end_ns,input.quiescence_ns,undefined,'external public/RAF condition; GPU convergence is separate');
 const summaries=[...new Set(metrics.map(m=>m.metric_id))].map(metric_id=>({metric_id,...summarize(metrics.filter(m=>m.metric_id===metric_id))}));
 const dispatch=metrics.filter(m=>m.metric_id.startsWith('DISPATCH_'));
 return{scope:'external/common OFF-ON observations, internal joins compiled separately',metrics,summaries,common:{p95_ns:summarize(dispatch).p95_ns,wall_ns:String(until-origin),dispatch_population:dispatch.length},observed_until_ns:String(until-origin)};
}
