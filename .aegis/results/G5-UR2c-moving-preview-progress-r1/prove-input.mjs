import fs from 'node:fs/promises';import path from 'node:path';import {execFileSync}from 'node:child_process';import{createHash}from'node:crypto';
import {point,gestures}from '../../../verification/runners/g5/moving_erase_progress_contract.mjs';
const ROOT=process.cwd(),B=path.join(ROOT,'.aegis/results/G5-UR2c-moving-preview-progress-r1'),O=path.join(ROOT,'out/G5-UR2c-moving-preview-progress-r1');
const oracle=path.join(ROOT,'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),derive=path.join(O,'moving-reference');
const sha=b=>createHash('sha256').update(b).digest('hex'),write=(p,v)=>fs.writeFile(p,JSON.stringify(v,null,2)+'\n');
if(sha(await fs.readFile(oracle))!=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')throw Error('old CPU oracle identity');
const snapshot=path.join(B,'local-web/UR2CMP1-R1/baseline.axsnap'),meta=JSON.parse(await fs.readFile(path.join(B,'local-web/UR2CMP1-R1/baseline.json')));
if(sha(await fs.readFile(snapshot))!==meta.sha256)throw Error('baseline identity');
const folder=path.join(B,'input-proof');await fs.mkdir(folder,{recursive:true});const specs=gestures(),history=[],checks=[];
const render=async(snap,request,prefix)=>{
 await write(prefix+'.request.json',request);const args=['--pixels',snap,prefix+'.request.json',prefix];
 const rendered=JSON.parse(execFileSync(oracle,args,{timeout:10000,maxBuffer:1024*1024,encoding:'utf8'}));if(rendered.status!=='REFERENCE_RENDERED')throw Error('old reference failure');await write(prefix+'.reference.json',rendered);return new Uint8Array(await fs.readFile(prefix+'.rgba'));
};
for(const [index,spec]of specs.entries()){
 const prefix=path.join(folder,'gesture-'+index),req=prefix+'.history.json',state=prefix+'.axsnap';await write(req,{history});
 const result=JSON.parse(execFileSync(derive,[snapshot,req,state],{timeout:10000,maxBuffer:1024*1024,encoding:'utf8'}));if(result.objects!==19)throw Error('derived reference object count');
 const common={width:590,height:900,logical_width:590,logical_height:900,camera:{scale:1,x:0,y:0},hud_bounds:[]};const before=await render(state,common,prefix+'-before');
 for(const fraction of [.25,.5,.75]){
  const i=Math.floor(spec.moves*fraction),confirmed=Math.max(1,i-9),trace=Array.from({length:confirmed+1},(_,j)=>point(spec.kind,spec.g,j));
  const active=await render(state,{...common,live_erase:{view_points:trace,diameter:32}},prefix+'-prefix-'+confirmed);
  const [x,y]=point(spec.kind,spec.g,confirmed),rect={x:Math.floor(x-8),y:Math.floor(y-8),width:16,height:16};let pigment=0,erased=0,notWhite=0;
  for(let y=rect.y;y<rect.y+16;++y)for(let x=rect.x;x<rect.x+16;++x){const p=(y*590+x)*4;if([0,1,2].some(c=>before[p+c]<253))++pigment;if([0,1,2].some(c=>Math.abs(before[p+c]-active[p+c])>2))++erased;if([0,1,2].some(c=>active[p+c]<253)||active[p+3]!==255)++notWhite;}
  const row={index,spec,fraction,i,prefix:confirmed,rect_css:rect,pigment_pixels:pigment,erased_pixels:erased,expected_nonwhite_pixels:notWhite,baseline_sha256:meta.sha256,prior_state_sha256:sha(await fs.readFile(state)),reference_prefix:prefix+'-prefix-'+confirmed,reference_rgba_sha256:sha(active),status:pigment>0&&erased>0&&notWhite===0?'QUALIFIED':'FAILED'};checks.push(row);console.log(JSON.stringify(row));
 }
 history.push(Array.from({length:spec.moves+1},(_,i)=>point(spec.kind,spec.g,i)));
}
const result={status:checks.every(x=>x.status==='QUALIFIED')?'VERIFIED':'VERIFICATION_OR_PACKAGE_DEFECT',baseline:meta,oracle:{path:oracle,sha256:sha(await fs.readFile(oracle))},adapter:{path:derive,sha256:sha(await fs.readFile(derive))},method:'All18 ROIs fixed from input point(prefix i-9),16x16 CSS; original uncached CPU reference with earlier committed masks; positive surviving pigment and fully erased interior. No candidate pixels or AA expansion.',checks};await write(path.join(B,'input-proof.json'),result);if(result.status!=='VERIFIED')process.exitCode=2;
