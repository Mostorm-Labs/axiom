// Diagnostic file-only comparison. Original comparison remains FAILED.
import fs from 'node:fs/promises';import path from 'node:path';import assert from 'node:assert/strict';
import {comparePixels,publicHudBand} from '../../../../verification/runners/g5/ur0/pixel_oracle.mjs';
const root=process.cwd(),out=path.join(root,'.aegis/results/G5-UR0b/control-review-resume-r4');
const source=path.join(root,'out/ur0b/resume-r4/execution/raw-runs/correctness-lower_z_translucent_drag'),read=async n=>JSON.parse(await fs.readFile(path.join(source,n),'utf8'));const rows=[];
for(const step of [0,1]){
 const prefix='off-'+step,row=await read(prefix+'.correctness.json'),before=await read(prefix+'-before.json'),after=await read(prefix+'-after.json');
 const actual=new Uint8Array(await fs.readFile(path.join(source,prefix+'.candidate.rgba'))),reference=new Uint8Array(await fs.readFile(path.join(source,prefix+'.reference.rgba'))),edge=new Uint8Array(await fs.readFile(path.join(source,prefix+'.reference.edge')));
 const original=comparePixels(actual,reference,edge,publicHudBand(row.request));assert.deepEqual(original,Object.fromEntries(Object.keys(original).map(k=>[k,row.comparison[k]])));
 const camera=row.request.camera,convert=b=>[b[0]*camera.scale+camera.x,b[1]*camera.scale+camera.y,b[2]*camera.scale+camera.x,b[3]*camera.scale+camera.y];
 const fromPublicBounds=[convert(before.selection.bounds),convert(after.selection.bounds)];
 const diagnosticRequest={...row.request,hud_bounds:step===0?[...row.request.hud_bounds,...fromPublicBounds]:[convert(after.selection.bounds)]};
 const diagnostic=comparePixels(actual,reference,edge,publicHudBand(diagnosticRequest));
 const bands=publicHudBand(row.request),diagnosticBand=publicHudBand(diagnosticRequest);let exactSelectionChrome=0,insideAddedBand=0;let minX=Infinity,minY=Infinity,maxX=-1,maxY=-1;const perColor=new Map();
 for(let n=0;n<edge.length;n++){
  if(bands[n]||edge[n])continue;
  let delta=0;for(let c=0;c<4;c++)delta=Math.max(delta,Math.abs(actual[n*4+c]-reference[n*4+c]));if(delta<=2)continue;
  const rgba=Array.from(actual.subarray(n*4,n*4+4)),key=rgba.join(',');perColor.set(key,(perColor.get(key)??0)+1);
  if(key==='105,101,255,255')exactSelectionChrome++;
  if(diagnosticBand[n])insideAddedBand++;
  const x=n%row.request.width,y=Math.floor(n/row.request.width);minX=Math.min(x,minX);minY=Math.min(y,minY);maxX=Math.max(x,maxX);maxY=Math.max(y,maxY);
 }
 rows.push({step,original,publicState:{tool:after.state.tool,selection_count:after.selection.count,selection_bounds:after.selection.bounds,transform:after.selection.transform},original_hud_bounds:row.request.hud_bounds,diagnostic_hud_bounds:diagnosticRequest.hud_bounds,diagnostic_only_comparison:diagnostic,exact_known_selection_chrome_mismatches:exactSelectionChrome,old_mismatches_in_added_public_band:insideAddedBand,old_difference_bounds_pixels:[minX,minY,maxX,maxY],most_common_different_candidate_colors:[...perColor].sort((a,b)=>b[1]-a[1]).slice(0,10)});
}
const result={status:'FILE_ONLY_DIAGNOSTIC_ORIGINAL_VERDICTS_UNCHANGED',reference_oracle_rerun:false,browser_runs:0,thresholds_unchanged:true,mask_source:'Archived public before/after selection bounds only; no candidate-difference-derived masks',rows};await fs.writeFile(path.join(out,'pixel-diagnostic.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify(rows.map(r=>({step:r.step,original_differences:r.original.interior_mismatched_pixels,diagnostic_differences:r.diagnostic_only_comparison.interior_mismatched_pixels,diagnostic_status:r.diagnostic_only_comparison.status,chrome:r.exact_known_selection_chrome_mismatches,in_added_public_band:r.old_mismatches_in_added_public_band,rectangle:r.old_difference_bounds_pixels,colors:r.most_common_different_candidate_colors.slice(0,4)}))));
