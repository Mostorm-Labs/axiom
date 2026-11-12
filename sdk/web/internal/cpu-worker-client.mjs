import {cpuWorkerOwner} from './cpu-worker-owner.mjs';
import {kernelId,limits} from './cpu-worker-build.mjs';
const magic=0x31575043;
export function inspectCpuEnvelope(buffer){
 if(!(buffer instanceof ArrayBuffer)||buffer.byteLength<128||buffer.byteLength>limits.inputBytes)throw Error('INPUT_BOUND');
 const v=new DataView(buffer);if(v.getUint32(0,true)!==magic||![1,2].includes(v.getUint32(4,true))||v.getUint32(96,true)!==buffer.byteLength-128)throw Error('INVALID_ENVELOPE');
 const build=Array.from(new Uint8Array(buffer,16,16),v=>v.toString(16).padStart(2,'0')).join('');if(build!==kernelId)throw Error('KERNEL_BUILD_MISMATCH');
 const words=Array.from({length:8},(_,i)=>v.getBigUint64(32+i*8,true).toString());if([0,1,4,5,6,7].some(i=>words[i]==='0')||(words[2]==='0'&&words[3]==='0'))throw Error('INVALID_IDENTITY');return{words,key:words.join('/')};
}
export async function createCpuWorker(runtime,{url}={}){
 if(typeof ArrayBuffer.prototype.transfer!=='function')throw Error('DETACH_CAPABILITY_REQUIRED');
 const owner=cpuWorkerOwner(runtime),expectedBasis=owner.basis().join('/'),executor=owner.begin();let floor;
 try{floor=owner.reserve(executor,limits.heapBytes);}catch(error){owner.end(executor);throw error;}let worker;
 const pending=new Map(),queue=[],ledger=[],results=new Map(),cleanupErrors=[];let active=null,closed=false,transport=0,readyResolve,readyReject,watermark=0n,basis=null,admitted=0,terminals=0;
 const ready=new Promise((a,b)=>{readyResolve=a;readyReject=b;});
 // A synchronous Worker/postMessage constructor error can precede await ready.
 // Keep that rejection observed while preserving the explicit returned error.
 ready.catch(()=>{});
 let startupTimer;
 const ungrant=token=>owner.release(executor,token);
 const terminal=(job,status,extra={})=>{
  if(job.terminal)return;job.terminal=true;pending.delete(job.key);
  if(job.bytes){try{if(job.bytes.byteLength)job.bytes.transfer(0);}catch(error){cleanupErrors.push(String(error));}job.bytes=null;}
  if(status==='Ready'){job.resultBuffer=extra.bytes;results.set(job.key,job);}else{transport-=job.charge;clean(()=>ungrant(job.grant));}
  ++terminals;ledger.push({key:job.key,status});if(ledger.length>64)ledger.shift();job.resolve({key:job.key,status,...extra,release:()=>api.release(job.key)});if(active===job)active=null;
 };
 const pump=()=>{if(closed||active||!queue.length)return;active=queue.shift();active.sent=performance.now();try{worker.postMessage({type:'run',key:active.key,bytes:active.bytes},[active.bytes]);active.bytes=null;}catch(error){stop('POST_MESSAGE_FAILED:'+String(error));}};
 const clean=fn=>{try{fn();}catch(error){cleanupErrors.push(String(error));}};
 const retire=(status,reason)=>{
  if(closed)return;closed=true;clearTimeout(startupTimer);
  // Stop the executor before any grant can be returned. A failing individual
  // release must not orphan the other jobs, results or initialization floor.
  clean(()=>worker?.terminate());
  for(const job of [...pending.values()])clean(()=>terminal(job,status,{reason}));
  queue.length=0;
  for(const key of [...results.keys()])clean(()=>api.release(key));
  clean(()=>ungrant(floor));clean(()=>owner.detach(api));clean(()=>owner.end(executor));
  readyReject(Error(reason));
 };
 const stop=reason=>retire('Failed',reason);
 const api={
  submit(bytes){
   if(closed)throw Error('CLOSED');const envelope=inspectCpuEnvelope(bytes);
   const jobId=BigInt(envelope.words[5]),identityBasis=envelope.words.slice(0,4).join('/');
   if(envelope.words[4]!==executor||jobId<=watermark||(identityBasis!==expectedBasis)||(basis&&basis!==identityBasis))throw Error('STALE_OR_DUPLICATE_IDENTITY');
   if(pending.size>=limits.queueJobs)throw Error('QUEUE_BOUND');
   const charge=2*bytes.byteLength+2*limits.outputBytes;
   if(transport>limits.transportBytes-charge)throw Error('TRANSPORT_BOUND');
   const grant=owner.reserve(executor,charge);let copy;try{copy=bytes.slice(0);}catch(error){ungrant(grant);throw error;}
   ++admitted;watermark=jobId;basis??=identityBasis;transport+=charge;let resolve;const promise=new Promise(r=>resolve=r);const job={...envelope,bytes:copy,grant,charge,resolve,terminal:false,started:performance.now()};pending.set(job.key,job);queue.push(job);pump();return{key:job.key,result:promise};
  },
  cancel(key){const job=pending.get(key);if(!job)return false;if(job===active){try{worker.postMessage({type:'cancel',key});}catch(error){stop('POST_MESSAGE_FAILED:'+String(error));}}else{queue.splice(queue.indexOf(job),1);terminal(job,'Cancelled');}return true;},
  release(key){const job=results.get(key);if(!job)return false;try{if(job.resultBuffer.byteLength)job.resultBuffer.transfer(0);}finally{job.resultBuffer=null;results.delete(key);transport-=job.charge;ungrant(job.grant);}return true;},
  async close(){retire('Cancelled','CLOSED_DURING_STARTUP');if(cleanupErrors.length)throw Error('CPU_WORKER_CLEANUP:'+cleanupErrors.join(';'));},
  observation(){return{executor,basis:expectedBasis.split('/'),pending:pending.size,active:active?{key:active.key,sent:active.sent,startedReceivedAt:active.startedReceivedAt??null,workerStartedAt:active.workerStartedAt??null}:null,admitted,terminals,transportBytes:transport,ledger:[...ledger],cleanupErrors:[...cleanupErrors],closed,heapBytes:limits.heapBytes,build:kernelId};}
 };
 owner.attach(api);
 try{worker=new Worker(url??new URL('cpu-worker-thread.mjs',owner.assetBaseURL),{type:'module'});
  worker.onmessage=({data})=>{
   if(closed)return;
   if(data.type==='ready'){if(data.build!==kernelId||data.heap!==limits.heapBytes){stop('KERNEL_BUILD_OR_HEAP_MISMATCH');return;}clearTimeout(startupTimer);readyResolve();pump();return;}
   if(data.type==='failure'){stop(data.reason);return;}
   if(data.type==='started'){if(active&&data.key===active.key&&!active.startedReceivedAt){active.startedReceivedAt=performance.now();active.workerStartedAt=data.at;}return;}
   if(data.type==='terminal'){
    const job=active;if(!job||data.key!==job.key)return;
    if(!['Ready','Failed','Cancelled','Stale'].includes(data.status)){stop('INVALID_TERMINAL_STATUS');return;}
    if(data.status==='Ready'){
     if(!(data.bytes instanceof ArrayBuffer)||data.bytes.byteLength<128||data.bytes.byteLength>limits.outputBytes){stop('RESULT_BOUND');return;}
     const view=new DataView(data.bytes),bytes=new Uint8Array(data.bytes);
     if(data.bytes.byteLength<128||view.getUint32(0,true)!==0x31525043||view.getUint32(4,true)!==1||view.getUint32(96,true)!==data.bytes.byteLength-128||view.getUint32(100,true)!==2||job.words.some((x,i)=>view.getBigUint64(32+i*8,true).toString()!==x)||Array.from(bytes.subarray(16,32),v=>v.toString(16).padStart(2,'0')).join('')!==kernelId||bytes.subarray(104,128).some(Boolean)){stop('FOREIGN_RESULT');return;}
    }
    terminal(job,data.status,{...data,totalMs:performance.now()-job.started});pump();
   }
  };worker.onerror=e=>stop('WORKER_MODULE_FAILURE:'+e.message);startupTimer=setTimeout(()=>stop('WORKER_INIT_TIMEOUT'),5000);worker.postMessage({type:'init'});await ready;return api;
 }catch(error){stop(String(error));throw error;}
}
