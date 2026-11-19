import{installNavigationObserver}from'./navigation_observer.mjs';import{factor,checkpoints}from'./continuous_zoom_quality_contract.mjs';
export function captureCurrentDisplay({index,requestedCamera,actualInputAt,stamp,read}){
 if(!Number.isInteger(index)||!requestedCamera)throw Error('checkpoint identity');
 const requested=structuredClone(requestedCamera),present=stamp?structuredClone(stamp):null;
 if(!present)return {index,requestedCamera:requested,actualInputAt,status:'UNAVAILABLE',reason:'NO_SUCCESSFUL_PRESENT'};
 const result=read();if(result.metadata.status!=='VALID'||result.metadata.restoration.status!=='RESTORED')throw Error('currenttargetreadback');
 if(present.dimensions[0]!==result.metadata.canvas.width||present.dimensions[1]!==result.metadata.canvas.height)throw Error('stamp targetdimensions');
 const affine=present.affine;
 const current=Math.abs(affine[0]-requested.scale)<1e-6&&Math.abs(affine[4]-requested.x)<1e-3&&Math.abs(affine[5]-requested.y)<1e-3;
 return {index,requestedCamera:requested,actualInputAt,status:'CAPTURED',current,stamp:present,readback:result.metadata,bytes:result.bytes};
}
export async function settleConsumer(c){
 const{createRuntime}=await import('/sdk/axiom.mjs');const{sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');const{readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const row={id:c.id,DPR:c.DPR,mode:c.mode,status:'STARTED',errors:[],operations:[],frames:[],routes:[],captures:[]};let sdk,observer,activeObservation=null,activePhase=null;
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(v,m)=>{if(!v)throw Error(m);},state=()=>sdk.readState();
 const stable=async()=>{const began=performance.now();let n=0;for(;;){const s=state(),a=observer?.activity();if(!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration&&(!a||a.pending===0))++n;else n=0;if(n>=3)return;check(performance.now()-began<20000,'original20ssettle');await pause(16);}};
 const pack=bytes=>{let raw='';for(let i=0;i<bytes.length;i+=16384)raw+=String.fromCharCode(...bytes.subarray(i,i+16384));return btoa(raw);};
 try{
 const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,message:e.message}),onOperation:b=>row.operations.push({bytes:b.length,at:performance.now()}),onFrame:f=>row.frames.push({at:performance.now(),frame:f})});
 sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;observer=installNavigationObserver(()=>sdk.canvas);row.build=sdk.readBuildInfo();const image=new Uint8Array(await(await fetch('/image.png')).arrayBuffer());const catalog=sdk.resources.list();check(catalog.entries.length===1,'image manifest');await sdk.resources.publish(catalog.entries[0].token,image);await sdk.selectBrush('round-handdrawn',4);
 await sdk.zoomAt(0,0,1);let camera=state().camera;await sdk.panBy(camera.x+200,camera.y+120);await stable();row.initial=state();check(state().objects===49,'Mixed49');
 for(const route of['wheel','touch']){
  if(c.mode==='diagnostic'){window.psRows=[];window.psDropped=0;window.psActive=false;window.psTailDeadline=0;}
  if(route==='touch'){await sdk.zoomAt(0,0,1);const x=state().camera;await sdk.panBy(x.x+200,x.y+120);await stable();await pause(3000);}
  const phase={route,inputs:[],optical:[],presents:[],startState:state(),started:performance.now()},observation={};observer.begin(observation);activeObservation=observation;activePhase=phase;let previous=1;const captureBuffers=[];
  if(c.mode!=='ordinary'){
   check(window.qcSuccessfulPresent,'actual successfulpresent stamp unavailable');phase.initialPresent=structuredClone(window.qcSuccessfulPresent);
   window.qcPresentHook=stamp=>{if(stamp.role==='preview')return;if(phase.presents.length<2048)phase.presents.push(structuredClone(stamp));else throw Error('presentstamp boundedbuffer');};
  }
  if(route==='touch')await window.qcTouch({type:'touchStart',points:[{x:635.5,y:505,id:1},{x:795.5,y:505,id:2}]});phase.started=performance.now();
  for(let index=1;index<=360;++index){const planned=phase.started+index*1000/60,delay=planned-performance.now();if(delay>0)await pause(delay);const actual=performance.now(),value=factor(index),rect=sdk.canvas.getBoundingClientRect();
   if(c.mode==='diagnostic'&&(index===(c.captureOptical?85:350))){window.psActive=true;window.psTraceStarted=performance.now();}
   if(route==='wheel')observer.dispatch(sdk.canvas,new WheelEvent('wheel',{clientX:rect.left+715.5,clientY:rect.top+505,ctrlKey:true,deltaY:-Math.log(value/previous)*100,bubbles:true,cancelable:true}));
   else await window.qcTouch({type:'touchMove',points:[{x:715.5-80*value,y:505,id:1},{x:715.5+80*value,y:505,id:2}]});
   // Capture exactly the currently displayed framebuffer immediately after
   // the fixed event returns; never defer a checkpoint to a future present.
   const ended=performance.now(),requested=structuredClone(state().camera);
   if((c.mode==='optical'||c.captureOptical)&&checkpoints.includes(index)){
    const captured=captureCurrentDisplay({index,requestedCamera:requested,actualInputAt:actual,stamp:window.qcSuccessfulPresent,read:()=>readWebGLTarget(sdk.canvas,{role:'canonical'})});
    const {bytes,...proof}=captured;proof.name=route+'-'+index;proof.captureAt=performance.now();phase.optical.push(proof);if(bytes)captureBuffers.push({name:proof.name,proof,bytes});
   }
   phase.inputs.push({index,planned,actual,ended,dispatchMs:ended-actual,factor:value,camera:requested,latestPresent:c.mode==='diagnostic'?structuredClone(window.qcSuccessfulPresent):undefined});previous=value;
  }
  phase.inputEnded=performance.now();if(c.mode==='diagnostic')window.psTailDeadline=phase.inputEnded+5000;if(route==='touch')await window.qcTouch({type:'touchEnd',points:[]});await stable();phase.stableAt=performance.now();phase.endState=state();if(c.mode==='optical'){const finalPixels=readWebGLTarget(sdk.canvas,{role:'canonical'});check(finalPixels.metadata.status==='VALID','finalsameposepixels');captureBuffers.push({name:route+'-final',proof:{name:route+'-final',requestedCamera:structuredClone(state().camera),stamp:structuredClone(window.qcSuccessfulPresent),readback:finalPixels.metadata},bytes:finalPixels.bytes});}observer.end(observation);activeObservation=null;phase.observation=observation;
  if(c.mode==='diagnostic'){window.psActive=false;phase.stopTrace=window.psRows;phase.stopTraceDropped=window.psDropped;phase.sdkActivity=observer.activity();}
  if(c.mode!=='ordinary'){phase.finalPresent=structuredClone(window.qcSuccessfulPresent);window.qcPresentHook=null;}
  if(c.mode==='optical'||c.captureOptical){check(phase.optical.length===12,'twelveexpectedcheckpoints');for(const v of captureBuffers)await window.qcSaveCapture({name:v.name,proof:v.proof,base64:pack(v.bytes)});}
  row.routes.push(phase);activePhase=null;
 }
 await stable();const s=state(),saved=await sdk.captureSnapshot({generation:s.generation,documentRevision:c.fixture.metadata.documentRevision,recoveryFrontier:c.fixture.metadata.recoveryFrontier});await sendBinaryChunks('final.axsnap',saved.bytes,window.persistChunk);row.final={state:s,selection:sdk.readSelection(),snapshot:{bytes:saved.bytes.length,checkpointDigest:saved.checkpointDigest,throughCommit:saved.throughCommit}};check(!row.operations.length&&!row.errors.length,'unexpectedOperationorError');row.status='OBSERVED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{row.failureState=state();}catch{}}
 finally{window.qcPresentHook=null;if(activeObservation){try{observer.end(activeObservation);}catch(e){row.observationEndError=String(e);}if(activePhase){activePhase.observation=activeObservation;row.partialRoute=activePhase;}}if(sdk)try{await stable();await sdk.close();row.closed=true;}catch(e){row.cleanupError=String(e);}if(observer)try{observer.restore();}catch(e){row.observerCleanupError=String(e);}row.entropy=window.exactFixtureEntropy?.transcript();await window.persistMetadata(row);}
 return row;
}
