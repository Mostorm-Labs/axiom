import {deliver} from './schedule.mjs';
import {realtimePlan,legacyCorpus} from './fixture_plan.mjs';
import {SnapshotTransfer} from './stream_transfer.mjs';
import {correctnessDriver} from './correctness_driver.mjs';
// This verification host taps the first noncapture input listener actually
// installed by the root SDK. It invokes the original exactly once and does not
// alter its return, queue, coordinates or admission decisions.
const nowNs=()=>BigInt(Math.floor(performance.now()*1e6+.5));
const sleep=ms=>new Promise(r=>setTimeout(r,ms));
const nativeRaf=requestAnimationFrame.bind(window),nativeCancel=cancelAnimationFrame.bind(window),pendingSdk=new Set();let lastSdkActivity=performance.now();
window.requestAnimationFrame=fn=>{if(fn.ur0Probe)return nativeRaf(fn);let handle;lastSdkActivity=performance.now();handle=nativeRaf(at=>{pendingSdk.delete(handle);try{return fn(at);}finally{lastSdkActivity=performance.now();}});pendingSdk.add(handle);return handle;};
window.cancelAnimationFrame=handle=>{pendingSdk.delete(handle);return nativeCancel(handle);};
const raf=()=>new Promise(r=>{r.ur0Probe=true;requestAnimationFrame(r);});
const errors=[],frames=[],handlers=[],planned=[],rafTimes=[],captures=[];
const snapshotTransfer=new SnapshotTransfer();
let sdk,correctness,current=null,measuring=false,contentNs=1000000000n,sequence=0;
const tapCounts=new WeakMap(),rawAdd=EventTarget.prototype.addEventListener,rawRemove=EventTarget.prototype.removeEventListener,wrapped=new WeakMap();
EventTarget.prototype.addEventListener=function(type,fn,opts){
 const capture=typeof opts==='boolean'?opts:!!opts?.capture;
 if(this instanceof HTMLCanvasElement&&['pointerdown','pointermove','pointerup','pointercancel','wheel'].includes(type)&&!capture){
  const counts=tapCounts.get(this)??{};tapCounts.set(this,counts);counts[type]=(counts[type]??0)+1;
  if(counts[type]===1){const call=typeof fn==='function'?fn:e=>fn.handleEvent(e);const wrapper=function(e){
   if(!measuring||this!==sdk?.canvas)return call.call(this,e);
   const row={event_id:current?.event_id??null,type,enter_ns:String(nowNs()),exit_ns:null,threw:null};handlers.push(row);
   try{return call.call(this,e);}catch(error){row.threw=String(error);throw error;}finally{row.exit_ns=String(nowNs());}
  };wrapped.set(fn,wrapper);return rawAdd.call(this,type,wrapper,opts);}
 }
 return rawAdd.call(this,type,fn,opts);
};
EventTarget.prototype.removeEventListener=function(type,fn,opts){return rawRemove.call(this,type,wrapped.get(fn)??fn,opts);};
const diag=()=>globalThis.__ur0diag;
function pointer(phase,x,y,time=undefined,coalesced=[]){
 contentNs=time===undefined?contentNs+4000000n:BigInt(time);
 const rect=sdk.canvas.getBoundingClientRect();
 const make=(p,x,y,t)=>{const e=new PointerEvent('pointer'+p,{pointerId:1,pointerType:'mouse',clientX:rect.left+x,clientY:rect.top+y,button:p==='move'?-1:0,buttons:p==='up'||p==='cancel'?0:1,pressure:p==='up'?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:Number(t)/1e6});return e;};
 const e=make(phase,x,y,contentNs);
 if(coalesced.length)Object.defineProperty(e,'getCoalescedEvents',{value:()=>coalesced.map(s=>make(s[4],s[0]/1000,s[1]/1000,BigInt(s[2])))});
 sdk.canvas.dispatchEvent(e);
}
function dispatch(e){
 current=e;const id=++sequence;e.diagnostic_event=String(id);if(diag())diag().control(5,id);
 try{
  if(e.kind==='pointer'){
   if(e.samples){const s=e.samples.at(-1);pointer(s[4],s[0]/1000,s[1]/1000,s[2],e.samples.length>1?e.samples:[]);}
   else pointer(e.phase,e.logical_x??e.x_milli/1000,e.logical_y??e.y_milli/1000,e.content_timestamp_ns);
  }else if(e.kind==='wheel'){
   const r=sdk.canvas.getBoundingClientRect();sdk.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:r.left+e.x_milli/1000,clientY:r.top+e.y_milli/1000,ctrlKey:e.ctrl_key,deltaMode:e.delta_mode,deltaX:e.delta_x,deltaY:e.delta_y,bubbles:true,cancelable:true}));
  }else if(e.kind==='control'&&e.public_method==='selectTool')return sdk.selectTool(e.argument);
  else throw Error('unrecognized frozen input kind');
 }finally{if(diag())diag().control(6,0);current=null;}
}
async function until(fn,label,deadline){while(!fn()){if(performance.now()>deadline)throw Error(label);await sleep(10);}}
async function settled(deadline){
 await until(()=>sdk.readState().activeContacts===0&&sdk.readState().lastCommit.presentedGeneration===sdk.readState().generation,'canonical setup completion',deadline);
 for(let i=0;i<10;++i){if(performance.now()>deadline)throw Error('setup deadline');await raf();}
}
async function checkpoint(label){const state=sdk.readState(),selection=sdk.readSelection();
 const snapshot=await sdk.captureSnapshot({generation:state.generation,documentRevision:state.generation,recoveryFrontier:[8,1,16,1,24,1]});
 const {bytes,...metadata}=snapshot;snapshotTransfer.begin(bytes);
 const source_sha256=Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes))).map(v=>v.toString(16).padStart(2,'0')).join('');
 const row={label,state,selection,snapshot:{...metadata,byte_length:bytes.length,source_sha256}};captures.push(row);return row;
}
async function init(){
 const {createRuntime}=await import('/axiom.mjs');
 sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'99999999999999999999999999999999',permission:'localSingleUser',builtinFonts:false,onError:error=>errors.push({at_ns:String(nowNs()),event_id:current?.event_id??null,error:String(error)}),onFrame:f=>frames.push({at_ns:String(nowNs()),...f})});
 sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
 if(diag())diag().control(2,1);
 const gl=sdk.canvas.getContext('webgl2'),extension=gl.getExtension('WEBGL_debug_renderer_info');
 let resolution=Infinity,previous=performance.now();for(let i=0;i<10000;++i){const t=performance.now();if(t>previous)resolution=Math.min(resolution,t-previous);previous=t;}
 return{build:sdk.readBuildInfo(),state:sdk.readState(),private_channel:!!diag(),environment:{userAgent:navigator.userAgent,timeOrigin:performance.timeOrigin,reported_resolution_ns:Number.isFinite(resolution)?String(Math.round(resolution*1e6)):null,renderer:gl.getParameter(extension?extension.UNMASKED_RENDERER_WEBGL:gl.RENDERER),vendor:gl.getParameter(extension?extension.UNMASKED_VENDOR_WEBGL:gl.VENDOR),dpr:devicePixelRatio,mount:[800,500],window:[innerWidth,innerHeight]}};
}
async function setup({fixture,copies=15,scale=2,deadlineMs=45000,legacy=false}){
 const deadline=performance.now()+deadlineMs;
 await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('ink');
 const divisor=legacy?1:1000;
 for(let i=0;i<fixture.samples.length;++i){const s=fixture.samples[i];pointer(s[4],s[0]/divisor,s[1]/divisor,s[2]);if(i%16===0)await raf();if(performance.now()>deadline)throw Error('setup stroke deadline');}
 await settled(deadline);await sdk.selectTool('select');
 const first=legacy?[340+215*Math.sin(.0605)+25*Math.sin(.0605*.17),280+140*Math.cos(.0605*.71)+20*Math.sin(.0605*.13)]:fixture.samples[0].slice(0,2).map(n=>n/1000);pointer('down',...first);pointer('up',...first);await settled(deadline);
 if(sdk.readSelection().count!==1)throw Error('fixture original head did not select one stroke');
 for(let i=0;i<copies;++i){await sdk.duplicateSelection({offset:[1,1]});await settled(deadline);console.info('UR0_STAGE '+JSON.stringify({stage:'duplicate',copy:i+1}));}
 if(scale!==1){await sdk.zoomAt(400,250,scale);await settled(deadline);}
 if(legacy)await sleep(copies===8?300:1200);else{await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');}
 return{state:sdk.readState(),selection:sdk.readSelection(),config:sdk.readBrushConfig(),content_end_ns:String(contentNs),legacy_drag_start:first.map(n=>n+copies)};
}
function clocks(){return Array.from({length:16},()=>{const before_ns=String(nowNs()),native_ns=diag().clock().clock_ns,after_ns=String(nowNs());return{before_ns,native_ns,after_ns};});}
function prepareMeasurement(events){
 const contentOrigin=contentNs+4000000n;
 return{origin_ns:String(nowNs()+500000000n),prepared_at_ns:String(nowNs()),events:events.map(e=>({...e,content_timestamp_ns:String(contentOrigin+BigInt(e.planned_offset_ns))}))};
}
async function measure({events,on=true,deadlineMs=120000,prepared=null}){
 // Event content timestamps derive from the immutable original plan, never
 // current wall time or the return time of a slow previous handler.
 const plan=prepared??prepareMeasurement(events),fixed=plan.events;
 const pre=diag()?clocks():null;if(diag())diag().control(on?1:0,0);
 const refresh=[];for(let i=0;i<17;++i)refresh.push(await raf());
 const originNs=BigInt(plan.origin_ns),deadline=Number(originNs)/1e6+deadlineMs;
 let running=true;const tick=at=>{if(!running)return;rafTimes.push({at_ns:String(BigInt(Math.floor(at*1e6+.5))),observed_ns:String(nowNs())});requestAnimationFrame(tick);};tick.ur0Probe=true;requestAnimationFrame(tick);
 measuring=true;const before=sdk.readState(),beforeFrame=frames.length;
 planned.push(...await deliver(fixed,{originNs,nowNs,sleep,dispatch}));
 const inputEnd=String(nowNs());let quiescence=null;
 // This post-input wait never gates the preplanned events. It is bounded by
 // the row's measurement deadline and only reports the public completion state.
 try{await until(()=>sdk.readState().activeContacts===0&&sdk.readState().lastCommit.presentedGeneration===sdk.readState().generation&&pendingSdk.size===0&&performance.now()-lastSdkActivity>=200,'measurement public completion and SDK RAF quiescence',deadline);quiescence=String(nowNs());}catch(error){errors.push({at_ns:String(nowNs()),error:String(error),event_id:null});}
 const end=String(nowNs());measuring=false;running=false;
 if(diag())diag().control(0,0);const post=diag()?clocks():null;
 return{origin_ns:String(originNs),input_end_ns:inputEnd,end_ns:end,quiescence_ns:quiescence,quiescence_scope:'public state plus SDK RAF queue; GPU convergence requires joined callback records',pending_sdk_raf:pendingSdk.size,before,after:sdk.readState(),selection:sdk.readSelection(),planned,handlers,errors,frames:frames.slice(beforeFrame),rafTimes,clock_before:pre,clock_after:post,refresh_intervals_ms:refresh.slice(1).map((v,i)=>v-refresh[i])};
}
async function measureLegacy({kind,on=true,start}){
 const pre=diag()?clocks():null;if(diag())diag().control(on?1:0,0);
 const originNs=nowNs(),before=sdk.readState(),phases=[],rafStart=rafTimes.length;let lastInput=originNs,quiescence=null;
 measuring=true;let running=true;const tick=at=>{if(!running)return;rafTimes.push({at_ns:String(BigInt(Math.floor(at*1e6+.5))),observed_ns:String(nowNs())});requestAnimationFrame(tick);};tick.ur0Probe=true;requestAnimationFrame(tick);
 const send=(e)=>{const t=nowNs();e.planned_offset_ns=String(t-originNs);e.planned_ns=String(t);e.producer_ns=String(t);e.schedule='LEGACY_CLOSED_LOOP';dispatch(e);e.dispatch_end_ns=String(nowNs());e.status='COMPLETE';e.status_scope='DISPATCH_ONLY';planned.push(e);lastInput=nowNs();};
 const point=(id,phase,x,y,n)=>send({event_id:id+'-'+n,interaction_id:id,kind:'pointer',phase,logical_x:x,logical_y:y});
 const quiet=async(prior,at)=>{const deadline=performance.now()+30000;for(;;){const state=sdk.readState();if(frames.slice(prior).some(f=>BigInt(f.at_ns)>=at)&&state.lastCommit.presentedGeneration===state.generation&&pendingSdk.size===0&&performance.now()-lastSdkActivity>=200){quiescence=String(nowNs());return;}if(performance.now()>deadline)throw Error('legacy post-input quiescence deadline');await sleep(25);}};
 const nav=async id=>{const prior=frames.length,started=String(nowNs());
  for(let i=0;i<72;++i){const pan=i>=36;for(let b=0;b<4;++b)send({event_id:id+'-'+i+'-'+b,interaction_id:id,kind:'wheel',x_milli:400000,y_milli:250000,ctrl_key:!pan,delta_mode:0,delta_x:pan?(i<54?-16:16):0,delta_y:pan?(i<54?-8:8):(i<18?2:-2)});await raf();}
  await quiet(prior,lastInput);phases.push({id,started_ns:started,ended_ns:String(nowNs()),state:sdk.readState()});
 };
 const samples=[];
 if(kind==='legacy9_drag'){
  const prior=frames.length;point('L-D','down',...start,'down');
  for(let i=0;i<48;++i){const at=nowNs();point('L-D','move',start[0]+48*i/47,start[1]+20*i/47,i);await raf();samples.push(String(nowNs()-at));sdk.readState();}
  point('L-D','up',start[0]+48,start[1]+20,'up');await quiet(prior,lastInput);
 }else if(kind==='legacy16_erase'){
  await nav('L-before');
  for(let n=0;n<10;++n){await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');const prior=frames.length,started=String(nowNs()),id='L-E'+n;
   for(let i=0;i<128;++i){point(id,i===0?'down':'move',390+160*Math.sin(i*.12+n*.9),290+100*Math.cos(i*.17+n*.3),i);await raf();}
   point(id,'up',470+n*30,230+n*85,'up');await quiet(prior,lastInput);phases.push({id,started_ns:started,ended_ns:String(nowNs()),state:sdk.readState()});
  }
  await nav('L-after');await nav('L-hot');
 }else throw Error('unknown legacy fixture');
 measuring=false;running=false;if(diag())diag().control(0,0);const post=diag()?clocks():null,end=String(nowNs());
 const sorted=[...samples].sort((a,b)=>BigInt(a)<BigInt(b)?-1:BigInt(a)>BigInt(b)?1:0);
 return{origin_ns:String(originNs),input_end_ns:String(lastInput),end_ns:end,quiescence_ns:quiescence,before,after:sdk.readState(),selection:sdk.readSelection(),planned,handlers,errors,frames,rafTimes:rafTimes.slice(rafStart),clock_before:pre,clock_after:post,phases,measurement_schedule:'LEGACY_CLOSED_LOOP',content_span_ns:'28796000000',legacy_floor_p95_frame_ns:sorted.length?sorted[Math.floor(sorted.length*.95)]:null,legacy_original_archives_unchanged:true};
}
window.ur0={init,setup,setupLegacy:kind=>setup({fixture:legacyCorpus(),legacy:true,copies:kind==='legacy9_drag'?8:15,scale:kind==='legacy9_drag'?1:2}),measureLegacy,checkpoint,snapshotChunk:(offset,count)=>snapshotTransfer.pull(offset,count),snapshotRelease:cancel=>snapshotTransfer.end(cancel),measure,prepareMeasurement,clocks,
 async correctnessSetup(kind,on){correctness=correctnessDriver({sdk,pointer,raf,settled,diag});return correctness.setup(kind,on);},
 correctnessNext:()=>correctness.next(),pixelChunk:(offset,count)=>correctness.pull(offset,count),pixelRelease:cancel=>correctness.release(cancel),
 async realtime({fixture,hz,on=true}){
 await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('ink');
 return measure({prepared:prepareMeasurement(realtimePlan(fixture,hz)),on,deadlineMs:35000});
},drain:()=>diag()?.drain()??null,state:()=>({state:sdk.readState(),selection:sdk.readSelection(),errors,frames}),async close(){await sdk.close();return{closed:!document.querySelector('#mount canvas'),errors};}};
