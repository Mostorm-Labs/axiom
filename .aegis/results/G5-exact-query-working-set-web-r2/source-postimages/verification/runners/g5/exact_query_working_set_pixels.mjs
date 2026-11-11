// Offline only: never launches a browser or changes the production/oracle.
import fs from'node:fs/promises';import path from'node:path';import{execFileSync}from'node:child_process';import{createHash}from'node:crypto';
import{comparePixels}from'./ur0/pixel_oracle.mjs';
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function qualifyStoredRows(root){
 const result=path.join(root,'.aegis/results/G5-exact-query-working-set-web-r2');
 const projector=path.join(root,'out/G5-exact-query-working-set-web-r2/offline-snapshot');
 const oracle=path.join(root,'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle');
 if(sha(await fs.readFile(oracle))!=='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105')throw Error('foreign frozen oracle');
 const rows=[],referenceCache=new Map();
 for(const window of['G5QW2-FINAL-PAIRS','G5QW2-CONTINUOUS']){
  const base=path.join(result,'local-web',window);
  for(const name of(await fs.readdir(base)).sort()){
   const directory=path.join(base,name);let row;try{row=JSON.parse(await fs.readFile(path.join(directory,'qualified-row.json'),'utf8'));}catch{continue;}
   const proofs=[];
   for(const stage of['final','redo']){
    const snapshot=path.join(directory,stage+'.axsnap'),actual=await fs.readFile(path.join(directory,stage+'.rgba'));
    const mixed=['witness','action'].includes(row.mode),projected=path.join(directory,'ink-'+stage+'.axsnap');
    let input=snapshot,projection=null;
    if(mixed){projection=JSON.parse(execFileSync(projector,['--project',snapshot,projected],{encoding:'utf8',timeout:10000}));input=projected;}
    const metadata=row[stage],camera=metadata.state.camera,width=metadata.readback.canvas.width,height=metadata.readback.canvas.height;
    const request={width,height,logical_width:1431,logical_height:1010,camera,hud_bounds:[]};
    const requestFile=path.join(directory,stage+'-independent.request.json');await fs.writeFile(requestFile,JSON.stringify(request));
    const bytes=await fs.readFile(input),prefix=path.join(directory,stage+'-independent');
    const key=sha(bytes)+'/'+JSON.stringify(request),invocation={snapshot_sha256:sha(bytes),original_snapshot_sha256:sha(await fs.readFile(snapshot)),request,projection,timeout_ms:10000,oracle_sha256:'ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105'};
    let reference,band;
    try{
     if(referenceCache.has(key)){const cached=referenceCache.get(key);reference=await fs.readFile(cached+'.rgba');band=await fs.readFile(cached+'.edge');invocation.reused_exact_reference=cached;}
     else{invocation.reference=JSON.parse(execFileSync(oracle,['--pixels',input,requestFile,prefix],{encoding:'utf8',timeout:10000,maxBuffer:1024*1024}));if(invocation.reference.status!=='REFERENCE_RENDERED')throw Error('reference absent');reference=await fs.readFile(prefix+'.rgba');band=await fs.readFile(prefix+'.edge');referenceCache.set(key,prefix);}
     invocation.status='REFERENCE_RENDERED';
    }catch(error){invocation.status='FAILED';invocation.error=String(error);await fs.writeFile(prefix+'-invocation.json',JSON.stringify(invocation,null,2)+'\n');proofs.push({stage,status:'MISSING_REFERENCE',invocation});break;}
    await fs.writeFile(prefix+'-invocation.json',JSON.stringify(invocation,null,2)+'\n');
    // Mixed-object kinds are validated separately by typed canonical source,
    // active pixels and snapshot semantics. Limit the ink comparison to a
    // fixed physical ROI whose world footprint cannot overlap these objects.
    let a=actual,b=reference,e=band,rect=null;
    if(mixed){rect={x:0,y:0,width:1600,height:800};a=new Uint8Array(rect.width*rect.height*4);b=new Uint8Array(a.length);e=new Uint8Array(rect.width*rect.height);
     for(let y=0;y<rect.height;++y){a.set(actual.subarray(y*width*4,y*width*4+rect.width*4),y*rect.width*4);b.set(reference.subarray(y*width*4,y*width*4+rect.width*4),y*rect.width*4);e.set(band.subarray(y*width,y*width+rect.width),y*rect.width);}}
    proofs.push({stage,...comparePixels(a,b,e),roi:rect,identity:{snapshot_sha256:sha(bytes),actual_sha256:sha(actual),reference_sha256:sha(reference),band_sha256:sha(band)},invocation_file:prefix+'-invocation.json'});
   }
   const status=proofs.length===2&&proofs.every(v=>v.status==='QUALIFIED')?'QUALIFIED':'MISSING_OR_FAILED';rows.push({window,case:name,status,proofs});
   await fs.writeFile(path.join(directory,'independent-pixel-proof.json'),JSON.stringify(rows.at(-1),null,2)+'\n');console.log(JSON.stringify({case:name,status}));
  }
 }
 const proof={status:rows.every(v=>v.status==='QUALIFIED')?'QUALIFIED_AVAILABLE_ROWS':'MISSING_OR_FAILED',rows,reference_invocations:referenceCache.size,missing:['action-zoom final/redo captures absent; no oracle invocation substituted'],rule:'Frozen CPU/per-primitive AA boundary; <=2 outside. Mixed pureink projection removes only nonink objects/resources in oracle-owned clone; fixed disjoint ROI plus typed nonink proof, no candidate-based band.'};
 await fs.writeFile(path.join(result,'independent-pixel-proof.json'),JSON.stringify(proof,null,2)+'\n');return proof;
}
if(process.argv[1]===new URL(import.meta.url).pathname)await qualifyStoredRows(process.cwd());
