// Attribution-only additions to the existing observer. Observe calls made by
// the SDK; never enqueue GPU work, force completion or inspect private state.
export function observeExistingGpuCalls(env=globalThis){
  const proto=env.WebGL2RenderingContext.prototype,undo=[],pending=new Map();
  let active=null,next=0;
  for(const name of ['fenceSync','clientWaitSync','deleteSync','finish','readPixels']){
    const raw=proto[name];if(typeof raw!=='function')continue;
    const replacement=function(...args){
      const started=env.performance.now(),result=raw.apply(this,args),ended=env.performance.now();
      if(name==='fenceSync'&&result)pending.set(result,{id:++next,submitted:started});
      const fence=pending.get(args[0]);
      if(active){const row={name,started,ended,fence_id:fence?.id??(name==='fenceSync'?pending.get(result)?.id:null),status:typeof result==='number'?result:null};
        if(name==='clientWaitSync'&&fence&&(result===this.ALREADY_SIGNALED||result===this.CONDITION_SATISFIED)){
          row.submit_to_signal_observed_ms=ended-fence.submitted;
          row.interval_scope='completion plus polling interval; not pure GPU time';
        }active.gpuCalls.push(row);
      }
      if(name==='deleteSync')pending.delete(args[0]);return result;
    };
    proto[name]=replacement;undo.push(()=>{if(proto[name]===replacement)proto[name]=raw;});
  }
  return {begin(p){if(active)throw Error('active GPU observation');p.gpuCalls=[];active=p;},
    end(p){if(active!==p)throw Error('foreign GPU observation');active=null;},
    restore(){if(active)throw Error('active GPU observation');for(const f of undo.reverse())f();}};
}
