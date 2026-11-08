// A fixed offered load. Timing is dispatch-only until joined to real Runtime admission.
const numeric = value => {
 if(typeof value!=='string'||!/^(0|[1-9][0-9]*)$/.test(value)||BigInt(value)>18446744073709551615n)throw Error('invalid lossless ns');return BigInt(value);
};
export async function deliver(events,{originNs,nowNs,sleep,yieldTask=()=>sleep(0),dispatch,maxCatchUp=8}){
 const offsets=events.map(e=>numeric(e.planned_offset_ns));
 if(offsets.some((n,i)=>i&&n<offsets[i-1]))throw Error('plan order');
 if(!Number.isInteger(maxCatchUp)||maxCatchUp<1||maxCatchUp>8)throw Error('catch-up bound');
 const rows=[];let catchUp=0;
 for(let i=0;i<events.length;++i){
  const e=events[i],planned=originNs+offsets[i];
  while(nowNs()<planned)await sleep(Math.max(.1,Number(planned-nowNs())/1e6));
  const sent=nowNs();let error=null;
  try{const pending=dispatch(e);if(pending&&typeof pending.catch==='function')pending.catch(cause=>{row.async_error=String(cause);});}catch(cause){error=String(cause);}
  const returned=nowNs();const row={...e,planned_ns:String(planned),producer_ns:String(sent),dispatch_end_ns:String(returned),late_ns:String(sent>planned?sent-planned:0n),catch_up:sent>planned,status:error?'FAILED':'COMPLETE',status_scope:'DISPATCH_ONLY',error};rows.push(row);
  if(sent>planned){if(++catchUp===maxCatchUp){catchUp=0;await yieldTask();}}else catchUp=0;
 }
 return rows;
}
export function summarize(samples){
 const fields={COMPLETE:'complete_count',FAILED:'failed_count',RIGHT_CENSORED:'censored_count',MISSING:'missing_count',NOT_APPLICABLE:'not_applicable_count'};
 const result={planned_count:samples.length,complete_count:0,failed_count:0,censored_count:0,missing_count:0,not_applicable_count:0,population:'complete_samples_only'};
 const values=[];
 for(const sample of samples){if(!fields[sample.status])throw Error('unknown metric outcome');++result[fields[sample.status]];if(sample.status==='COMPLETE')values.push(numeric(sample.value_ns));}
 values.sort((a,b)=>a<b?-1:a>b?1:0);
 for(const [name,p] of [['p50_ns',.5],['p95_ns',.95],['p99_ns',.99],['max_ns',1]])result[name]=values.length?String(values[Math.max(1,Math.ceil(p*values.length))-1]):null;
 result.tail_status=result.failed_count+result.censored_count+result.missing_count?'INCOMPLETE':values.length?'COMPLETE':'NO_SAMPLES';return result;
}
