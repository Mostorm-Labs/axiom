import fs from'node:fs/promises';import path from'node:path';import{gunzipSync}from'node:zlib';import crypto from'node:crypto';
const root=process.cwd();const{comparePixels}=await import(path.join(root,'verification/runners/g5/ur0/pixel_oracle.mjs'));const{fields}=await import(path.join(root,'verification/runners/g5/cpu_v2_wire.mjs'));const{regressionGuard}=await import(path.join(root,'verification/runners/g5/canonical_latency_guard.mjs'));
const out=path.join(root,'out/G5-canonical-convergence-latency-r1'),res=path.join(root,'.aegis/results/G5-canonical-convergence-latency-r1');const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
async function bytes(p){try{return await fs.readFile(p);}catch(e){if(e.code!=='ENOENT')throw e;return gunzipSync(await fs.readFile(p+'.gz'));}}
const content=b=>fields(b).filter(f=>[1,2,3,4,8,9,11].includes(f.number)).map(f=>({number:f.number,type:f.type,value:f.value instanceof Uint8Array?Buffer.from(f.value).toString('hex'):String(f.value)}));const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
const known=JSON.parse(await fs.readFile(path.join(res,'known-image-residual-freeze.json'))).rows;const rows=[];
for(const role of['BASELINE','CANDIDATE'])for(const row of['append-zoom-out-immediate','partial-erase-zoom-out-immediate']){
 const dir=path.join(out,'web','CL-D4-MIXED-'+role,row);let raw;try{raw=JSON.parse((await bytes(path.join(dir,'raw.json'))).toString());}catch(e){if(e.code==='ENOENT')continue;throw e;}
 for(const stage of['feedback','final','redo']){
  const k=known.find(v=>v.row===row&&v.stage===stage),baseline=await bytes(path.join(root,k.baseline.path).replace(/\.gz$/,'')),expected=await bytes(path.join(root,k.expected.path).replace(/\.gz$/,'')),edge=await bytes(path.join(root,k.edge.path).replace(/\.gz$/,'')),extra=await bytes(path.join(root,k.extra).replace(/\.gz$/,''));const union=Uint8Array.from(edge,(v,i)=>v||extra[i]?1:0);
  const state=stage==='feedback'?raw.feedback.state:raw[stage].state,metadata=stage==='feedback'?raw.feedback.readback:raw[stage+'Readback'];
  if(!same(state.camera,k.known.basis.camera)||state.generation!==state.lastCommit.presentedGeneration||metadata.context.drawingBufferWidth!==5724||metadata.context.drawingBufferHeight!==4040)throw Error('actualcamera/target/generation');
  const snapshot=await bytes(path.join(dir,(stage==='feedback'?'final':stage)+'.axsnap'));
  if(sha(snapshot)!==k.known.basis.full_snapshot_SHA)throw Error('actualsnapshotfullbasis');
  const imagePath=path.join(root,'.aegis/results/G5-object-interaction-r2-image-move-diagnostic/image-2_3mb.png');const image=sha(await fs.readFile(imagePath));if(image!==k.known.basis.image||raw.resources.entries[0].resourceId!==image.slice(0,32))throw Error('image resource binding');
  const basis={snapshot_content_digest:'samefullcanonicalcontentverifiedperstage',full_snapshot_SHA:sha(snapshot),camera:state.camera,target:[metadata.context.drawingBufferWidth,metadata.context.drawingBufferHeight],image,source_path:'image+snapshot+shader+matrix unchanged'};
  const actual=await bytes(path.join(dir,stage+'.rgba'));rows.push({role,row,stage,oldCPU:comparePixels(actual,expected,union),guard:regressionGuard(actual,baseline,expected,union,k.known,basis),actual_sha256:sha(actual),basis,known_freeze:'known-image-residual-freeze.json'});
 }
}
await fs.writeFile(path.join(res,'mixed-regression-guard.json'),JSON.stringify({rows,image_qualification:'OPEN302; scoped knownbaselineguard only',imagepath:'source-delta dependencyaudit required'},null,2)+'\n');console.log(rows.map(r=>({role:r.role,row:r.row,stage:r.stage,status:r.guard.status,globalCPU:r.oldCPU.status,new:r.guard.new_residual_pixels,known:r.guard.changed_known_pixels})));
