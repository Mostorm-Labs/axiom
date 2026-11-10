// Verification only: a context belongs to a canvas, but its current READ FBO
// need not be that canvas's default target. Never draw or alter DRAW state here.
const identities=new WeakMap();let nextIdentity=0,nextRead=0,nextFailure=0;
const objectId=(o,kind)=>{if(o===null)return kind==='framebuffer'?'DEFAULT':'NONE';if(!identities.has(o))identities.set(o,kind+'-'+(++nextIdentity));return identities.get(o);};
const now=()=>globalThis.performance?.now()??Date.now();
const packNames=['PACK_ALIGNMENT','PACK_ROW_LENGTH','PACK_SKIP_ROWS','PACK_SKIP_PIXELS'];
function errors(gl){const codes=[];for(let i=0;i<32;++i){const v=gl.getError();if(v===gl.NO_ERROR)return{codes,truncated:false};codes.push(v);}return{codes,truncated:true};}
function state(gl){return{readFramebuffer:gl.getParameter(gl.READ_FRAMEBUFFER_BINDING),drawFramebuffer:gl.getParameter(gl.DRAW_FRAMEBUFFER_BINDING),readBuffer:gl.getParameter(gl.READ_BUFFER),pixelPackBuffer:gl.getParameter(gl.PIXEL_PACK_BUFFER_BINDING),pack:packNames.map(n=>gl.getParameter(gl[n]))};}
function describe(s){return{readFramebuffer:objectId(s.readFramebuffer,'framebuffer'),drawFramebuffer:objectId(s.drawFramebuffer,'framebuffer'),readBuffer:s.readBuffer,pixelPackBuffer:objectId(s.pixelPackBuffer,'buffer'),pack:Object.fromEntries(packNames.map((n,i)=>[n,s.pack[i]]))};}
function same(a,b){return a.readFramebuffer===b.readFramebuffer&&a.drawFramebuffer===b.drawFramebuffer&&a.readBuffer===b.readBuffer&&a.pixelPackBuffer===b.pixelPackBuffer&&a.pack.every((v,i)=>v===b.pack[i]);}
export function readWebGLTarget(canvas,{role,rect}={}){
 const metadata={schema:'g5-qualified-default-readback-v1',id:++nextRead,role,status:'INVALID',started:now(),selected:null,errors:{},restoration:{status:'NOT_CHANGED'},error_queue:'Preexisting/read/restore errors are consumed and recorded; the GL error queue is not restorable.'};
 let gl,saved,defaultMode,changed=false,raw=null,bytes=null,width=0,height=0;
 try{
  if(!canvas)throw Error('target canvas unavailable');
  metadata.canvas={id:canvas.id??'',identity:objectId(canvas,'canvas'),width:canvas.width,height:canvas.height};gl=canvas.getContext('webgl2');
  if(!gl||gl.canvas!==canvas){metadata.status='UNAVAILABLE';throw Error('matching WebGL2 context unavailable');}
  metadata.context={identity:objectId(gl,'context'),attributes:gl.getContextAttributes(),drawingBufferWidth:gl.drawingBufferWidth,drawingBufferHeight:gl.drawingBufferHeight,lost:gl.isContextLost()};
  metadata.errors.preexisting=errors(gl);
  if(metadata.context.lost){metadata.status='UNAVAILABLE';throw Error('context lost');}
  saved=state(gl);metadata.before=describe(saved);
  if(metadata.errors.preexisting.codes.length||metadata.errors.preexisting.truncated)throw Error('unexpected preexisting GL error');
  const w=gl.drawingBufferWidth,h=gl.drawingBufferHeight;
  if(!Number.isSafeInteger(w)||!Number.isSafeInteger(h)||w<=0||h<=0||canvas.width!==w||canvas.height!==h)throw Error('canvas/drawing buffer dimensions mismatch');
  const r=rect??{x:0,y:0,width:w,height:h};
  if(!['x','y','width','height'].every(k=>Number.isSafeInteger(r[k]))||r.x<0||r.y<0||r.width<=0||r.height<=0||r.x+r.width>w||r.y+r.height>h||!Number.isSafeInteger(r.width*r.height*4))throw Error('invalid readback ROI');
  width=r.width;height=r.height;metadata.rectangle={...r,origin:'top-left',readY:h-r.y-r.height};
  gl.bindFramebuffer(gl.READ_FRAMEBUFFER,null);changed=true;defaultMode=gl.getParameter(gl.READ_BUFFER);metadata.defaultReadBufferBefore=defaultMode;
  gl.readBuffer(gl.BACK);gl.bindBuffer(gl.PIXEL_PACK_BUFFER,null);
  for(const n of packNames)gl.pixelStorei(gl[n],n==='PACK_ALIGNMENT'?1:0);
  metadata.selected=describe(state(gl));metadata.errors.setup=errors(gl);
  if(metadata.selected.readFramebuffer!=='DEFAULT'||metadata.selected.readBuffer!==gl.BACK||metadata.selected.pixelPackBuffer!=='NONE'||metadata.selected.drawFramebuffer!==metadata.before.drawFramebuffer||packNames.some(n=>metadata.selected.pack[n]!== (n==='PACK_ALIGNMENT'?1:0)))throw Error('selected default target/state mismatch');
  if(metadata.errors.setup.codes.length||metadata.errors.setup.truncated)throw Error('read target selection GL error');
  raw=new Uint8Array(width*height*4);gl.readPixels(r.x,h-r.y-r.height,width,height,gl.RGBA,gl.UNSIGNED_BYTE,raw);metadata.errors.read=errors(gl);
  if(metadata.errors.read.codes.length||metadata.errors.read.truncated||gl.isContextLost())throw Error('readPixels GL error/context loss');
  bytes=new Uint8Array(raw.length);for(let y=0;y<height;++y)bytes.set(raw.subarray((height-1-y)*width*4,(height-y)*width*4),y*width*4);
  metadata.status='VALID';
 }catch(error){metadata.reason=String(error.message??error);if(gl?.isContextLost())metadata.status='UNAVAILABLE';}
 finally{
  if(saved&&changed){
   const failures=[];const restore=fn=>{try{fn();}catch(e){failures.push(String(e.message??e));}};
   restore(()=>gl.bindFramebuffer(gl.READ_FRAMEBUFFER,null));
   if(defaultMode!==undefined)restore(()=>gl.readBuffer(defaultMode));
   restore(()=>{metadata.defaultReadBufferAfter=gl.getParameter(gl.READ_BUFFER);if(defaultMode!==undefined&&metadata.defaultReadBufferAfter!==defaultMode)failures.push('default per-FBO readBuffer mismatch');});
   restore(()=>gl.bindFramebuffer(gl.READ_FRAMEBUFFER,saved.readFramebuffer));
   if(saved.readFramebuffer!==null)restore(()=>gl.readBuffer(saved.readBuffer));
   restore(()=>gl.bindBuffer(gl.PIXEL_PACK_BUFFER,saved.pixelPackBuffer));
   for(let i=0;i<packNames.length;++i)restore(()=>gl.pixelStorei(gl[packNames[i]],saved.pack[i]));
   restore(()=>{const after=state(gl);metadata.after=describe(after);if(!same(saved,after))failures.push('READ/DRAW/PBO/PACK state mismatch');});
   restore(()=>{metadata.errors.restore=errors(gl);if(metadata.errors.restore.codes.length||metadata.errors.restore.truncated)failures.push('restoration GL error');});
   metadata.restoration={status:failures.length?'FAILED':'RESTORED',failures};
   if(failures.length){metadata.status=gl.isContextLost()?'UNAVAILABLE':'INVALID';metadata.reason='GL state restoration failed';}
  }
  if(saved&&!changed)try{metadata.after=describe(state(gl));}catch(e){metadata.restoration={status:'FAILED',failures:[String(e)]};metadata.status='INVALID';}
  metadata.finished=now();
 }
 if(metadata.status!=='VALID')bytes=null;
 return{bytes,width,height,target:canvas?.id??'',metadata};
}
// Failure witnesses own copied readbacks; no async persistence or later frame is
// allowed to replace one. Host streaming can start only after this returns.
const captures=[],buffers=new Map();
export function captureFailureNow({targets,state:readState,reason,trigger}={}){
 const id='failure-'+(++nextFailure),d={schema:'g5-immediate-target-failure-v1',id,reason,started:now(),synchronous:true,state:null,buffers:[]};
 try{const v=readState?.();if(v?.then)throw Error('asynchronous failure state');d.state=structuredClone(v??null);}catch(e){d.state={status:'MISSING',reason:String(e)};}
 const keep=(role,sample)=>{
  const key=id+'-'+role;const meta={role,key,length:sample?.bytes?.length??0,status:sample?.bytes&&sample.metadata?.status==='VALID'?'AVAILABLE_CAPTURED':'MISSING',readback:sample?.metadata??null,reason:sample?.metadata?.reason??null};
  if(meta.status==='AVAILABLE_CAPTURED')buffers.set(key,sample.bytes);else meta.reason??='qualified capture unavailable';d.buffers.push(meta);
 };
 if(trigger)keep('trigger-ROI',trigger);
 for(const role of['canonical','preview']){
  try{keep(role,readWebGLTarget(typeof targets?.[role]==='function'?targets[role]():targets?.[role],{role}));}
  catch(e){keep(role,{metadata:{status:'INVALID',reason:String(e)},bytes:null});}
 }
 d.finished=now();captures.push(d);return structuredClone(d);
}
export function failureDescriptors(){return structuredClone(captures);}
export function failureChunk(key,off,count){const b=buffers.get(key);if(!b)throw Error('captured buffer MISSING/released; no later-frame fallback');if(!Number.isSafeInteger(off)||!Number.isSafeInteger(count)||off<0||count<1||count>262144||off>=b.length)throw Error('failure chunk bounds');const part=b.subarray(off,Math.min(off+count,b.length));let s='';for(let i=0;i<part.length;i+=8192)s+=String.fromCharCode(...part.subarray(i,i+8192));return btoa(s);}
export function releaseFailureBuffer(key){return buffers.delete(key);}
export function resetFailureStoreForTests(){captures.length=0;buffers.clear();}
