// Pure fixture/clock/qualification rules. No Runtime state or drawing algorithm.
export function legacyCorpus(){
 const samples=[];let a=0;
 for(let i=0;i<7200;++i){a+=.018+.085*(.5+.5*Math.sin(i*.009));samples.push([340+215*Math.sin(a)+25*Math.sin(a*.17),280+140*Math.cos(a*.71)+20*Math.sin(a*.13),String(1004000000n+BigInt(i)*4000000n),32768,i===0?'down':i===7199?'up':'move']);}
 return{fixture_id:'LEGACY_28_796',samples,divisor:1,content_span_ns:'28796000000',source:'immutable drag_copies.mjs / cumulative_erase.mjs numeric formulas, no quantization'};
}
export function realtimePlan(literal,hz){
 if(![60,120,240].includes(hz)||literal.samples.length!==7200)throw Error('frozen realtime fixture');
 const origin=BigInt(literal.samples[0][2]),limit=240/hz,plan=[];
 const push=samples=>plan.push({event_id:'RT-'+plan.length,interaction_id:'RT',kind:'pointer',phase:samples.at(-1)[4],planned_offset_ns:String(BigInt(samples.at(-1)[2])-origin),samples});
 push([literal.samples[0]]);
 for(let i=1;i<literal.samples.length-1;i+=limit)push(literal.samples.slice(i,Math.min(i+limit,literal.samples.length-1)));
 push([literal.samples.at(-1)]);return plan;
}
export function calibrate(before,after){
 const best=xs=>{if(xs.length!==16)throw Error('16 clock brackets required');return xs.map(x=>{
  const a=BigInt(x.before_ns),b=BigInt(x.after_ns),n=BigInt(x.native_ns);if(b<a)throw Error('clock reversal');
  return{lo:n-b,hi:n-a,width:b-a};
 }).sort((a,b)=>a.width<b.width?-1:a.width>b.width?1:0)[0];};
 const a=best(before),b=best(after),lo=a.lo>b.lo?a.lo:b.lo,hi=a.hi<b.hi?a.hi:b.hi;
 if(lo>hi||a.width>2000000n||b.width>2000000n)return{status:'MISSING',reason:'clock uncertainty exceeds 1ms or offset intervals disjoint',before,after};
 return{status:'QUALIFIED',offset_ns:String((lo+hi)/2n),error_bound_ns:String((hi-lo+1n)/2n),before,after,method:'16 bracket narrowest interval, native minus browser'};
}
const median=xs=>{const a=[...xs].sort((a,b)=>a-b),n=a.length;return n%2?a[(n-1)/2]:(a[n/2-1]+a[n/2])/2;};
export function qualifyOverhead(pairs){
 if(pairs.length!==6||pairs.some(p=>!p.off||!p.on))return{status:'MISSING',reason:'all six independent pairs required'};
 if(pairs.some(p=>['off','on'].some(mode=>['p95_ns','wall_ns'].some(key=>typeof p[mode][key]!=='string'||!/^\d+$/.test(p[mode][key])))))return{status:'MISSING',reason:'missing common measurement'};
 const out={};for(const field of ['p95_ns','wall_ns']){
  const off=pairs.map(p=>Number(p.off[field])),delta=pairs.map(p=>Number(p.on[field])-Number(p.off[field]));
  if([...off,...delta].some(n=>!Number.isFinite(n)))return{status:'MISSING',reason:'missing common measurement'};
  const center=median(off),mad=median(off.map(n=>Math.abs(n-center))),bound=Math.max(field==='p95_ns'?1e6:20e6,.05*center);
  out[field]={off_median_ns:center,off_mad_ns:mad,paired_median_increment_ns:median(delta),limit_ns:bound,noisy:mad>1e6&&mad>.1*center};
 }
 const lost=pairs.some(p=>p.on.critical_loss!==0);
 return{status:lost?'FAILED':Object.values(out).some(v=>v.noisy)?'NOISY':Object.values(out).every(v=>v.paired_median_increment_ns<=v.limit_ns)?'QUALIFIED':'FAILED',critical_loss:lost,measurements:out};
}
