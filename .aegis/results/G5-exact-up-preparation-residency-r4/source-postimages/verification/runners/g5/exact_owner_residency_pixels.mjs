// Verification-only offline completion. Original oracle/comparator unchanged.
import fs from'node:fs/promises';import path from'node:path';import{execFileSync}from'node:child_process';import{createHash}from'node:crypto';
import{comparePixels}from'./ur0/pixel_oracle.mjs';
const sha=bytes=>createHash('sha256').update(bytes).digest('hex');
function documentDigest(bytes){
 let offset=0;const variable=()=>{let value=0n,shift=0n;for(;;){const b=bytes[offset++];value|=BigInt(b&127)<<shift;if(b<128)return value;shift+=7n;}};
 while(offset<bytes.length){const key=variable(),field=Number(key>>3n),wire=Number(key&7n);if(wire===0){variable();continue;}if(wire!==2)throw Error('unsupported canonical wire');const n=Number(variable()),value=bytes.subarray(offset,offset+n);offset+=n;if(field===8)return Buffer.from(value).toString('hex');}throw Error('canonical digest absent');
}
export async function completePixels(root){
 const result=path.join(root,'.aegis/results/G5-exact-up-preparation-residency-r4');
 const projector=path.join(root,'out/G5-exact-query-working-set-web-r2/offline-snapshot');
 const checker=path.join(root,'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip');
 const oracle=path.join(root,'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle');
 const guard=path.join(root,'verification/runners/g5/bounded_worker.py');
 if(sha(await fs.readFile(oracle))!=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')throw Error('foreign oracle');
 const refs=path.join(result,'references');await fs.mkdir(refs,{recursive:true});
 const cache=new Map(),invocations=[],rows=[];
 const equal=(a,b)=>{const v=JSON.parse(execFileSync(checker,[a,b],{encoding:'utf8',timeout:10000}));if(!v.objects_equal||!v.manifest_background_equal||!v.document_digest_equal)throw Error('reference reuse canonical mismatch');return v;};
 const control=path.join(root,'.aegis/results/G5-exact-query-working-set-control-r1/oracle-offline-qualification-r1');
 const controlPlan=JSON.parse(await fs.readFile(path.join(control,'plan.json'),'utf8')),controlResult=JSON.parse(await fs.readFile(path.join(control,'result.json'),'utf8'));
 for(const file of['reference.rgba','reference.edge'])if(sha(await fs.readFile(path.join(control,file)))!==controlResult.files[file].sha256)throw Error('control reference bytes changed');
 const sources=[{label:'r4',base:path.join(result,'local-web/G5OR4-FINAL-COMPARE')},{label:'r4',base:path.join(result,'local-web/G5OR4-CONTINUITY')}];
 // Seed reusable references from r3 only after their immutable files and
 // current input's complete canonical/camera identity qualify below.
 const previous=JSON.parse(await fs.readFile(path.join(root,'.aegis/results/G5-exact-fixed-scene-reverification-r3/independent-pixel-proof.json'),'utf8'));
 for(const row of previous.rows)for(const proof of row.proofs)if(proof.status==='QUALIFIED'){
  const input=proof.plan.reference_snapshot;
  const referencePath=proof.reference.path,bandPath=proof.band.path;
  if(sha(await fs.readFile(referencePath))!==proof.reference.sha256||sha(await fs.readFile(bandPath))!==proof.band.sha256)throw Error('prior reference hash mismatch');
  cache.set(proof.plan.document_digest+'/'+JSON.stringify(proof.plan.request),{input,referencePath,bandPath});
 }
 for(const source of sources)for(const name of(await fs.readdir(source.base)).sort()){
  const original=path.join(source.base,name);let row;try{row=JSON.parse(await fs.readFile(path.join(original,'qualified-row.json'),'utf8'));}catch{continue;}
  const directory=path.join(result,'pixel-completions',source.label,path.basename(source.base),name);await fs.mkdir(directory,{recursive:true});const proofs=[];
  for(const stage of['final','redo']){
   const snapshot=path.join(original,stage+'.axsnap'),actual=await fs.readFile(path.join(original,stage+'.rgba'));let input=snapshot,projection=null;
   const mixed=['witness','action'].includes(row.mode);
   if(mixed){input=path.join(directory,'ink-'+stage+'.axsnap');projection=JSON.parse(execFileSync(projector,['--project',snapshot,input],{encoding:'utf8',timeout:10000}));}
   const metadata=row[stage],request={width:metadata.readback.canvas.width,height:metadata.readback.canvas.height,logical_width:1431,logical_height:1010,camera:metadata.state.camera,hud_bounds:[]};
   const bytes=await fs.readFile(input),key=documentDigest(bytes)+'/'+JSON.stringify(request);
   const plan={source_snapshot:snapshot,source_snapshot_sha256:sha(await fs.readFile(snapshot)),reference_snapshot:input,reference_snapshot_sha256:sha(bytes),document_digest:documentDigest(bytes),request,projection,timeout_seconds:60,rss_mib:4096,oracle_sha256:'ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105'};
   let referencePath,bandPath,reuseProof=null;
   if(sha(bytes)===controlPlan.snapshot.sha256&&JSON.stringify(request)===JSON.stringify(controlPlan.request)){
    referencePath=path.join(control,'reference.rgba');bandPath=path.join(control,'reference.edge');reuseProof=equal(input,controlPlan.snapshot.path);cache.set(key,{input,referencePath,bandPath});plan.reuse='Qualifiedcontrolworststress10exactinput/request';
   }else if(cache.has(key)){
    const known=cache.get(key);reuseProof=equal(input,known.input);referencePath=known.referencePath;bandPath=known.bandPath;plan.reuse='Completeobjects/background/manifest/digest+cameraequal';
   }else{
    const id='reference-'+String(invocations.length).padStart(3,'0'),directory=path.join(refs,id);await fs.mkdir(directory);
    const requestFile=path.join(directory,'request.json'),prefix=path.join(directory,'reference');await fs.writeFile(requestFile,JSON.stringify(request));await fs.writeFile(path.join(directory,'plan.json'),JSON.stringify(plan,null,2)+'\n');
    const args=[guard,'--prefix',path.join(directory,'guard'),'--timeout','60','--rss-mib','4096','--',oracle,'--pixels',input,requestFile,prefix];
    let error;try{execFileSync('python3',args,{encoding:'utf8',timeout:70000,maxBuffer:1024*1024});}catch(e){error=String(e);}
    const observed=JSON.parse(await fs.readFile(path.join(directory,'guard.worker.json'),'utf8'));
    const invocation={id,plan,guard:observed,status:observed.reason==='completed'&&observed.cleanup_complete?'REFERENCE_RENDERED':'FAILED',error};invocations.push(invocation);
    await fs.writeFile(path.join(directory,'invocation.json'),JSON.stringify(invocation,null,2)+'\n');
    if(invocation.status!=='REFERENCE_RENDERED'){proofs.push({stage,status:'MISSING_REFERENCE',invocation});break;}
    const output=JSON.parse(await fs.readFile(path.join(directory,'guard.stdout.log'),'utf8'));if(output.status!=='REFERENCE_RENDERED')throw Error('oracle output invalid');
    referencePath=prefix+'.rgba';bandPath=prefix+'.edge';cache.set(key,{input,referencePath,bandPath});
   }
   const reference=await fs.readFile(referencePath),band=await fs.readFile(bandPath);let a=actual,b=reference,e=band,roi=null;
   if(mixed){roi={x:0,y:0,width:1600,height:800};a=new Uint8Array(roi.width*roi.height*4);b=new Uint8Array(a.length);e=new Uint8Array(roi.width*roi.height);
    for(let y=0;y<roi.height;++y){a.set(actual.subarray(y*request.width*4,y*request.width*4+roi.width*4),y*roi.width*4);b.set(reference.subarray(y*request.width*4,y*request.width*4+roi.width*4),y*roi.width*4);e.set(band.subarray(y*request.width,y*request.width+roi.width),y*roi.width);}}
   const comparison=comparePixels(a,b,e);const proof={stage,...comparison,roi,plan,reuseProof,reference:{path:referencePath,sha256:sha(reference)},band:{path:bandPath,sha256:sha(band)},actual_sha256:sha(actual)};proofs.push(proof);
   await fs.writeFile(path.join(directory,stage+'-proof.json'),JSON.stringify(proof,null,2)+'\n');
   if(proof.status==='FAILED'){
    rows.push({source:source.label,window:path.basename(source.base),case:name,status:'FROZEN_INDEPENDENT_PIXEL_MISMATCH',proofs});await fs.writeFile(path.join(result,'independent-pixel-proof.json'),JSON.stringify({status:'FROZEN_INDEPENDENT_PIXEL_MISMATCH',rows},null,2)+'\n');throw Error('FROZEN_INDEPENDENT_PIXEL_MISMATCH');
   }
  }
  const observation={source:source.label,window:path.basename(source.base),case:name,status:proofs.length===2&&proofs.every(v=>v.status==='QUALIFIED')?'QUALIFIED':'MISSING_REFERENCE',proofs};rows.push(observation);await fs.writeFile(path.join(directory,'row-proof.json'),JSON.stringify(observation,null,2)+'\n');
  await fs.writeFile(path.join(result,'oracle-invocations.json'),JSON.stringify({status:'IN_PROGRESS',unique:invocations.length,invocations},null,2)+'\n');console.log(JSON.stringify({source:source.label,case:name,status:observation.status,unique:invocations.length}));
 }
 const status=rows.every(v=>v.status==='QUALIFIED')?'QUALIFIED_AVAILABLE_FINAL_REDO':'BOUNDED_REQUIRED_PROOF_INCOMPLETE';
 await fs.writeFile(path.join(result,'independent-pixel-proof.json'),JSON.stringify({status,rows,old_missing_records_unchanged:true,original_AA_comparator_unchanged:true,required_r4_rows:21,completed_r4_rows:rows.filter(v=>v.status==='QUALIFIED').length},null,2)+'\n');
 await fs.writeFile(path.join(result,'oracle-invocations.json'),JSON.stringify({status:'COMPLETE',unique:invocations.length,invocations,qualified_control_reference_reused:true,parallelism:1},null,2)+'\n');
 return status;
}
if(process.argv[1]===new URL(import.meta.url).pathname)await completePixels(process.cwd());
