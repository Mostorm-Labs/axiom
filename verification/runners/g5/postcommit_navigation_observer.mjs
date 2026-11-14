import {installPendingObserver} from './pending_navigation_progress_observer.mjs';
// Native timing variant only. Bounded records and existing calls, no polling,
// frame requests, geometry or changes to SDK transport business behavior.
export function installPostcommitObserver(){
 installPendingObserver();const records=[];let lost=0;
 for(const name of ['log','info','warn','error']){
  const original=console[name];console[name]=function(...args){
   const text=args.map(value=>typeof value==='string'?value:String(value)).join(' ');
   if(/AXIOM_IF_EVENT.* nc_|AXIOM_FRAME_TIMING |AXIOM_FRAME_TIMING_LOSS/.test(text)){
    if(records.length<8192)records.push({at:performance.now(),phase:window.idleObservation.summary().phase,text});else ++lost;
   }
   return original.apply(this,args);
  };
 }
 window.postcommitObservation={read:()=>({values:[...records],lost})};
}
