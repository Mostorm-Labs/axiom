import factory from './cpu-worker-kernel.mjs';
import {kernelId,limits} from './cpu-worker-build.mjs';
let module,active=null,closed=false;
// Posted tasks yield to the Worker's message queue without the nesting clamp
// of a timer chain. Each task still performs at most the kernel's8/2ms slice.
const turns=new MessageChannel();
turns.port1.onmessage=({data})=>{
 if(active&&active.handle===data.handle&&active.key===data.key)tick(active);
};
function schedule(job){job.queuedAt=performance.now();turns.port2.postMessage({handle:job.handle,key:job.key});}
const terminal=(job,status,extra={})=>{if(job.terminal)return;job.terminal=true;postMessage({type:'terminal',key:job.key,status,...extra},extra.bytes?[extra.bytes]:[]);};
async function initialize(){
 module=await factory();if(module.UTF8ToString(module._axiom_cpu_build())!==kernelId)throw Error('KERNEL_BUILD_MISMATCH');
 postMessage({type:'ready',build:kernelId,heap:module.HEAPU8.buffer.byteLength,limits});
}
function tick(job){
 if(!job||active!==job||job.terminal||closed)return;
 if(!job.started){job.started=true;postMessage({type:'started',key:job.key,at:performance.now(),cursor:module._axiom_cpu_cursor(job.handle)});}
 const started=performance.now();job.yieldMs+=started-job.queuedAt;
 const status=module._axiom_cpu_step(job.handle,limits.stepSegments,limits.stepMicroseconds);job.compute+=performance.now()-started;++job.steps;
 if(status===1){schedule(job);return;}
 if(status===2){const at=performance.now(),p=module._axiom_cpu_result(job.handle),n=module._axiom_cpu_result_size(job.handle);if(!p||n>limits.outputBytes){terminal(job,'Failed',{reason:'OUTPUT_BOUND'});}else{
   const bytes=module.HEAPU8.slice(p,p+n).buffer;terminal(job,'Ready',{bytes,importMs:job.importMs,computeMs:job.compute,yieldMs:job.yieldMs,steps:job.steps,exportMs:performance.now()-at,maxUnionNs:module._axiom_cpu_max_union_ns(job.handle)});
  }}else terminal(job,status===3?'Cancelled':'Failed',{reason:'KERNEL_REJECTED'});
 module._axiom_cpu_release(job.handle);active=null;
}
onmessage=async({data})=>{
 try{
  if(data.type==='init'){if(module)throw Error('DUPLICATE_INIT');await initialize();return;}
  if(!module||closed)throw Error('NOT_READY');
  if(data.type==='cancel'){if(active&&data.key===active.key){const job=active,cursor=module._axiom_cpu_cursor(job.handle);module._axiom_cpu_cancel(job.handle);terminal(job,'Cancelled',{cursor,steps:job.steps,computeMs:job.compute,yieldMs:job.yieldMs,cancelObservedAt:performance.now()});module._axiom_cpu_release(job.handle);active=null;}return;}
  if(data.type==='run'){
   if(active||!(data.bytes instanceof ArrayBuffer)||data.bytes.byteLength>limits.inputBytes)throw Error('BUSY_OR_INPUT_BOUND');
   const at=performance.now(),p=module._malloc(data.bytes.byteLength);if(!p)throw Error('OUT_OF_MEMORY');let handle;
   try{module.HEAPU8.set(new Uint8Array(data.bytes),p);handle=module._axiom_cpu_create(p,data.bytes.byteLength);}finally{module._free(p);}
   if(!handle){postMessage({type:'terminal',key:data.key,status:'Failed',reason:'INVALID_REQUEST'});return;}
   active={key:data.key,handle,terminal:false,compute:0,yieldMs:0,steps:0,importMs:performance.now()-at};schedule(active);return;
  }
  if(data.type==='close'){closed=true;if(active){module._axiom_cpu_cancel(active.handle);terminal(active,'Cancelled');module._axiom_cpu_release(active.handle);active=null;}turns.port1.close();turns.port2.close();postMessage({type:'closed'});close();return;}
  throw Error('UNKNOWN_MESSAGE');
 }catch(error){postMessage({type:'failure',key:data?.key,reason:String(error)});}
};
