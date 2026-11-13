// Verification-only bounded passive observation. Never requests a frame or polls state.
export function boundedRing(capacity){
 if(!Number.isSafeInteger(capacity)||capacity<1)throw Error('capacity');
 const values=[];let lost=0;
 return{push(v){if(values.length<capacity)values.push(v);else ++lost;},read(){return{values:[...values],lost,capacity};}};
}
export function installIdleObserver(){
 const ring=capacity=>{const values=[];let lost=0;return{push(v){if(values.length<capacity)values.push(v);else ++lost;},read:()=>({values:[...values],lost,capacity})};};
 const frames=ring(8192),events=ring(1024),logs=ring(1024),tasks=ring(1024),workers=ring(1024),calls=ring(8192),terminals=ring(1024),nativeFrames=ring(8192);
 const repeated=new Map();
 const initial={at:performance.now(),wall:Date.now(),timeOrigin:performance.timeOrigin,visibility:document.visibilityState,focus:document.hasFocus()};
 let rafRequested=0,rafCompleted=0,inflight=0,glWrites=0,lastWrite=null,lastRAF=null,phase='setup';
 const record=(type,data={})=>events.push({type,phase,at:performance.now(),...data});
 const rawRAF=window.requestAnimationFrame;
 window.requestAnimationFrame=function(fn){++rafRequested;++inflight;return rawRAF.call(this,t=>{--inflight;const at=performance.now();try{return fn(t);}finally{++rafCompleted;lastRAF=performance.now();frames.push({phase,t,at,end:lastRAF});}});};
 const rawCancel=window.cancelAnimationFrame;window.cancelAnimationFrame=function(id){rawCancel.call(this,id);};
 for(const type of ['visibilitychange','freeze','resume'])document.addEventListener(type,()=>record(type,{visibility:document.visibilityState,focus:document.hasFocus()}),true);
 for(const type of ['focus','blur','pagehide','pageshow','error','unhandledrejection'])window.addEventListener(type,e=>record(type,{visibility:document.visibilityState,focus:document.hasFocus(),message:String(e.message??e.reason??'')}),true);
 for(const type of ['pointerdown','pointerup','pointercancel','lostpointercapture','webglcontextlost','webglcontextrestored'])document.addEventListener(type,e=>record(type,{source:e.pointerType,pointer:e.pointerId,x:e.clientX,y:e.clientY,buttons:e.buttons,eventStamp:e.timeStamp,trusted:e.isTrusted,target:e.target?.id}),true);
 const glProto=window.WebGL2RenderingContext?.prototype;
 if(glProto)for(const name of ['drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced','clear','blitFramebuffer']){
  const original=glProto[name];if(typeof original!=='function')continue;
  glProto[name]=function(...args){++glWrites;lastWrite={at:performance.now(),canvas:this.canvas.id,method:name,phase};return original.apply(this,args);};
 }
 for(const name of ['log','info','warn','error']){const original=console[name];console[name]=function(...args){
  const text=args.map(v=>typeof v==='string'?v:String(v)).join(' ');
  if(/^AXIOM_FRAME_TIMING /.test(text))nativeFrames.push({at:performance.now(),phase,text});
  else if(/present begin|present skia failed: exact cache preparation pending|AXIOM_IF_EVENT.*(?:fence_poll|preview_submit|preview_ack)|AXIOM_GPU_RETIREMENT/.test(text)){const key=text.replace(/frame=\d+/g,'frame=*').replace(/t_ns=\d+/g,'t_ns=*').replace(/revision=\d+/g,'revision=*').replace(/count=\d+/g,'count=*');const old=repeated.get(key);if(old){++old.count;old.last=performance.now();}else if(repeated.size<64)repeated.set(key,{text:key,count:1,first:performance.now(),last:performance.now()});}
  else if(/AXIOM_|Axiom Web|context.*lost|INPUT_REJECT/.test(text))logs.push({at:performance.now(),phase,type:name,text:text.slice(0,18000)});
  return original.apply(this,args);
 };}
 const OriginalWorker=window.Worker;
 window.Worker=class extends OriginalWorker{constructor(...args){super(...args);workers.push({type:'create',at:performance.now(),url:String(args[0])});this.addEventListener('message',e=>{const d=e.data;if(d?.type==='terminal'||d?.type==='ready')workers.push({type:d.type,at:performance.now(),key:d.key,status:d.status,computeMs:d.computeMs,steps:d.steps});});}postMessage(message,...rest){if(message?.type==='run')workers.push({type:'run',at:performance.now(),key:message.key,bytes:message.bytes?.byteLength});return super.postMessage(message,...rest);}};
 const performanceObserver=new PerformanceObserver(list=>{for(const e of list.getEntries())tasks.push({phase,at:e.startTime,duration:e.duration});});try{performanceObserver.observe({type:'longtask',buffered:false});}catch{}
 const mutation=new MutationObserver(list=>{for(const m of list)if(m.target instanceof HTMLCanvasElement&&m.attributeName==='style')record('canvas-style',{target:m.target.id,visibility:m.target.style.visibility,width:m.target.width,height:m.target.height});});
 mutation.observe(document,{subtree:true,attributes:true,attributeFilter:['style']});
 window.idleObservation={engine(m){const original=m._axiom_js_call;m._axiom_js_call=function(ptr,...args){const input=JSON.parse(m.UTF8ToString(ptr)),start=performance.now();let answer;try{const result=original.call(this,ptr,...args);answer=JSON.parse(m.UTF8ToString(result));return result;}finally{const v={at:start,end:performance.now(),phase,command:input.command,request:['pointer','control','pendingReceipt'].includes(input.command)?input:undefined,result:answer};if(input.command==='frame')calls.push(v);else if(input.command==='pointer'||input.command==='control'||answer?.ok===false)terminals.push(v);}};},phase(value){phase=value;record('phase',{value});},summary(){return{at:performance.now(),wall:Date.now(),phase,visibility:document.visibilityState,focus:document.hasFocus(),rafRequested,rafCompleted,inflight,lastRAF,glWrites,lastWrite};},read(){return{initial,summary:this.summary(),frames:frames.read(),events:events.read(),logs:logs.read(),tasks:tasks.read(),workers:workers.read(),calls:calls.read(),terminals:terminals.read(),nativeFrames:nativeFrames.read(),repeated:[...repeated.values()],policy:'passive; no observer RAF/timer/state polling; all page timestamps performance.now; 8192 frames/1024 each other ring'};}};
}
export function summarize(values){const v=values.filter(Number.isFinite).sort((a,b)=>a-b);return{count:v.length,median:v.length?v[Math.floor((v.length-1)*.5)]:null,p95:v.length?v[Math.ceil(v.length*.95)-1]:null,max:v.at(-1)??null};}
export function qualifyIdle(before,after,events,duration){
 const reasons=[];if(after.at-before.at<duration)reasons.push('duration');
 if(before.visibility!=='visible'||after.visibility!=='visible')reasons.push('visibility');
 if(before.rafRequested!==after.rafRequested||before.rafCompleted!==after.rafCompleted)reasons.push('runtime_not_idle');
 if(before.glWrites!==after.glWrites)reasons.push('GPU_writes_during_idle');
 for(const e of events)if(e.at>before.at&&e.at<after.at&&['blur','pagehide','freeze','webglcontextlost','webglcontextrestored'].includes(e.type))reasons.push(e.type);
 for(const e of events)if(e.at>before.at&&e.at<after.at&&e.type==='visibilitychange'&&e.visibility!=='visible')reasons.push('hidden');
 return{qualified:!reasons.length,reasons,elapsed_ms:after.at-before.at,focus_before:before.focus,focus_after:after.focus,physical_frontmost:'not continuously observed; browser-visible plus event focus only'};
}
