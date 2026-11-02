import type {Controller, ControllerOptions} from '../../api/axiom.mjs';
import {AxiomError, SDK_VERSION} from '../../api/axiom.mjs';
import {createTransportController, type TransportReply} from '../../api/internal/transport.mjs';
import NativeAxiom from './specs/NativeAxiom';

export interface NativeController extends Controller {
  /** Opaque native registry token; never a pointer or JS number ID. */
  readonly sessionToken:string;
}
/** Bind this Controller's token to one AxiomCanvasView. */
export async function createNativeController(options:ControllerOptions):Promise<NativeController> {
  if(!options||!/^[0-9a-f]{32}$/.test(options.documentId)||/^0+$/.test(options.documentId))throw new AxiomError('INVALID_ARGUMENT');
  const {onError,restore,...values}=options;
  const sessionToken=await NativeAxiom.createSession(JSON.stringify({...values,...(restore?{restore:restore.metadata}:{})}),Array.from(restore?.bytes??[]));
  if(!/^[0-9a-f]{32}$/.test(sessionToken))throw new AxiomError('INCOMPATIBLE_ENGINE');
  let closed=false,draining=false;
  type Subscription={kind:'state'|'operation';fn:(event:never)=>void;paused:boolean;last?:string;cursor?:unknown;initialized?:boolean};
  const listeners=new Map<number,Subscription>();let serial=0;
  let nativeTail:Promise<unknown>=Promise.resolve(),pendingCalls=0;
  const request=(command:string,args:object={},bytes?:Uint8Array):Promise<TransportReply>=>{
    if(pendingCalls>=256)return Promise.reject(new AxiomError('CAPACITY'));
    const json=JSON.stringify({command,...args}),owned=Array.from(bytes??[]);++pendingCalls;
    const task=nativeTail.then(async()=>{
    const reply=JSON.parse(await NativeAxiom.request(sessionToken,json,owned));
    if(reply.bytes)reply.bytes=new Uint8Array(reply.bytes);return reply;
    });nativeTail=task.catch(()=>{});return task.finally(()=>{--pendingCalls;});
  };
  const dispatchEvents=async()=>{
    if(closed||draining)return;draining=true;
    try{
      for(const [token,s]of listeners){
        if(closed||s.paused||!listeners.has(token))continue;
        try{
          if(s.kind==='state'){
            const r=await request('state');if(!r.ok)throw new AxiomError(r.code??'REJECTED');
            const key=JSON.stringify(r.value);if(key!==s.last&&listeners.has(token)&&!closed){const outcome=s.fn(r.value as never) as unknown as {then?:unknown;catch?:(fn:()=>void)=>unknown}|undefined;if(outcome?.then){outcome.catch?.(()=>{});throw new AxiomError('ASYNC_LISTENER');}s.last=key;}
          }else{
            if(!s.initialized){const r=await request('data',{action:'attach'});if(!r.ok)throw new AxiomError(r.code??'REJECTED');s.cursor=(r.value as {latest:unknown}).latest;s.initialized=true;}
            for(let n=0;n<64&&!closed&&listeners.has(token);++n){
              const r=await request('data',{action:'next',after:s.cursor??null});if(!r.ok)throw new AxiomError(r.code??'REJECTED');if(!r.value||closed||!listeners.has(token))break;
              const metadata=r.value as Record<string,unknown>;const outcome=s.fn({...metadata,owner:sessionToken,bytes:r.bytes} as never) as unknown as {then?:unknown;catch?:(fn:()=>void)=>unknown}|undefined;if(outcome?.then){outcome.catch?.(()=>{});throw new AxiomError('ASYNC_LISTENER');}
              // A thrown listener retains the previous cursor for explicit retry.
              s.cursor=metadata;
            }
          }
        }catch(error){if(error instanceof AxiomError&&error.code==='NOT_ATTACHED')continue;s.paused=true;onError?.(error instanceof AxiomError?error:new AxiomError('EVENT_LISTENER_FAILED',String(error)));}
      }
    }finally{draining=false;}
  };
  // State notifications only; native Host services input and presentation.
  const timer=setInterval(()=>{void dispatchEvents().catch(e=>onError?.(e));},32);
  const controller=createTransportController({owner:sessionToken,version:SDK_VERSION,
    request:(command,args,bytes)=>request(command,args as object,bytes),
    subscribe(kind,fn){if(closed)throw new AxiomError('CLOSED');if(typeof fn!=='function'||listeners.size>=64)throw new AxiomError('INVALID_ARGUMENT');const token=++serial;listeners.set(token,{kind,fn,paused:false});return()=>{listeners.delete(token);};},
    dispatchEvents:async()=>{for(const s of listeners.values())s.paused=false;await dispatchEvents();},
    close:async()=>{await NativeAxiom.close(sessionToken);closed=true;clearInterval(timer);listeners.clear();}
  });
  return Object.freeze({...controller,sessionToken});
}
