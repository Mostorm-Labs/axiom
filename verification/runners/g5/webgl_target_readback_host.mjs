import fs from 'node:fs/promises';import path from 'node:path';import{createHash}from'node:crypto';
export async function persistCapturedFailures({descriptors,readChunk,release,write,metadata,cache=new Map()}){
 const results=[];
 for(const d of descriptors){
  if(cache.has(d.id)){results.push(cache.get(d.id));continue;}
  const row={...d,buffers:[],persistence_started:Date.now()};
  for(const b of d.buffers){
   const saved={...b};row.buffers.push(saved);if(b.status!=='AVAILABLE_CAPTURED')continue;
   try{
    const chunks=[];let count=0;
    for(let off=0;off<b.length;off+=262144){const part=Buffer.from(await readChunk(b.key,off,Math.min(262144,b.length-off)),'base64');if(part.length!==Math.min(262144,b.length-off))throw Error('captured byte stream length');chunks.push(part);count+=part.length;}
    const bytes=Buffer.concat(chunks,count);saved.file=await write(d.id,b,bytes);saved.sha256=createHash('sha256').update(bytes).digest('hex');saved.status='PERSISTED';await release?.(b.key);
   }catch(e){saved.status='MISSING';saved.reason='Persistence failed: '+String(e.stack??e);}
  }
  row.status=row.buffers.every(b=>b.status==='PERSISTED')?'COMPLETE':'MISSING';row.persistence_finished=Date.now();results.push(row);cache.set(d.id,row);await metadata?.(row);
 }
 return results;
}
const pageCaches=new WeakMap();
export async function persistPageFailures(page,out,prefix){
 if(!pageCaches.has(page))pageCaches.set(page,new Map());
 const descriptors=await page.evaluate(async()=>{const m=await import('/g5/webgl_target_readback.mjs');return m.failureDescriptors();});
 return persistCapturedFailures({descriptors,cache:pageCaches.get(page),
  readChunk:(key,off,count)=>page.evaluate(async({key,off,count})=>(await import('/g5/webgl_target_readback.mjs')).failureChunk(key,off,count),{key,off,count}),
  release:key=>page.evaluate(async key=>(await import('/g5/webgl_target_readback.mjs')).releaseFailureBuffer(key),key),
  write:async(id,b,bytes)=>{const p=path.join(out,prefix+'-'+id+'-'+b.role+'.rgba');await fs.writeFile(p,bytes);return p;},
  metadata:r=>fs.writeFile(path.join(out,prefix+'-'+r.id+'-capture.json'),JSON.stringify(r,null,2)+'\n')});
}
export function classifyFailureCaptures(captures){
 if(!captures?.length)return'FAILURE_ARTIFACT_CAPTURE_MISSING';
 const c=captures.at(-1);
 const trigger=c.buffers.find(b=>b.role==='trigger-ROI');
 if(trigger?.readback?.status!=='VALID')return trigger?.readback?.errors?.preexisting?.codes?.length?'UNEXPECTED_PREEXISTING_GL_ERROR':'READ_TARGET_ERROR_OR_STATE_RESTORATION_FAILURE';
 if(c.buffers.some(b=>b.status!=='PERSISTED'))return'FAILURE_ARTIFACT_CAPTURE_MISSING';
 return'PRODUCT_PIXEL_FAILURE';
}
