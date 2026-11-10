export const WINDOWS=Object.freeze({
 'G5EC1-BASELINE':{seconds:180,rows:1},'G5EC1-DIAGNOSTIC':{seconds:180,rows:1},
 'G5EC1-FINAL-PAIRS':{seconds:360,rows:6},'G5EC1-STRESS':{seconds:180,rows:1}
});
export function tracePoint(round,i){
 if(!Number.isSafeInteger(round)||round<0||!Number.isSafeInteger(i)||i<0||i>160)throw Error('invalid frozen trace index');
 return [420-round*18+(i?7*Math.sin(i*.17):0),160+170*i/160];
}
export function qualifyRounds(rounds,count=3,{stress=false}={}){
 const failures=[];
 if(rounds?.length!==count)failures.push('missing rounds');
 for(const [index,r]of(rounds??[]).entries()){
  if((!r.mainCommitted&&!(stress&&r.noopProven))||r.mainContacts!==0||r.immediateDownContacts!==1||r.tinyContacts!==0)failures.push('contact/commit '+index);
  if(!(stress?[0,1,2]:[1,2]).includes(r.generationDelta)||r.operationDelta!==r.generationDelta)failures.push('operation '+index);
  if((!stress||index===count-1)&&(r.after?.activeContacts!==0||r.after?.generation!==r.after?.lastCommit?.presentedGeneration||r.activity?.pending!==0))failures.push('idle convergence '+index);
 }
 return{status:failures.length?'FAILED':'QUALIFIED',failures};
}
export async function waitForTerminalGeneration({expected,read,pause,now,timeoutMs=25000}){
 const start=now(),want=BigInt(expected);for(;;){const s=read(),actual=BigInt(s.generation);
  if(actual>want)throw Error('unexpected generation after actual UI command');
  if(actual===want&&s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation)return s;
  if(now()-start>timeoutMs)throw Error('actual UI command generation timeout');await pause(10);
 }
}
export async function observeCorrectFeedback({probe,pause,now,timeoutMs=15000,onProbe=()=>{}}){
 const start=now(),observations=[];for(;;){const value=probe(),at=now();observations.push({at,positive:value.positive});onProbe(value,at);
  if(value.positive>0)return{status:'QUALIFIED',firstCorrectAt:at,observations,value};
  if(at-start>=timeoutMs)return{status:'MISSING',firstCorrectAt:null,observations,value};await pause(16);
 }
}
