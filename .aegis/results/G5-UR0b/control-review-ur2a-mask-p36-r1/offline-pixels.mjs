import fs from 'node:fs';
import path from 'node:path';
import assert from 'node:assert/strict';
import {createHash} from 'node:crypto';
const root=process.cwd(),base=path.join(root,'.aegis/results/G5-UR2a-mask-isolation-p36-r1/local-functional');
const json=p=>JSON.parse(fs.readFileSync(p));
const sha=b=>createHash('sha256').update(b).digest('hex');
const checked=(p,h)=>{const b=fs.readFileSync(p);assert.equal(sha(b),h,p);return b;};
const originalOracle='ab7a1a3ce61076eb34c1bccb7c59a61f09e5a51323c45403796ad2eab1440105';
checked(path.join(root,'out/G5-UR1c1-prepared-body-index/build-native/runtime/render/axiom_ur0_fixture_oracle'),originalOracle);
const observations=[],roundtrips=[];
for(const worker of ['UR2AMASKR1-Q1','UR2AMASKR1-N1']){
 const dir=path.join(base,worker);
 for(const file of fs.readdirSync(dir).filter(n=>n.endsWith('-cold.json')).sort()){
  const id=file.slice(0,-10),stages=json(path.join(dir,file)),actuals=new Map();
  for(const stage of stages){
   const prefix=path.join(dir,`${id}-${stage.stage}`),s=stage.surfaces.canonical;
   const actual=checked(prefix+'.canonical.rgba',s.sha256),preview=checked(prefix+'.preview.rgba',stage.surfaces.preview.sha256);
   const snapshot=checked(prefix+'.axsnap',stage.snapshot.sha256);
   const expected=fs.readFileSync(prefix+'-reference.rgba'),edge=fs.readFileSync(prefix+'-reference.edge');
   assert.deepEqual(json(prefix+'-reference.request.json'),{width:s.width,height:s.height,logical_width:800,logical_height:500,camera:stage.camera,hud_bounds:[]});
   const oracle=stage.oracle,inputs=oracle.reference_inputs;
   if(inputs){
    assert.equal(inputs.oracle.sha256,originalOracle);checked(inputs.oracle.path,originalOracle);
    assert(snapshot.equals(checked(inputs.snapshot.path,inputs.snapshot.sha256)));
    assert(expected.equals(checked(inputs.rgba.path,inputs.rgba.sha256)));
    assert(edge.equals(checked(inputs.edge.path,inputs.edge.sha256)));
    for(const r of inputs.resources)checked(r.path,r.sha256);
   }else{
    assert.equal(oracle.reference_origin,'NEW_ORACLE_ONCE');assert.equal(oracle.reference_snapshot,prefix+'.axsnap');
   }
   assert.equal(actual.length,expected.length);assert.equal(edge.length*4,actual.length);
   let interior=0,boundary=0,mismatches=0,maxDelta=0,whiteTransparent=0,flattenedMismatches=0,nonOpaque=0;
   const bbox=[s.width,s.height,-1,-1],colors=new Map();
   for(let p=0;p<edge.length;p++){
    const at=p*4;if(actual[at+3]<255)nonOpaque++;
    assert(edge[p]===0||edge[p]===1);if(edge[p]){boundary++;continue;}interior++;
    let delta=0,flatDelta=0;
    for(let c=0;c<4;c++){
     delta=Math.max(delta,Math.abs(actual[at+c]-expected[at+c]));
     const flat=c===3?255:Math.round(actual[at+c]*actual[at+3]/255+255-actual[at+3]);
     flatDelta=Math.max(flatDelta,Math.abs(flat-expected[at+c]));
    }
    if(flatDelta>2)flattenedMismatches++;maxDelta=Math.max(maxDelta,delta);
    if(delta<=2)continue;mismatches++;
    const x=p%s.width,y=Math.floor(p/s.width);bbox[0]=Math.min(bbox[0],x);bbox[1]=Math.min(bbox[1],y);bbox[2]=Math.max(bbox[2],x);bbox[3]=Math.max(bbox[3],y);
    const a=[...actual.subarray(at,at+4)],e=[...expected.subarray(at,at+4)];
    const key=`${a}->${e}`;colors.set(key,(colors.get(key)||0)+1);
    if(a.every(x=>x===0)&&e.every(x=>x===255))whiteTransparent++;
   }
   assert.equal(oracle.interior_mismatched_pixels,mismatches);assert.equal(oracle.max_interior_delta,maxDelta);
   const row={worker,case:id,stage:stage.stage,actual_sha256:sha(actual),reference_sha256:sha(expected),edge_sha256:sha(edge),snapshot_sha256:sha(snapshot),status:mismatches?'FAILED':'QUALIFIED',interior,boundary,mismatches,maxDelta,nonOpaque,whiteTransparent,flattenedMismatches,bbox:mismatches?bbox:null,colors:[...colors].sort((a,b)=>b[1]-a[1]).slice(0,8),preview_visible:stage.surfaces.preview.visible,preview_nonzero_alpha:stage.surfaces.preview.nonzeroAlpha};
   observations.push(row);actuals.set(stage.stage,{actual,edge});
  }
  const a=actuals.get('after'),r=actuals.get('redo');let mismatch=0;
  for(let p=0;p<a.edge.length;p++)if(!a.edge[p]&&[0,1,2,3].some(c=>Math.abs(a.actual[p*4+c]-r.actual[p*4+c])>2))mismatch++;
  roundtrips.push({worker,case:id,after_redo_interior_mismatches:mismatch});
 }
}
assert.equal(observations.length,80);assert.equal(roundtrips.length,20);
const failed=observations.filter(x=>x.status==='FAILED');
const report={status:failed.length?'BLOCKED':'QUALIFIED',scope:'Independent offline re-comparison of all frozen buffers; alpha flattening is diagnostic only and never changes acceptance',browser_starts:0,oracle_invocations:0,original_oracle_sha256:originalOracle,cold_stages:80,qualified:80-failed.length,failed:failed.length,observations,roundtrips};
fs.writeFileSync(new URL('./offline-pixels.json',import.meta.url),JSON.stringify(report,null,2)+'\n');
console.log(JSON.stringify({status:report.status,cold_stages:80,qualified:report.qualified,failed:report.failed,failed_rows:failed.map(({worker,case:id,stage,mismatches,whiteTransparent,flattenedMismatches,bbox,preview_visible})=>({worker,case:id,stage,mismatches,whiteTransparent,flattenedMismatches,bbox,preview_visible})),roundtrips:roundtrips.length,roundtrip_failures:roundtrips.filter(x=>x.after_redo_interior_mismatches).length,browser_starts:0,oracle_invocations:0}));
