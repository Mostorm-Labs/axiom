// Deterministic qualification host: real terminal delivery is deliberately
// delayed. No duration in this test is ordinary performance evidence.
import {installHeldWorkerGate} from '/g5/pending_navigation_held_worker_gate.mjs';
import {readWebGLTarget} from '/g5/live_erase_feedback_pixels.mjs';
import {completeContent} from '/g5/pending_navigation_held_worker_r2_predicate.mjs';
export async function run(q){
 const data={q,status:'STARTED',qualification:'REAL_WORKER_TERMINAL_DELIVERY_HELD_NOT_PERFORMANCE',errors:[],operations:[],snapshots:{},pixels:[],facts:[]};
 const pause=ms=>new Promise(resolve=>setTimeout(resolve,ms));const check=(v,m)=>{if(!v)throw Error(m);};
 const base64=bytes=>{let text='';for(let i=0;i<bytes.length;i+=8192)text+=String.fromCharCode(...bytes.subarray(i,i+8192));return btoa(text);};
 const wrapper=installHeldWorkerGate();let sdk,nav;let phase=null;
 const latest=()=>window.pendingObservation.read().values.filter(v=>v.command==='cpuPending'&&!v.action).at(-1)?.result;
 const fact=label=>{const v={label,at:performance.now(),state:sdk.readState(),pendingFacts:latest(),operations:data.operations.length};data.facts.push(v);return v;};
 const heldPending=()=>{const s=sdk.readState(),p=latest();check(s.pending.intents.length===1&&s.pending.intents[0].kind==='erase'&&p?.state==='WaitingWorker'&&s.generation===data.originalGeneration&&!data.operations.length,'real held pending basis failed');};
 const capture=label=>{heldPending();const rect={x:300,y:300,width:900,height:600};const p=readWebGLTarget(sdk.canvas,{role:'canonical',rect});check(p.metadata.status==='VALID'&&p.bytes,'ROI readback invalid');data.pixels.push({label,at:performance.now(),camera:sdk.readState().camera,pending:sdk.readState().pending,metadata:p.metadata,base64:base64(p.bytes)});fact(label);return p.bytes;};
 const snapshot=async label=>{const s=sdk.readState(),documentRevision=(BigInt(s.generation)+1n).toString(),recoveryFrontier=[8,1,16,1,24,1];const v=await sdk.captureSnapshot({generation:s.generation,documentRevision,recoveryFrontier});data.snapshots[label]={state:s,base64:base64(v.bytes),metadata:{documentRevision,recoveryFrontier,checkpointDigest:v.checkpointDigest,resourceBinding:'1'}};};
 const observe=label=>{if(phase){nav.end(phase);data.phases??=[];data.phases.push(phase);}phase={name:label};nav.begin(phase);return phase;};
 const activeBegan=performance.now();data.activeBegan=activeBegan;
 const wait=async(predicate,message,limit=6000)=>{const start=performance.now();while(!predicate()){if(performance.now()-activeBegan>90000)throw Error('active qualification90s');if(performance.now()-start>limit)throw Error(message);await pause(15);}};

 try{
  const {createRuntime}=await import(q.sdkPrefix+'axiom.mjs');
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:q.documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes:Uint8Array.from(atob(q.fixture.base64),v=>v.charCodeAt(0)),metadata:q.fixture.metadata},onError:e=>data.errors.push({at:performance.now(),code:e.code,message:String(e)}),onOperation:b=>data.operations.push({at:performance.now(),base64:base64(new Uint8Array(b))})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;nav=window.installNav(()=>sdk.canvas);
  await wait(()=>!nav.activity().pending,'initial frame did not quiet');
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await wait(()=>!nav.activity().pending,'tool frame did not quiet');await snapshot('before');data.originalGeneration=sdk.readState().generation;data.build=sdk.readBuildInfo();
  let sequence=1000;const pointer=(kind,x,y)=>{const rect=sdk.canvas.getBoundingClientRect();sequence+=8;const e=new PointerEvent('pointer'+kind,{pointerId:1,pointerType:'mouse',isPrimary:true,buttons:kind==='up'?0:1,button:kind==='move'?-1:0,pressure:kind==='up'?0:.5,clientX:rect.left+x,clientY:rect.top+y,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:sequence});sdk.canvas.dispatchEvent(e);};
  observe('old-camera-transient');pointer('down',430,150);for(let i=1;i<=24;++i)pointer('move',430,150+i*16);pointer('up',430,534);fact('after-up');
  data.held=await Promise.race([wrapper.gate.held(),pause(6000).then(()=>{throw Error('no real terminal held');})]);
  // Actual SDK frames continue naturally while WaitingWorker. Wait for old
  // transient content and two consecutive intervals without new GL writes.
  await wait(()=>completeContent(phase),'old transient never completed');let old=-1,quiet=0;
  await wait(()=>{const writes=phase.colorOps.length;if(writes===old)++quiet;else quiet=0;old=writes;return quiet>=3;},'old camera pixels did not quiet');
  heldPending();const before=capture('before-zoom');
  observe('held-zoom');await sdk.zoomAt(600,400,.7);fact('after-zoom-command');await wait(()=>completeContent(phase),'new camera actual content not rendered');
  // Actual content, followed by the existing natural frame opportunities,
  // witnesses transient drawing. Global retry remains true while pending.
  await pause(40);const zoom=capture('new-camera');
  observe('same-camera-pan-control');await sdk.panBy(0,0);await wait(()=>completeContent(phase),'samecamera reference not rendered');await pause(40);const reference=capture('same-camera-zero-pan');
  data.pixelComparison={old_changed_channels:before.reduce((n,v,i)=>n+(v!==zoom[i]),0),same_camera_mismatched_channels:zoom.reduce((n,v,i)=>n+(v!==reference[i]),0)};
  check(data.pixelComparison.old_changed_channels>0&&data.pixelComparison.same_camera_mismatched_channels===0,'held currentcamera ROI not equal pan reference');heldPending();
  data.released=await wrapper.gate.release();check(data.released,'real terminal not released');
  await wait(()=>{const state=sdk.readState();return !state.pending.intents.length&&state.generation!==data.originalGeneration&&state.generation===state.lastCommit.presentedGeneration;},'one actual atomic commit/canonical did not settle');
  await pause(60);fact('final');await snapshot('final');data.end=sdk.readState();check(data.operations.length===1&&BigInt(data.end.generation)===BigInt(data.originalGeneration)+1n,'not exactly one erase commit');data.status='OBSERVED';
 }catch(error){data.status='FAILED';data.failure=String(error.stack??error);if(sdk)fact('failure');}
 finally{
  if(nav){try{if(phase){nav.end(phase);data.phases??=[];data.phases.push(phase);}nav.restore();data.navRestored=true;}catch(error){data.cleanupError=String(error);}}
  try{if(wrapper.gate.record&&!wrapper.gate.passthrough)await wrapper.gate.release();}catch(error){data.gateCleanupError=String(error);}
  data.gateEvents=wrapper.gate.events;data.exactHeldDeliveries=wrapper.gate.deliveries;
  if(sdk){try{
    await wait(()=>{const state=sdk.readState();return !state.pending.intents.length&&!state.activeContacts&&state.generation===state.lastCommit.presentedGeneration;},'cleanup pending did not drain');
    data.cleanupDrained={at:performance.now(),state:sdk.readState(),operations:data.operations.length};
    await sdk.close();data.closed=true;
   }catch(error){data.closeError=String(error);}
   data.observer=window.idleObservation.read();data.pendingObservation=window.pendingObservation.read();
  }
  try{data.WorkerWrapperRestored=wrapper.restore();}catch(error){data.wrapperError=String(error);}
  document.getElementById('mount').replaceChildren();
 }
 data.activeEnded=performance.now();return data;
}
