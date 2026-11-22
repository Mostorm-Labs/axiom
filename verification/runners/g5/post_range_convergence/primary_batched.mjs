// Reuses original CPUW7 confirmedinput/history/capture sequence, only passive observation/persistence adapted.
export async function primaryConsumer(c){
 const {createRuntime}=await import('/sdk/axiom.mjs');
 const {sendBinaryChunks}=await import('/g5/ur2d_selection_up_contract.mjs');
 const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
 const row={id:c.id,status:'STARTED',events:[],operations:[],frames:[],errors:[],stages:{},snapshots:{}};
 const pause=ms=>new Promise(r=>setTimeout(r,ms)),check=(x,m)=>{if(!x)throw Error(m);};
 let sdk,sequence=90000000000;const operations=[];const h=window.heavyObservation;
 const state=()=>sdk.readState();
 const privateFacts=()=>h.state().latestFacts;
 const synchronize=()=>({js:performance.now(),basis:'Nativeattemptstart_ns/bracketedactualbridgeframecalls; no extra privateclockpoll'});
 const observe=label=>{const s=state();row.stages[label]={at:performance.now(),state:s,privateFacts:privateFacts()};h.mark(label,{state:s,facts:privateFacts()});return s;};
 const save=()=>window.persistMetadata(row);
 const pointer=(phase,x,y)=>{sequence+=8000000;const b=sdk.canvas.getBoundingClientRect();
  const event=new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'pen',clientX:b.left+x,clientY:b.top+y,
   pressure:.5,buttons:['up','cancel'].includes(phase)?0:1,bubbles:true,cancelable:true});
  Object.defineProperty(event,'timeStamp',{value:sequence/1e6});const at=performance.now();sdk.canvas.dispatchEvent(event);
  row.events.push({phase,x,y,at,end:performance.now()});
 };
 const stable=async()=>{const at=performance.now();let quiet=0;
  for(;;){const s=state();if(!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration){if(++quiet===3)return performance.now();}else quiet=0;
   check(performance.now()-at<160000,'pending drain limit');await pause(16);}
 };
 const capture=async label=>{h.phase('capture-'+label);const captureStart=performance.now();const s=state();const saved=await sdk.captureSnapshot({generation:s.generation,
  documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier});
  const snapshotEncodedAt=performance.now();await sendBinaryChunks(label+'.axsnap',saved.bytes,window.persistChunk);
  const read=readWebGLTarget(sdk.canvas,{role:'canonical'});check(read.metadata.status==='VALID','readback '+label);
  await sendBinaryChunks(label+'.rgba',read.bytes,window.persistChunk);
  row.snapshots[label]={captureStart,snapshotEncodedAt,captureEnd:performance.now(),state:s,readback:read.metadata,through:operations.length,metadata:{generation:s.generation,
   documentRevision:(BigInt(c.fixture.metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:c.fixture.metadata.recoveryFrontier,checkpointDigest:saved.checkpointDigest,resourceBinding:'1'}};
  await save();
 };
 const wheel=async()=>{const b=sdk.canvas.getBoundingClientRect();for(const [ctrl,dx,dy]of [[false,24,12],[true,0,-9]]){
  const at=performance.now();sdk.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:b.left+500,clientY:b.top+300,ctrlKey:ctrl,deltaX:dx,deltaY:dy,bubbles:true,cancelable:true}));
  row.events.push({kind:'wheel',ctrl,dx,dy,at,end:performance.now()});await pause(16);}
 };
 try{
  const bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:c.fixture.documentId,
   restore:{bytes,metadata:c.fixture.metadata},builtinFonts:false,permission:'localSingleUser',
   onError:e=>row.errors.push({code:e.code,text:String(e)}),
   onOperation:b=>{operations.push(b);const s=state();row.operations.push({at:performance.now(),bytes:b.length,generation:s.generation,commit:s.lastCommit});},
   onFrame:f=>{const s=state();row.frames.push({at:performance.now(),frame:f,presentedGeneration:s.lastCommit.presentedGeneration});}});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  await stable();check(state().objects===40,'Fixed40');row.build=sdk.readBuildInfo();row.clockStart=synchronize();const initial=observe('initial');
  await sdk.selectTool('partialEraser');await sdk.setEraser({mode:2,diameter:32});
  h.phase('A-input');const down=performance.now();pointer('down',420,160);
  for(let i=1;i<=7200;++i){pointer('move',420+7*Math.sin(i*.17),160+170*i/7200);
   // Focused post-Up case: all confirmed points arrive before an RAF.
   // Coordinates/timestamps and subsequent operations are unchanged.
  }
  row.inputCadence='7200 confirmed moves synchronously before next RAF; separate from original interleaved-input case';row.A_down_to_up_ms=performance.now()-down;h.phase('AB-post-Up');const up=performance.now();pointer('up',999,999);observe('A-Up');
  pointer('down',300,230);observe('B-Down');for(let i=1;i<=8;++i)pointer('move',300,230+i*4);pointer('up',999,999);observe('B-Up');
  if(c.navigation)await wheel();
  const settled=await stable();observe('AB-settled');row.up_to_drain_ms=settled-up;
  row.first_A_commit_ms=row.operations.find(v=>v.at>=up&&BigInt(v.generation)>BigInt(initial.generation))?.at-up;
  row.first_A_present_ms=row.frames.find(v=>v.at>=up&&BigInt(v.presentedGeneration)>BigInt(initial.generation))?.at-up;
  row.first_AB_present_ms=row.frames.find(v=>v.at>=up&&BigInt(v.presentedGeneration)>=BigInt(initial.generation)+2n)?.at-up;
  row.observation_policy='onFrame/onOperation capture one readState; primary interval includes matching observer overhead in both variants';
  row.workerObservation='Sparseactualrun/terminalrecords streamed; no jobpayloadcopies';await save();
  check(row.errors.length===0,'root SDK errors');
  await capture('AB');
  // Reference/serialization occurs after the primary timing interval.
  h.phase('Undo');observe('before-undo');await sdk.undo();await stable();observe('undo-settled');await capture('undo');h.phase('Redo');observe('before-redo');await sdk.redo();await stable();observe('redo-settled');await capture('redo');
  if(c.followup){
   h.phase('C-followup');observe('before-C');const at=performance.now();const camera=state().camera;
   pointer('down',350*camera.scale+camera.x,210*camera.scale+camera.y);
   pointer('move',360*camera.scale+camera.x,230*camera.scale+camera.y);pointer('up',999,999);observe('C-Up');
   await stable();row.followup_erase_ms=performance.now()-at;observe('followup-erase');
   h.phase('masked-selection');await sdk.selectTool('select');await sdk.setSelectionMode('rectangle');
   const beforeSelection=state();const selectionAt=performance.now();pointer('down',80*camera.scale+camera.x,90*camera.scale+camera.y);
   pointer('move',620*camera.scale+camera.x,480*camera.scale+camera.y);pointer('up',620*camera.scale+camera.x,480*camera.scale+camera.y);
   const wait=performance.now();for(;;){const s=state();if(!s.activeContacts&&BigInt(s.selectionRevision)>BigInt(beforeSelection.selectionRevision)&&
      JSON.stringify([...s.selected].sort())===JSON.stringify([...c.fixture.objectIds].sort()))break;
    check(performance.now()-wait<30000,'masked selection incomplete');await pause(16);}
   row.masked_selection_ms=performance.now()-selectionAt;row.masked_selection=sdk.readSelection();
   row.selection_completion='public selectionRevision advanced; complete original40 targetID set; no partial set accepted';
   await sdk.setSelectionMode('point');pointer('down',1300,950);pointer('up',1300,950);await stable();
   await capture('followup');
  }
  row.workerObservation='Sparseactualrun/terminalrecords streamed; no jobpayloadcopies';
  row.clockEnd=synchronize();row.observation=await h.flush();row.entropy=window.exactFixtureEntropy.transcript();row.status='OBSERVED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{observe('failure');}catch{}}
 finally{
  if(!row.observation)row.observation=await h.flush();
  for(let i=0;i<operations.length;++i)await sendBinaryChunks('operation-'+i+'.axsnap',operations[i],window.persistChunk);
  if(sdk)try{if(state().activeContacts)pointer('cancel',0,0);await sdk.close();row.closed=true;}catch(e){row.cleanupError=String(e);}
  row.observation=await h.finish();
  await save();
 }
 return row;
}