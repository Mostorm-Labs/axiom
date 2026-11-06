// Verification-only observer. Records API activity,never a camera-qualified or
// physical present. No Runtime/SDK calls or sampler-owned timers/RAFs occur here.
export function installNavigationObserver(canvasOf,env=globalThis){
  const now=()=>env.performance.now(),proto=env.WebGL2RenderingContext.prototype;
  const undo=[],bindings=new WeakMap(),writes=new WeakMap();
  let active=null,nextEvent=0,nextRaf=0,inputId=null,rafId=null;
  const pending=new Set();let sdkRequests=0,sdkCallbacks=0,lastSdkActivity=now();
  const wrap=(target,name,factory)=>{const raw=target[name];if(typeof raw!=='function')return;
    const replacement=factory(raw);target[name]=replacement;undo.push(()=>{if(target[name]===replacement)target[name]=raw;});};
  wrap(proto,'bindFramebuffer',raw=>function(target,fb){const result=raw.call(this,target,fb);
    if(target===this.FRAMEBUFFER||target===this.DRAW_FRAMEBUFFER)bindings.set(this,fb);return result;});
  wrap(proto,'colorMask',raw=>function(...args){const result=raw.apply(this,args);writes.set(this,args.slice(0,4).some(Boolean));return result;});
  for(const name of ['drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced','clear','blitFramebuffer','clearBufferfv','clearBufferiv','clearBufferuiv']){
    wrap(proto,name,raw=>function(...args){
      const color=name==='clear'?!!(args[0]&this.COLOR_BUFFER_BIT):name==='blitFramebuffer'?!!(args[8]&this.COLOR_BUFFER_BIT):name.startsWith('clearBuffer')?args[0]===this.COLOR:true;
      const phase=active,observe=phase&&color&&this.canvas===canvasOf()&&!bindings.get(this);
      const started=observe?now():0,result=raw.apply(this,args);
      if(observe)phase.colorOps.push({name,kind:name==='blitFramebuffer'?'content':writes.get(this)===false?'masked':name.startsWith('clear')?'clear':'content',
        started,ended:now(),rafId,inputId});return result;
    });
  }
  wrap(env,'requestAnimationFrame',raw=>function(fn){
    const probe=fn.g5SamplerRaf===true;let handle;
    if(!probe){++sdkRequests;lastSdkActivity=now();}
    handle=raw.call(env,function(timestamp){
      if(!probe){pending.delete(handle);++sdkCallbacks;}
      const phase=active,previous=rafId;let row;
      if(phase){row={id:++nextRaf,timestamp,started:now(),source:probe?'probe':'sdk'};rafId=row.id;}
      try{return fn.call(this,timestamp);}finally{
        if(row){row.ended=now();phase.rafCallbacks.push(row);}rafId=previous;
        if(!probe)lastSdkActivity=now();
      }
    });
    if(!probe)pending.add(handle);return handle;
  });
  wrap(env,'cancelAnimationFrame',raw=>function(handle){const result=raw.call(env,handle);pending.delete(handle);return result;});
  for(const name of ['info','log','warn','error','debug'])wrap(env.console,name,raw=>function(...args){
    if(active&&typeof args[0]==='string'&&/^(AXIOM_NAVIGATION_TIMING |AXIOM_FRAME_TIMING )/.test(args[0]))
      active.nativeRecords.push({at:now(),rafId,inputId,text:args[0]});
    return raw.apply(this,args);
  });
  return {
    begin(phase){if(active)throw Error('overlapping observation');
      phase.observationVersion=2;phase.inputEvents=[];phase.colorOps=[];phase.rafCallbacks=[];phase.nativeRecords=[];active=phase;},
    end(phase){if(active!==phase)throw Error('foreign observation');phase.observationEnded=now();active=null;},
    dispatch(target,event){if(!active)return target.dispatchEvent(event);
      const phase=active,previous=inputId,row={id:++nextEvent,type:event.type,started:now()};inputId=row.id;
      try{return target.dispatchEvent(event);}finally{row.ended=now();phase.inputEvents.push(row);inputId=previous;}
    },
    activity(){return {sdkRequests,sdkCallbacks,pending:pending.size,lastSdkActivity,at:now()};},
    restore(){if(active)throw Error('active observation restore');for(const fn of undo.reverse())fn();}
  };
}
