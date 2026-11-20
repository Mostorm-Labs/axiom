// Verification only. No SDK calls, GL queries, forced frames or pixel reads.
export function installPreviewObserver(canvasOf,env=globalThis){
 const undo=[],fbo=new WeakMap(),writeMask=new WeakMap(),now=()=>env.performance.now();
 const rows={callbacks:[],writes:[],inputs:[],diagnostics:[],dropped:0};
 const pending=new Set();let active=false,serial=0,callback=null;
 const keep=(list,v)=>{if(list.length<20000)list.push(v);else ++rows.dropped;};
 const wrap=(object,name,make)=>{const raw=object[name];if(typeof raw!=='function')return;const fn=make(raw);object[name]=fn;undo.push(()=>{if(object[name]===fn)object[name]=raw;});};
 const gl=env.WebGL2RenderingContext.prototype;
 wrap(gl,'bindFramebuffer',raw=>function(target,fb){const r=raw.call(this,target,fb);if(target===this.FRAMEBUFFER||target===this.DRAW_FRAMEBUFFER)fbo.set(this,fb);return r;});
 wrap(gl,'colorMask',raw=>function(...args){const r=raw.apply(this,args);writeMask.set(this,args.some(Boolean));return r;});
 for(const name of ['drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced','blitFramebuffer'])wrap(gl,name,raw=>function(...args){
  const observe=active&&this.canvas===canvasOf()&&!fbo.get(this)&&writeMask.get(this)!==false&&(name!=='blitFramebuffer'||(args[8]&this.COLOR_BUFFER_BIT));
  const started=observe?now():0;const result=raw.apply(this,args);
  if(observe)keep(rows.writes,{name,started,ended:now(),callback});return result;
 });
 wrap(env,'requestAnimationFrame',raw=>function(fn){let handle;handle=raw.call(env,function(stamp){pending.delete(handle);const previous=callback,row={id:++serial,stamp,started:now()};callback=row.id;
  try{return fn.call(this,stamp);}finally{row.ended=now();if(active)keep(rows.callbacks,row);callback=previous;}
 });pending.add(handle);return handle;});
 wrap(env,'cancelAnimationFrame',raw=>function(handle){pending.delete(handle);return raw.call(env,handle);});
 for(const name of ['log','error','warn','info'])wrap(env.console,name,raw=>function(...args){
  if(active)for(const text of args)if(typeof text==='string'&&text.startsWith('AXIOM_HD_PREVIEW ')){
   try{keep(rows.diagnostics,{at:now(),...JSON.parse(text.slice(17))});}catch{++rows.dropped;}
  }return raw.apply(this,args);
 });
 return {begin(){active=true;},end(){active=false;return structuredClone(rows);},pending(){return pending.size;},
  dispatch(target,event,index){const row={index,type:event.type,started:now()};try{return target.dispatchEvent(event);}finally{row.ended=now();if(active)keep(rows.inputs,row);}},
  snapshot(){return structuredClone(rows);},restore(){for(const fn of undo.reverse())fn();}};
}
