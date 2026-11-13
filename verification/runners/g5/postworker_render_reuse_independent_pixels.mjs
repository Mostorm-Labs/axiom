// Verification only: original mixed pixels after bounded driver correction.
import fs from'node:fs/promises';import {gunzipSync}from'node:zlib';import path from'node:path';import crypto from'node:crypto';
const root=process.cwd();const {comparePixels}=await import(path.join(root,'verification/runners/g5/ur0/pixel_oracle.mjs'));const res=path.join(root,'.aegis/results/G5-postworker-render-reuse-r1'),out=path.join(root,'out/G5-postworker-render-reuse-r1');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');const bytes=async p=>{try{return await fs.readFile(p);}catch(e){if(e.code!=='ENOENT')throw e;return gunzipSync(await fs.readFile(p+'.gz'));}};
const rows=[];
for(const name of['append-zoom-out-immediate','partial-erase-zoom-out-immediate']){
 const prefix=path.join(out,'oracle-corrective',name,'original');const expected=await bytes(prefix+'.rgba'),edge=await bytes(prefix+'.edge'),requestPath=path.join(out,'oracle-corrective',name,'request.json'),q=JSON.parse(await fs.readFile(requestPath,'utf8')),snap=await bytes(prefix+'.axsnap');
 for(const role of['BASELINE','CANDIDATE']){
  const dir=path.join(out,'web','RR-ZOOM-D4-'+role,name),raw=JSON.parse((await bytes(path.join(dir,'raw.json'))).toString());const actualSnap=await bytes(path.join(dir,'final.axsnap'));
  if(sha(actualSnap)!==sha(snap)||JSON.stringify(raw.states.after.camera)!==JSON.stringify(q.camera))throw Error('referencebasis mismatch');
  for(const stage of['feedback','final','redo']){
   if(stage==='feedback'&&(JSON.stringify(raw.feedback.state.camera)!==JSON.stringify(q.camera)||raw.feedback.state.generation!==raw.states.after.generation))throw Error('feedback snapshot/camera notfinal');
   const actual=await bytes(path.join(dir,stage+'.rgba'));const comparison=comparePixels(actual,expected,edge);
   rows.push({row:name,role,stage,...comparison,snapshot_sha256:sha(actualSnap),actual_sha256:sha(actual),reference_sha256:sha(expected),reference_edge_sha256:sha(edge),request:requestPath.replace(root+'/',''),request_sha256:sha(await fs.readFile(requestPath)),camera:q.camera,width:q.width,height:q.height,reference_source:'originalc480binary/OperationEngine/fullScene/nullcache',feedback_scope:stage==='feedback'?'sixwheels+currentcamera+canonicalgeneration; earliestLOD notqualified':undefined});
  }
 }
}
await fs.writeFile(path.join(res,'zoomout-independent-pixels.json'),JSON.stringify({rows},null,2)+'\n');console.log(JSON.stringify(rows.map(v=>({row:v.row,role:v.role,stage:v.stage,status:v.status,failures:v.interior_mismatched_pixels,max:v.max_interior_delta,witness:v.witnesses})),null,2));
