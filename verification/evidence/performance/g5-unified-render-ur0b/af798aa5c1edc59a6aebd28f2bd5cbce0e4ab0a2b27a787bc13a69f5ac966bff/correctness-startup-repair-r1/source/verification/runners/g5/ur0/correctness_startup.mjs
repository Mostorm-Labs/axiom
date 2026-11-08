// Cold correctness host protocol only. Never requests a native frame or mutates a document.
export const startupSpecRef=Object.freeze({path:'.aegis/verification/G5-UR0b-correctness-startup-v1/verification-spec.json',sha256:'7bb77a8fe61739267ac23f46d15b4920e9b3331de7c2054932a1b99a9bcac534'});
const uint=x=>typeof x==='string'&&/^(0|[1-9][0-9]*)$/.test(x)&&BigInt(x)<2n**64n;
const project=s=>({documentId:s.documentId,generation:s.generation,objects:s.objects,activeContacts:s.activeContacts,lastCommit:{ordinal:s.lastCommit?.ordinal,operationId:s.lastCommit?.operationId},target:Object.fromEntries(['runtimeEpoch','attachmentEpoch','viewId','viewEpoch'].map(k=>[k,s.target?.[k]]))});
export function createStartupFrontier({page,documentId,timeOrigin,nowNs,timeoutMs}){
 if(!page||typeof timeOrigin!=='number'||!Number.isFinite(timeOrigin)||timeOrigin<=0||!Number.isFinite(timeoutMs)||timeoutMs<=0||timeoutMs>45000)throw Error('invalid startup page/deadline');
 const created=nowNs();if(!uint(created))throw Error('invalid startup clock');
 const deadline=BigInt(created)+BigInt(Math.floor(timeoutMs*1e6));let count=0,last='0',accepted=null,failure=null,waiting=false;
 const state=read=>{const s=project(read());if(s.documentId!==documentId||!(/^[0-9a-f]{32}$/.test(s.documentId))||/^0+$/.test(s.documentId)||s.generation!=='0'||s.objects!==0||s.activeContacts!==0||s.lastCommit.ordinal!=='0'||s.lastCommit.operationId!=='0'.repeat(32)||Object.values(s.target).some(v=>!uint(v)||BigInt(v)===0n))throw Error('foreign/nonempty startup state');return s;};
 return {
  onFrame(owner,frame){
   if(owner!==page){failure=Error('foreign startup page callback');return;}
   const at=nowNs(),presented=frame?.presented;
   if(!uint(at)||BigInt(at)<BigInt(created)||!uint(presented)||BigInt(presented)<=BigInt(last)){failure=Error('invalid/stale startup accepted callback');return;}
   count++;last=presented;accepted=at;
  },
  async wait({readState,pendingSdk,probe,errors,isCurrent}){
   if(waiting)throw Error('startup frontier already awaited');waiting=true;
   let stopped=false,timer;const initial=state(readState),target=JSON.stringify(initial.target);
   const check=()=>{if(failure)throw failure;if(!isCurrent())throw Error('foreign startup page');if(errors().length)throw Error('startup SDK error');const at=nowNs();if(!uint(at)||BigInt(at)<BigInt(created)||BigInt(at)>=deadline)throw Error('startup deadline');const s=state(readState);if(JSON.stringify(s.target)!==target)throw Error('startup target changed');const queued=pendingSdk();if(!Number.isSafeInteger(queued)||queued<0)throw Error('invalid SDK RAF queue');return{s,at,queued};};
   try{
    const remaining=Number(deadline-BigInt(nowNs()))/1e6;if(remaining<=0)throw Error('startup deadline');
    return await Promise.race([
     (async()=>{for(;;){if(stopped)throw Error('startup deadline');check();await probe();await Promise.resolve();const current=check();if(count>0&&current.queued===0)return{scheme:'CORRECTNESS_STARTUP_QUIESCENT_V1',page_time_origin_ms:timeOrigin,created_at_ns:created,accepted_callback_count:count,last_presented_count:last,accepted_at_ns:accepted,quiescent_at_ns:current.at,pending_sdk_raf:0,public_state:current.s,verification_spec_ref:{...startupSpecRef}};}})(),
     new Promise((_,reject)=>{timer=setTimeout(()=>{stopped=true;reject(Error('startup deadline'));},remaining);})
    ]);
   }finally{stopped=true;clearTimeout(timer);}
  }
 };
}
