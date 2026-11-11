// Isolated test host only. It observes actual create/delete owners without
// changing parameters, GL state, error queues, completion or product callbacks.
export function installContextObservation(){
 const contexts=new WeakMap(),objects=new WeakMap(),events=[],mismatches=[];
 let nextContext=0,nextObject=0,created=0,deleted=0,unknown=0,overflow=false;
 const context=gl=>{if(!contexts.has(gl))contexts.set(gl,{id:++nextContext,canvas:gl.canvas?.id??'',width:gl.canvas?.width,height:gl.canvas?.height});return contexts.get(gl);};
 const pair=[['Texture','Texture'],['Buffer','Buffer'],['Renderbuffer','Renderbuffer'],['Framebuffer','Framebuffer'],['Program','Program'],['Shader','Shader'],['Sampler','Sampler'],['VertexArray','VertexArray'],['Query','Query'],['TransformFeedback','TransformFeedback'],['Sync','Sync']];
 const proto=globalThis.WebGL2RenderingContext?.prototype;
 if(!proto){globalThis.convergenceContextObservation={snapshot:()=>({status:'UNAVAILABLE'})};return;}
 for(const[create,drop]of pair){const cname=create==='Sync'?'fenceSync':'create'+create,dname='delete'+drop,originalCreate=proto[cname],originalDelete=proto[dname];
  if(originalCreate)proto[cname]=function(...args){const object=originalCreate.apply(this,args);if(object){const owner=context(this),row={id:++nextObject,kind:create,owner,createdAt:performance.now()};objects.set(object,row);++created;}return object;};
  if(originalDelete)proto[dname]=function(object,...args){if(object){++deleted;const owner=objects.get(object),current=context(this),row={method:dname,at:performance.now(),objectId:owner?.id,owner:owner?.owner,current,createdAt:owner?.createdAt,kind:owner?.kind};if(!owner)++unknown;else if(owner.owner.id!==current.id){row.stack=new Error('foreign WebGL object deletion').stack;if(mismatches.length<64)mismatches.push(row);else overflow=true;}if(events.length<128)events.push(row);}return originalDelete.call(this,object,...args);};
 }
 globalThis.convergenceContextObservation={snapshot:()=>({status:'OBSERVED',created,deleted,unknown,contextCount:nextContext,mismatches,firstDeletes:events,overflow,scope:'actual object owner vs delete receiver; no GL errors drained or operations suppressed'})};
}
