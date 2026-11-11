// Offline, independent UR3 correctness completion. Never imported by the SDK.
import fs from 'node:fs/promises';
import path from 'node:path';
import {execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
import {comparePixels,publicHudBand} from './ur0/pixel_oracle.mjs';
const sha=b=>createHash('sha256').update(b).digest('hex');
const root=path.resolve(new URL('../../..',import.meta.url).pathname);
const result=path.join(root,'.aegis/results/G5-UR3-continuous-boundaries-r1');
const base=path.join(result,'local-web/G5U3C1-RECOVERY-FINAL');
const oracle=path.join(root,'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle');
const expected=path.join(root,'out/G5-UR3-continuous-boundaries-r1/expected-dom');
const projector=path.join(root,'out/G5-exact-query-working-set-web-r2/offline-snapshot');
const checker=path.join(root,'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip');
const output=path.join(result,'pixel-completions-recovery');
const write=async(p,v)=>fs.writeFile(p,JSON.stringify(v,null,2)+'\n');
const crop=(b,w,r,channels)=>{const out=new Uint8Array(r.width*r.height*channels);for(let y=0;y<r.height;++y)out.set(b.subarray(((r.y+y)*w+r.x)*channels,((r.y+y)*w+r.x+r.width)*channels),y*r.width*channels);return out;};
const cache=new Map(),invocations=[],rows=[];
async function reference(snapshot,request){
 const bytes=await fs.readFile(snapshot),key=sha(bytes)+'/'+JSON.stringify(request);
 if(cache.has(key))return cache.get(key);
 const dir=path.join(output,'reference-'+String(invocations.length).padStart(3,'0'));await fs.mkdir(dir);
 const requestFile=path.join(dir,'request.json'),prefix=path.join(dir,'reference');await write(requestFile,request);
 const plan={snapshot,snapshot_sha256:sha(bytes),request,oracle_sha256:sha(await fs.readFile(oracle)),timeout_seconds:60,rss_mib:4096};await write(path.join(dir,'plan.json'),plan);
 let error;try{execFileSync('python3',[path.join(root,'verification/runners/g5/bounded_worker.py'),'--prefix',path.join(dir,'guard'),'--timeout','60','--rss-mib','4096','--',oracle,'--pixels',snapshot,requestFile,prefix],{timeout:70000,encoding:'utf8',maxBuffer:1024*1024});}catch(e){error=String(e);}
 const guard=JSON.parse(await fs.readFile(path.join(dir,'guard.worker.json'),'utf8'));
 const v={plan,guard,error,status:guard.reason==='completed'&&guard.cleanup_complete&&!guard.remaining_owned_pids.length?'REFERENCE_RENDERED':'MISSING_REFERENCE',prefix};invocations.push(v);await write(path.join(dir,'invocation.json'),v);await write(path.join(result,'recovery-oracle-invocations.json'),{status:'IN_PROGRESS',unique:invocations.length,parallelism:1,invocations});
 if(v.status!=='REFERENCE_RENDERED')throw Error('MISSING_REFERENCE '+snapshot);
 const ref={...v,rgba:await fs.readFile(prefix+'.rgba'),edge:await fs.readFile(prefix+'.edge')};cache.set(key,ref);return ref;
}
async function evaluate(actual,snapshot,request,roi=null,hud=null){
 const ref=await reference(snapshot,request);let a=actual,b=ref.rgba,e=ref.edge,h=hud;
 if(roi){a=crop(a,request.width,roi,4);b=crop(b,request.width,roi,4);e=crop(e,request.width,roi,1);if(h)h=crop(h,request.width,roi,1);}
 return{...comparePixels(a,b,e,h),roi,request,actual_sha256:sha(actual),reference:{path:ref.prefix+'.rgba',sha256:sha(ref.rgba),snapshot,plan:ref.plan},band:{path:ref.prefix+'.edge',sha256:sha(ref.edge)}};
}
export async function complete(){
 if(sha(await fs.readFile(oracle))!=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')throw Error('foreign CPU oracle');
 await fs.mkdir(output,{recursive:true});const prior=JSON.parse(await fs.readFile(path.join(result,'independent-pixel-proof.json'),'utf8'));const oldLive=JSON.parse(await fs.readFile(path.join(result,'live-independent-pixel-proof-r2.json'),'utf8'));prior.rows.push(...oldLive.rows);for(const row of prior.rows)for(const proof of row.final??[]){if(proof.status!=='QUALIFIED')continue;const reference=proof.reference,bytes=await fs.readFile(reference.snapshot),rgba=await fs.readFile(reference.path),edge=await fs.readFile(proof.band.path);if(sha(rgba)!==reference.sha256||sha(edge)!==proof.band.sha256)throw Error('prior reference changed');cache.set(sha(bytes)+'/'+JSON.stringify(proof.request),{status:'REFERENCE_RENDERED',prefix:reference.path.slice(0,-5),rgba,edge,plan:reference.plan,reuse:true});}
 const request=JSON.parse(await fs.readFile(path.join(root,'out/G5-UR3-continuous-boundaries-r1/recovery-final-request.json'),'utf8'));
 for(const spec of request.cases){
  const dir=path.join(base,spec.id),record={id:spec.id,status:'STARTED',final:[],active:[]};
  let row;try{row=JSON.parse(await fs.readFile(path.join(dir,'raw-row.json'),'utf8'));}catch(error){record.status='MISSING_ROW';rows.push(record);continue;}
  if(row.status!=='QUALIFIED'){record.status='FAILED_WEB_ROW';record.web_error=row.error;rows.push(record);continue;}
  const local=path.join(output,spec.id);await fs.mkdir(local);
  try{
   for(const stage of ['final','redo']){
    const metadata=row[stage],pixels=await fs.readFile(path.join(dir,stage+'.rgba'));
    let snapshot=path.join(dir,stage+'.axsnap'),roi=null,projection=null;
    const mixed=['mixed-continuous','foreground-idle'].includes(spec.kind);
    if(mixed){const original=snapshot;snapshot=path.join(local,'ink-'+stage+'.axsnap');projection=JSON.parse(execFileSync(projector,['--project',original,snapshot],{encoding:'utf8',timeout:10000}));roi={x:0,y:0,width:300*spec.dpr,height:760*spec.dpr};
     // Non-ink starts at x350+; final fixed camera inverse maps this ROI's
     // right edge below world350. This proof is geometry-based, not diff-based.
     if((300-metadata.state.camera.x)/metadata.state.camera.scale>=350)throw Error('mixed ink ROI intersects typed objects');
    }
    const req={width:metadata.readback.canvas.width,height:metadata.readback.canvas.height,logical_width:1100,logical_height:760,camera:metadata.state.camera,hud_bounds:[]};
    const proof=await evaluate(pixels,snapshot,req,roi);record.final.push({stage,projection,...proof});await write(path.join(local,stage+'-proof.json'),proof);
   }
   if(spec.kind==='masked-affine')for(const active of row.active){
    if(!active.label?.startsWith('ink-'))continue;
    const input=path.join(local,active.label+'-expected-request.json'),clone=path.join(local,active.label+'-expected.axsnap');await write(input,{selected:active.selected,expectedTransform:active.expectedTransform});execFileSync(expected,[path.join(dir,active.label+'-snapshot.axsnap'),input,'affine',clone],{timeout:10000});
    const bounds=active.selected.bounds.map((v,i)=>v+(i%2===0?80:30));const req={width:1100*spec.dpr,height:760*spec.dpr,logical_width:1100,logical_height:760,camera:row.initial.state.camera,hud_bounds:[bounds]};
    const pixels=await fs.readFile(path.join(dir,active.label+'-active.rgba')),proof=await evaluate(pixels,clone,req,null,publicHudBand(req));record.active.push({label:active.label,...proof});await write(path.join(local,active.label+'-proof.json'),proof);
   }
   if(spec.kind==='live-prefix'){
    const sealRequest=path.join(local,'seal-request.json');await write(sealRequest,{samples:row.liveSamples,config:row.brushConfig});record.seal=JSON.parse(execFileSync(expected,[path.join(dir,'final.axsnap'),sealRequest,'verify-seal',path.join(local,'unused')],{encoding:'utf8',timeout:10000}));
    for(const active of row.active){const input=path.join(local,active.label+'-expected-request.json'),clone=path.join(local,active.label+'-expected.axsnap');await write(input,{samples:row.liveSamples,config:row.brushConfig,count:active.confirmed});execFileSync(expected,[path.join(dir,'initial.axsnap'),input,'live',clone],{timeout:10000});
     const req={width:1100*spec.dpr,height:760*spec.dpr,logical_width:1100,logical_height:760,camera:active.camera,hud_bounds:[]},ref=await reference(clone,req),target=crop(ref.rgba,req.width,active.rect,4),edge=crop(ref.edge,req.width,active.rect,1),composite=new Uint8Array(target.length);
     for(let p=0;p<target.length;p+=4){const alpha=active.previewVisibility==='visible'?active.rgba[p+3]/255:0;for(let c=0;c<3;++c)composite[p+c]=Math.round((alpha?active.rgba[p+c]:0)+(1-alpha)*active.canonicalRGBA[p+c]);composite[p+3]=Math.round(255*alpha+(1-alpha)*active.canonicalRGBA[p+3]);}
     const proof={label:active.label,confirmed:active.confirmed,rect:active.rect,preview_visibility:active.previewVisibility,...comparePixels(composite,target,edge),composite_sha256:sha(composite),reference:{path:ref.prefix+'.rgba',sha256:sha(ref.rgba),snapshot:clone},band:{path:ref.prefix+'.edge',sha256:sha(ref.edge)},paint:{rgb:[1,.62,.04],opacity:.92},css_composition:'premultiplied source-over canonical+preview'};record.active.push(proof);await write(path.join(local,active.label+'-proof.json'),proof);
    }
   }
   for(const active of row.active)if(active.status==='QUALIFIED')record.active.push({label:active.label,status:'QUALIFIED',positive:active.positive,beforeUp:active.beforeUp,rect:active.rect,color:active.color,metadata:active.metadata});
   record.roundtrip=JSON.parse(execFileSync(checker,[path.join(dir,'final.axsnap'),path.join(dir,'redo.axsnap')],{encoding:'utf8',timeout:10000}));
   record.status=record.final.length===2&&record.final.every(x=>x.status==='QUALIFIED')&&record.active.every(x=>x.status==='QUALIFIED')?'QUALIFIED':'FROZEN_PIXEL_OR_FEEDBACK_FAILURE';
  }catch(error){record.status='INCOMPLETE';record.error=String(error.stack??error);}
  rows.push(record);await write(path.join(local,'row-proof.json'),record);await write(path.join(result,'recovery-independent-pixel-proof.json'),{status:'IN_PROGRESS',rows,original_AA_unchanged:true});console.log(JSON.stringify({case:spec.id,status:record.status,unique:invocations.length}));
 }
 const status=rows.length===3&&rows.every(x=>x.status==='QUALIFIED')?'QUALIFIED':'BOUNDED_REQUIRED_PROOF_INCOMPLETE';await write(path.join(result,'recovery-independent-pixel-proof.json'),{status,rows,original_AA_unchanged:true});await write(path.join(result,'recovery-active-feedback-proof.json'),{status,rows:rows.map(x=>({id:x.id,status:x.status,active:x.active,seal:x.seal}))});await write(path.join(result,'recovery-oracle-invocations.json'),{status:'COMPLETE',unique:invocations.length,parallelism:1,invocations});return status;
}
if(process.argv[1]===new URL(import.meta.url).pathname)await complete();
