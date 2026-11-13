// Final stage only: actual colors cannot influence supplemental eligibility.
import fs from'node:fs/promises';import path from'node:path';import{gunzipSync}from'node:zlib';import crypto from'node:crypto';
import{comparePixels}from'./ur0/pixel_oracle.mjs';
import{fields}from'./cpu_v2_wire.mjs';
const root=process.cwd(),result=path.join(root,'.aegis/results/G5-mixed-reference-qualification-r1'),out=path.join(root,'out/G5-mixed-reference-qualification-r1');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
async function read(ref){const b=await fs.readFile(path.join(root,ref.path));if(sha(b)!==ref.sha256)throw Error('binding '+ref.path);return ref.path.endsWith('.gz')?gunzipSync(b):b;}
const workload=JSON.parse(await fs.readFile(path.join(root,'.aegis/packages/G5-mixed-reference-qualification-r1/workload.json'),'utf8'));
const same=(a,b)=>JSON.stringify(a)===JSON.stringify(b);
const canonicalBasis=bytes=>fields(bytes).filter(f=>[1,2,3,4,8,9,11].includes(f.number)).map(f=>({number:f.number,type:f.type,value:f.value instanceof Uint8Array?Buffer.from(f.value).toString('hex'):String(f.value)}));
const rows=[];
for(const row of workload.rows){
 const reference=await read(row.reference['original.rgba.gz']),edge=await read(row.reference['original.edge.gz']);
 const request=JSON.parse((await read(row.reference['request.json'])).toString()),snapshot=await read(row.reference['input.axsnap.gz']);
 const supplement=gunzipSync(await fs.readFile(path.join(out,row.row,'material.edge.gz')));
 if(supplement.length!==edge.length)throw Error('supplement shape');const combined=new Uint8Array(edge.length);let originalArea=0,newArea=0;
 for(let i=0;i<edge.length;++i){originalArea+=!!edge[i];newArea+=!edge[i]&&!!supplement[i];combined[i]=edge[i]||supplement[i]?1:0;}
 for(const capture of row.captures){
  const raw=JSON.parse((await read(capture.raw)).toString());
  for(const stage of['feedback','final','redo']){
   const target=raw.states.after.camera;if(!same(target,request.camera)||request.width!==5724||request.height!==4040)throw Error('camera/targetbasis');
   const state=stage==='feedback'?raw.feedback.state:stage==='redo'?raw.redo.state:raw.final.state;
   if(!same(state.camera,request.camera)||state.generation!==state.lastCommit.presentedGeneration)throw Error('capture generation/camera');
   if(stage==='feedback'&&state.generation!==raw.states.after.generation)throw Error('feedbackdocumentgeneration');
   const capturedSnapshot=await read(capture.snapshots[stage==='feedback'?'final':stage]);
   if(!same(canonicalBasis(capturedSnapshot),canonicalBasis(snapshot)))throw Error('snapshotsemanticcontentbasis');
   const actual=await read(capture.images[stage]),before=comparePixels(actual,reference,edge),after=comparePixels(actual,reference,combined);
   rows.push({row:row.row,role:capture.role,stage,original:before,material_boundary_corrected:after,original_edge_pixels:originalArea,new_material_edge_pixels:newArea,remaining_strict_pixels:after.compared_pixels,actual: capture.images[stage],expected:row.reference['original.rgba.gz'],original_edge:row.reference['original.edge.gz'],material_edge:{path:path.relative(root,path.join(out,row.row,'material.edge.gz')),sha256:sha(await fs.readFile(path.join(out,row.row,'material.edge.gz')))},snapshot:row.reference['input.axsnap.gz'],camera:raw.states.after.camera,width:5724,height:4040,image_expected_alternates_applied:0,classification:after.status==='QUALIFIED'?'QUALIFIED':'PARTIAL_REFERENCE_CORRECTION_REMAINING_IMAGE_REALIZATION'});
  }
 }
}
await fs.writeFile(path.join(result,'pixel-comparisons.json'),JSON.stringify({rows,policy:'same exterior<=2; oldedge OR source-only supplemental materialboundary width2devicepixels; noimageexemption'},null,2)+'\n');
console.log(JSON.stringify(rows.map(r=>({row:r.row,role:r.role,stage:r.stage,raw:r.original.interior_mismatched_pixels,corrected:r.material_boundary_corrected.interior_mismatched_pixels,new_edge:r.new_material_edge_pixels,strict:r.remaining_strict_pixels,witness:r.material_boundary_corrected.witnesses})),null,2));
