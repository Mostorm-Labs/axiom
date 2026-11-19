// Root SDK inputs only. Every lane restores the same fixture and uses the same
// immediate Up->action ordering; no semantic or geometry implementation in JS.
export async function run(q) {
  const data = {q, errors: [], operations: [], frames: [], pixels: [], snapshots: {}, status: 'STARTED'};
  const pause = ms => new Promise(resolve => setTimeout(resolve, ms));
  const base64 = bytes => {let text=''; for(let i=0;i<bytes.length;i+=8192)text+=String.fromCharCode(...bytes.subarray(i,i+8192)); return btoa(text);};
  const {createRuntime} = await import(q.sdkPrefix + 'axiom.mjs');
  const sdk = await createRuntime({mount: document.getElementById('mount'), documentId: q.documentId,
    builtinFonts: false, permission: 'localSingleUser',
    restore: {bytes: Uint8Array.from(atob(q.fixture.base64), value=>value.charCodeAt(0)), metadata: q.fixture.metadata},
    onError: error=>data.errors.push({at:performance.now(),code:error.code,message:String(error)}),
    onOperation: bytes=>data.operations.push({at:performance.now(),base64:base64(new Uint8Array(bytes))}),
    onFrame: value=>data.frames.push({at:performance.now(),value})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  const nav=window.installNav(()=>sdk.canvas);
  const latestPending=()=>window.pendingObservation.read().values.filter(v=>v.command==='cpuPending'&&!v.action).at(-1)?.result??null;
  let sequence=1000,readback=null,rawRAF=null,pixelRAF=null,pixelsActive=false,pixelCount=0;
  const pixels=label=>{if(!readback)return;
    const p=readback(sdk.canvas,{role:'canonical'});
    const preview=document.getElementById('mount').querySelector('canvas[id$="-preview"]');
    const overlay=preview?readback(preview,{role:'preview'}):null;
    data.pixels.push({label,at:performance.now(),camera:sdk.readState().camera,pending:sdk.readState().pending,facts:latestPending(),metadata:p.metadata,base64:p.bytes?base64(p.bytes):null,
      preview:overlay?{metadata:overlay.metadata,visibility:getComputedStyle(preview).visibility,base64:overlay.bytes?base64(overlay.bytes):null}:null});};
  const pointer=(phase,x,y)=>{sequence+=8;const rect=sdk.canvas.getBoundingClientRect();const event=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',isPrimary:true,buttons:phase==='up'?0:1,button:phase==='move'?-1:0,pressure:phase==='up'?0:.5,clientX:rect.left+x,clientY:rect.top+y,bubbles:true,cancelable:true});Object.defineProperty(event,'timeStamp',{value:sequence});sdk.canvas.dispatchEvent(event);};
  const stable=async()=>{let quiet=0,last='';const start=performance.now();for(;;){await pause(25);const state=sdk.readState(),activity=nav.activity(),key=activity.sdkRequests+'/'+activity.sdkCallbacks+'/'+state.lastCommit.presentedGeneration;if(!state.activeContacts&&!state.pending?.intents.length&&state.generation===state.lastCommit.presentedGeneration&&!activity.pending&&key===last)++quiet;else quiet=0;last=key;if(quiet>=3)return {at:performance.now(),state};if(performance.now()-start>12000)throw Error('stable12s:'+JSON.stringify({state,activity}));}};
  const snapshot=async label=>{const state=sdk.readState();const documentRevision=(BigInt(state.generation)+1n).toString(),recoveryFrontier=[8,1,16,1,24,1];const value=await sdk.captureSnapshot({generation:state.generation,documentRevision,recoveryFrontier});data.snapshots[label]={state,base64:base64(value.bytes),metadata:{documentRevision,recoveryFrontier,checkpointDigest:value.checkpointDigest,resourceBinding:'1'}};};
  const phase={name:'immediate-'+q.action};
  try {
    if(q.pixel){({readWebGLTarget:readback}=await import('/g5/live_erase_feedback_pixels.mjs'));rawRAF=window.requestAnimationFrame;pixelRAF=function(fn){return rawRAF.call(this,function(t){try{return fn(t);}finally{if(pixelsActive&&pixelCount<3)pixels('opportunity-'+(++pixelCount));}});};window.requestAnimationFrame=pixelRAF;}
    await stable();data.build=sdk.readBuildInfo();await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});await stable();await snapshot('before-erase');
    pointer('down',430,150);for(let i=1;i<=24;++i)pointer('move',430,150+i*16);
    // Consume one naturally scheduled active-transient opportunity, without
    // waiting for canonical/erase completion. Up and action remain same task.
    await pause(20);pixels('before-up');data.beforeUp={at:performance.now(),state:sdk.readState(),facts:latestPending()};
    window.idleObservation.phase(phase.name);nav.begin(phase);phase.wheels=[];
    phase.upStart=performance.now();pointer('up',430,534);phase.upEnd=performance.now();
    data.atUp={at:performance.now(),state:sdk.readState(),facts:latestPending()};
    pixelsActive=!!q.pixel;
    if(q.action==='zoom'||q.action==='pan'){
      const event=new WheelEvent('wheel',{ctrlKey:q.action==='zoom',deltaY:q.action==='zoom'?-Math.log(.7)/.01:-30,deltaX:q.action==='pan'?70:0,deltaMode:0,clientX:600,clientY:400,bubbles:true,cancelable:true});
      const planned=performance.now(),started=performance.now();nav.dispatch(sdk.canvas,event);phase.wheels.push({planned,started,end:performance.now()});data.atNavigation={at:performance.now(),state:sdk.readState(),facts:latestPending()};
      await pause(100);
    } else {
      const admissionAt=performance.now();await sdk.selectTool('ink');pointer('down',800,500);
      data.atNextDown={at:performance.now(),admissionAt,state:sdk.readState(),facts:latestPending()};
      for(let i=1;i<=16;++i){pointer('move',800+i*6,500+Math.sin(i*.3)*8);await pause(8);if(i===8||i===16)pixels('nextdraw-move'+i);}
      pointer('up',896,492);data.nextUp={at:performance.now(),state:sdk.readState()};
    }
    data.settled=await stable();phase.settledAt=data.settled.at;phase.after=data.settled.state;nav.end(phase);pixelsActive=false;data.phase=phase;
    await snapshot('final');pixels('final');data.end=sdk.readState();data.status='OBSERVED';
  } catch(error) {data.status='FAILED';data.failure=String(error.stack??error);data.lastState=sdk.readState();}
  finally {
    pixelsActive=false;if(pixelRAF){data.pixelRAFRestored=window.requestAnimationFrame===pixelRAF;if(data.pixelRAFRestored)window.requestAnimationFrame=rawRAF;}
    data.observer=window.idleObservation.read();data.pendingObservation=window.pendingObservation.read();
    try{if(!data.phase&&phase.colorOps)nav.end(phase);nav.restore();data.navRestored=true;}catch(error){data.cleanupError=String(error);}
    try{await sdk.close();data.closed=true;}catch(error){data.closeError=String(error);}
    document.getElementById('mount').replaceChildren();
  }
  return data;
}
