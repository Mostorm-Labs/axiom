// Offline sequential independent references; frozen ab7a oracle/AA policy.
import fs from 'node:fs/promises';import path from 'node:path';import {execFileSync} from 'node:child_process';import {createHash} from 'node:crypto';
import {comparePixels} from './ur0/pixel_oracle.mjs';
const root=process.cwd(),result=path.join(root,'.aegis/results/G5-long-query-preparation-r1'),output=path.join(root,'out/G5-long-query-preparation-r1/pixels');
const oracle=path.join(root,'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),checker=path.join(root,'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip'),projector=path.join(root,'out/G5-exact-query-working-set-web-r2/offline-snapshot');
const sha=b=>createHash('sha256').update(b).digest('hex'),write=(p,v)=>fs.writeFile(p,JSON.stringify(v,null,2)+'\n');
const cache=[],rows=[],invocations=[];
async function reference(snapshot,request){
 for(const c of cache){
  if(JSON.stringify(c.request)!==JSON.stringify(request))continue;
  let q;try{q=JSON.parse(execFileSync(checker,[snapshot,c.snapshot],{encoding:'utf8',timeout:10000}));}catch{continue;}
  if(q.objects_equal&&q.manifest_background_equal&&q.document_digest_equal)return{...c,reuse:q};
 }
 const directory=path.join(output,'reference-'+invocations.length);await fs.mkdir(directory);const prefix=path.join(directory,'reference'),requestFile=path.join(directory,'request.json');await write(requestFile,request);
 const plan={snapshot,snapshot_sha256:sha(await fs.readFile(snapshot)),request,oracle_sha256:sha(await fs.readFile(oracle)),seconds:60,rss_mib:4096};await write(path.join(directory,'plan.json'),plan);
 let error;try{execFileSync('python3',[path.join(root,'verification/runners/g5/bounded_worker.py'),'--prefix',path.join(directory,'guard'),'--timeout','60','--rss-mib','4096','--',oracle,'--pixels',snapshot,requestFile,prefix],{encoding:'utf8',timeout:70000,maxBuffer:1024*1024});}catch(e){error=String(e);}
 const guard=JSON.parse(await fs.readFile(path.join(directory,'guard.worker.json'),'utf8'));invocations.push({plan,guard,error});await write(path.join(result,'oracle-invocations.json'),{status:'IN_PROGRESS',invocations,parallelism:1});
 if(guard.reason!=='completed'||!guard.cleanup_complete)throw Error('MISSING_BOUNDED_REFERENCE');
 const c={snapshot,request,rgba:prefix+'.rgba',edge:prefix+'.edge',plan};cache.push(c);return c;
}
await fs.mkdir(output,{recursive:true});
if(sha(await fs.readFile(oracle))!=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')throw Error('foreign frozen oracle');
for(const p of ['G5-crossframe-history-pressure-r1/independent-pixel-proof.json','G5-exact-history-path-residency-r1/independent-pixel-proof.json','G5-convergence-idle-up-r1/up-independent-pixel-proof.json','G5-convergence-idle-up-r1/up-growth-independent-pixel-proof.json']){
 const old=JSON.parse(await fs.readFile(path.join(root,'.aegis/results',p),'utf8'));
 for(const row of old.rows)for(const proof of row.proofs??[]){
  if(proof.status!=='QUALIFIED'||!proof.plan?.request)continue;
  const snapshot=proof.plan.snapshot??proof.plan.reference_snapshot;if(!snapshot)continue;
  const rgba=await fs.readFile(proof.reference.path),edge=await fs.readFile(proof.band.path);
  if(sha(rgba)!==proof.reference.sha256||sha(edge)!==proof.band.sha256)throw Error('frozen reference modified');
  cache.push({snapshot,request:proof.plan.request,rgba:proof.reference.path,edge:proof.band.path,plan:proof.plan});
 }
}
const roster=[...['old-burst','candidate-burst','old-paced','candidate-paced','old-growth','candidate-growth'].map(id=>['G5LQP1-PAIRS',id]),...['candidate-burst-diagnostic','candidate-paced-diagnostic'].map(id=>['G5LQP1-DIAGNOSTIC',id]),['G5LQP1-FEEDBACK','candidate-mixed-feedback']];
for(const [win,id]of roster){
 const dir=path.join(result,'local-web',win,id),proofs=[],mixed=id==='candidate-mixed-feedback';
 for(const stage of ['final','redo']){
  const original=path.join(dir,stage+'.axsnap');let snapshot=original,projection=null;
  if(mixed){snapshot=path.join(output,id+'-'+stage+'-ink.axsnap');projection=JSON.parse(execFileSync(projector,['--project',original,snapshot],{encoding:'utf8',timeout:10000}));}
  const request={width:2862,height:2020,logical_width:1431,logical_height:1010,camera:{scale:1,x:0,y:0},hud_bounds:[]};
  try{
   const ref=await reference(snapshot,request),actual=await fs.readFile(path.join(dir,stage+'.rgba')),expected=await fs.readFile(ref.rgba),edge=await fs.readFile(ref.edge);
   const roi=mixed?{x:0,y:0,width:1600,height:800}:null;
   const crop=(b,channels)=>roi?Buffer.concat(Array.from({length:roi.height},(_,y)=>b.subarray((y*request.width)*channels,(y*request.width+roi.width)*channels))):b;
   proofs.push({stage,...comparePixels(crop(actual,4),crop(expected,4),crop(edge,1)),roi,projection,snapshot,snapshot_sha256:sha(await fs.readFile(snapshot)),original_snapshot_sha256:sha(await fs.readFile(original)),actual_sha256:sha(actual),reference:{path:ref.rgba,sha256:sha(expected)},band:{path:ref.edge,sha256:sha(edge)},request,plan:ref.plan,reuse:ref.reuse});
  }catch(error){proofs.push({stage,status:'MISSING_REFERENCE',error:String(error)});}
 }
 const row={window:win,id,status:proofs.length===2&&proofs.every(p=>p.status==='QUALIFIED')?'QUALIFIED':'INCOMPLETE',proofs};rows.push(row);await write(path.join(result,'independent-pixel-proof.json'),{status:'IN_PROGRESS',rows});console.log(JSON.stringify({id,status:row.status,uniqueReferences:invocations.length}));
}
await write(path.join(result,'independent-pixel-proof.json'),{status:rows.every(r=>r.status==='QUALIFIED')?'QUALIFIED':'FROZEN_REQUIRED_PROOF_INCOMPLETE',rows,original_AA_unchanged:true,mixed_scope:'ink projection disjoint worldx<800,y<400 plus separately typed resource/table canonical painter; not fullmixedoracle',no_current_pixels_as_reference:true});
await write(path.join(result,'oracle-invocations.json'),{status:'COMPLETE',parallelism:1,invocations});
