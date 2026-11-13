import fs from 'node:fs/promises';import path from 'node:path';import {gunzipSync} from 'node:zlib';import crypto from 'node:crypto';
import{comparePixels}from'./ur0/pixel_oracle.mjs';
const root=process.cwd(),out=path.join(root,'out/G5-postworker-render-reuse-r1'),res=path.join(root,'.aegis/results/G5-postworker-render-reuse-r1');
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
async function bytes(p){try{return await fs.readFile(p);}catch(e){if(e.code!=='ENOENT')throw e;return gunzipSync(await fs.readFile(p+'.gz'));}}
const inventory=JSON.parse(await fs.readFile(path.join(out,'reference-inventory.json'),'utf8')),primary=[];
for(const id of ['RR-D4-BASELINE','RR-D4-CANDIDATE','RR-D1-CANDIDATE']){
 const dir=path.join(out,'web',id),raw=JSON.parse((await bytes(path.join(dir,'raw.json'))).toString());const frame=raw.snapshots.followup,actualSnap=await bytes(path.join(dir,'followup.axsnap'));
 const width=frame.readback.context.drawingBufferWidth,height=frame.readback.context.drawingBufferHeight,camera=frame.state.camera;
 const matched=inventory.find(v=>v.snapshot_sha256===sha(actualSnap)&&v.width===width&&v.height===height&&JSON.stringify(v.camera)===JSON.stringify(camera)&&v.resources.length===0&&v.available_rgba);
 if(!matched)throw Error('no exact original basis for '+id);
 const expected=await bytes(matched.output_prefix+'.rgba'),edge=await bytes(matched.output_prefix+'.edge'),actual=await bytes(path.join(dir,'followup.rgba'));
 const comparison=comparePixels(actual,expected,edge);primary.push({id,...comparison,snapshot_sha256:sha(actualSnap),camera,width,height,actual_sha256:sha(actual),reference_sha256:sha(expected),reference_edge_sha256:sha(edge),reference_request:matched.request,reference_request_sha256:matched.sha256,reference_prefix:matched.output_prefix,reuse:'exact fullsnapshot/currentcamera/target/resources/originalCPUbinary+brush+AA policy',readback:frame.readback});
}
await fs.writeFile(path.join(res,'primary-pixels.json'),JSON.stringify({rows:primary},null,2)+'\n');
const zoom=[];
for(const name of ['append-zoom-out-immediate','partial-erase-zoom-out-immediate']){
 const dirs=['BASELINE','CANDIDATE'].map(s=>path.join(out,'web','RR-ZOOM-D4-'+s,name));const raws=await Promise.all(dirs.map(d=>bytes(path.join(d,'raw.json')).then(b=>JSON.parse(b.toString()))));
 const finals=await Promise.all(dirs.map(d=>bytes(path.join(d,'final.axsnap'))));const operations=await Promise.all(dirs.map(async d=>Promise.all([0,1,2].map(i=>bytes(path.join(d,`operation-${i}.axsnap`)).then(sha)))));
 const paired={};for(const stage of['feedback','final','redo']){const a=await bytes(path.join(dirs[0],stage+'.rgba')),b=await bytes(path.join(dirs[1],stage+'.rgba'));let changed=0,maximum=0;for(let i=0;i<a.length;++i)if(a[i]!==b[i]){++changed;maximum=Math.max(maximum,Math.abs(a[i]-b[i]));}paired[stage]={baseline_sha256:sha(a),candidate_sha256:sha(b),exact_equal:sha(a)===sha(b),changed_channels:changed,maximum_difference:maximum,independent_oracle:'zoomout-independent-pixels.json corrected original reference; paired equality cannot substitute'};}
 const rows=raws.map((r,i)=>{const nav=r.navigation[0],feedback=r.feedback,up=r.events.findLast(e=>e.phase==='up');return {role:i?'CANDIDATE':'BASELINE',status:r.status,up_handler_ms:up.ended-up.started,up_to_first_wheel_ms:nav.started-r.upReturned,input_queue_ms:r.navigation.map(e=>e.started-e.planned),wheel_handlers_ms:r.navigation.map(e=>e.ended-e.started),first_GL_proxy_after_first_wheel_ms:r.proxy.firstDefaultFboContentAt-nav.started,complete_camera_canonical_feedback_ms:feedback?.at-nav.started,feedback_readback_ms:feedback?.readFinished-feedback?.at,up_to_settled_ms:r.settledAt-r.upReturned,canonical_final_after_first_wheel_ms:r.settledAt-nav.started,camera:r.states.after.camera,expectedCamera:r.expectedCamera,errors:r.errors,closed:r.closed,source:dirs[i].replace(root+'/','')+'/raw.json'};});
 zoom.push({row:name,rows,paired_pixels:paired,paired_snapshot_byte_equal:sha(finals[0])===sha(finals[1]),snapshots:finals.map(sha),all3_operations_byte_equal:JSON.stringify(operations[0])===JSON.stringify(operations[1]),reference:'initial10s misconfigured attempts preserved; corrected original reference completed35.7s each; zoomout-independent-pixels.json',earliest_retained_LOD_feedback:'NOT_QUALIFIED; measured feedback is fullcamera after6wheels canonical onFrame only',classification:'NO_MATERIAL_IMPROVEMENT_AND_ORIGINAL_PIXEL_FINDING'});
}
await fs.writeFile(path.join(res,'zoomout-results.json'),JSON.stringify({rows:zoom,not_overall_performance_PASS:true},null,2)+'\n');
console.log(JSON.stringify({primary:primary.map(r=>({id:r.id,status:r.status,interior_mismatches:r.interior_mismatched_pixels})),zoom:zoom.map(v=>({row:v.row,pixels:v.paired_pixels,rows:v.rows}))},null,2));
