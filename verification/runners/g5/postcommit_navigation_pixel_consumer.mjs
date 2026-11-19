// Finite correctness qualification only. Small ROI readbacks occur after
// actual default-target content writes; never mix these timings with ordinary.
import {readWebGLTarget} from '/g5/live_erase_feedback_pixels.mjs';
export async function run(q){
 const data={q,status:'STARTED',errors:[],operations:[],snapshots:{},pixels:[],frameFacts:[]};
 const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));const base64=b=>{let s='';for(let i=0;i<b.length;i+=8192)s+=String.fromCharCode(...b.subarray(i,i+8192));return btoa(s);};
 const {createRuntime}=await import(q.sdkPrefix+'axiom.mjs');
 let sdk,nav,rawRAF,pixelRAF,active=false,samples=0,phase,lastContentAt=-1;
 const capture=label=>{const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect:{x:300,y:300,width:900,height:600}});data.pixels.push({label,at:performance.now(),camera:sdk.readState().camera,state:sdk.readState(),metadata:p.metadata,base64:p.bytes?base64(p.bytes):null});};
 const snapshot=async label=>{const s=sdk.readState(),documentRevision=(BigInt(s.generation)+1n).toString(),recoveryFrontier=[8,1,16,1,24,1];const v=await sdk.captureSnapshot({generation:s.generation,documentRevision,recoveryFrontier});data.snapshots[label]={state:s,base64:base64(v.bytes),metadata:{documentRevision,recoveryFrontier,checkpointDigest:v.checkpointDigest,resourceBinding:'1'}};};
 const stable=async()=>{let quiet=0,last='';const at=performance.now();for(;;){await pause(25);const s=sdk.readState(),a=nav.activity(),key=a.sdkRequests+'/'+a.sdkCallbacks+'/'+s.lastCommit.presentedGeneration;if(!s.activeContacts&&!s.pending.intents.length&&s.generation===s.lastCommit.presentedGeneration&&!a.pending&&key===last)++quiet;else quiet=0;last=key;if(quiet>=3)return s;if(performance.now()-at>12000)throw Error('pixel stable timeout');}};
 try{
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:q.documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes:Uint8Array.from(atob(q.fixture.base64),v=>v.charCodeAt(0)),metadata:q.fixture.metadata},onError:e=>data.errors.push({code:e.code,message:String(e)}),onOperation:b=>data.operations.push({at:performance.now(),base64:base64(new Uint8Array(b))})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;nav=window.installNav(()=>sdk.canvas);
  await stable();await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await stable();await snapshot('before');
  let sequence=1000;const pointer=(kind,x,y)=>{const rect=sdk.canvas.getBoundingClientRect();sequence+=8;const e=new PointerEvent('pointer'+kind,{pointerId:1,pointerType:'mouse',isPrimary:true,buttons:kind==='up'?0:1,button:kind==='move'?-1:0,pressure:kind==='up'?0:.5,clientX:rect.left+x,clientY:rect.top+y,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:sequence});sdk.canvas.dispatchEvent(e);};
  rawRAF=window.requestAnimationFrame;pixelRAF=function(fn){return rawRAF.call(this,function(t){try{return fn(t);}finally{
   if(active&&samples<3){const state=sdk.readState(),last=phase.colorOps.filter(v=>v.kind==='content').at(-1);data.frameFacts.push({at:performance.now(),state,contentAt:last?.started??null});
    if(last&&last.started!==lastContentAt){lastContentAt=last.started;capture('actual-content-'+(++samples));}
   }
  }});};window.requestAnimationFrame=pixelRAF;
  pointer('down',430,150);for(let i=1;i<=24;++i)pointer('move',430,150+i*16);await pause(20);capture('before-up');
  phase={name:'postcommit-pixel-'+q.action};nav.begin(phase);phase.wheels=[];
  data.Up={at:performance.now(),state:sdk.readState()};pointer('up',430,534);data.afterUp={at:performance.now(),state:sdk.readState()};active=true;
  const event=new WheelEvent('wheel',{ctrlKey:q.action==='zoom',deltaX:q.action==='pan'?70:0,deltaY:q.action==='zoom'?-Math.log(.7)/.01:-30,deltaMode:0,clientX:600,clientY:400,bubbles:true,cancelable:true});nav.dispatch(sdk.canvas,event);data.afterNavigation={at:performance.now(),state:sdk.readState()};
  await stable();active=false;nav.end(phase);data.phase=phase;capture('final');await snapshot('final');data.end=sdk.readState();data.status='OBSERVED';
 }catch(error){data.status='FAILED';data.failure=String(error.stack??error);}
 finally{
  active=false;if(pixelRAF){data.pixelRAFRestored=window.requestAnimationFrame===pixelRAF;if(data.pixelRAFRestored)window.requestAnimationFrame=rawRAF;}
  if(sdk){data.observer=window.idleObservation.read();data.pendingObservation=window.pendingObservation.read();try{if(nav){if(!data.phase&&phase)nav.end(phase);nav.restore();}data.navRestored=true;await stable();await sdk.close();data.closed=true;}catch(error){data.closeError=String(error);}}
  document.getElementById('mount').replaceChildren();
 }
 return data;
}
