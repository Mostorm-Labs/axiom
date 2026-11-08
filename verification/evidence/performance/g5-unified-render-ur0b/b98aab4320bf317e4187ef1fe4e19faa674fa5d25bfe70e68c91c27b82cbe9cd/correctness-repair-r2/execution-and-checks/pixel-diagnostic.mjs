// File-only HUD recomparison. No browser/native invocation; no archive rewrite.
import fs from 'node:fs';import path from 'node:path';import {createHash} from 'node:crypto';import assert from 'node:assert/strict';
import {comparePixels,publicHudBand} from '../../../../verification/runners/g5/ur0/pixel_oracle.mjs';
import {fixedLiveExpectation,selectionHudRequest} from '../../../../verification/runners/g5/ur0/correctness_contract.mjs';
import * as old from '../../../../out/ur0b/correctness-repair-r2/baseline/verification/runners/g5/ur0/pixel_oracle.mjs';
const root=process.cwd(),out=path.join(root,'.aegis/results/G5-UR0b/correctness-repair-r2');
const source=path.join(root,'verification/evidence/performance/g5-unified-render-ur0b/d33166e423e7558bf4d18b87108425b2cb2553ab6ff15271fd4752a0c200d73a/resume-r4/execution-and-checks/execution/raw-runs/correctness-lower_z_translucent_drag');
const load=n=>JSON.parse(fs.readFileSync(path.join(source,n))),hash=b=>createHash('sha256').update(b).digest('hex');
const rows=[];
for(const step of [0,1]){
 const label='off-'+step,row=load(label+'.correctness.json'),before=load(label+'-before.json'),after=load(label+'-after.json');
 // Build the mask entirely from the fixed fixture and public state before
 // inspecting the candidate. OFF0 uses the independent snapped expectation.
 const request={...row.request,...selectionHudRequest({selection:step?after.selection:before.selection,camera:row.request.camera,caseId:row.case_id,step,before})};
 const hud=publicHudBand(request),actual=fs.readFileSync(path.join(source,label+'.candidate.rgba')),reference=fs.readFileSync(path.join(source,label+'.reference.rgba')),edge=fs.readFileSync(path.join(source,label+'.reference.edge'));
 const original=old.comparePixels(actual,reference,edge,old.publicHudBand(row.request));assert.equal(original.interior_mismatched_pixels,step?16112:2463);assert.equal(original.status,'FAILED');
 assert.equal(hash(actual),row.candidate_sha256);assert.equal(hash(fs.readFileSync(path.join(source,after.snapshot.path))),after.snapshot.sha256);
 const compared=comparePixels(actual,reference,edge,hud),coordinates=[];
 for(let pixel=0;pixel<edge.length;++pixel){if(hud[pixel]||edge[pixel])continue;let delta=0;for(let c=0;c<4;++c)delta=Math.max(delta,Math.abs(actual[pixel*4+c]-reference[pixel*4+c]));if(delta>2)coordinates.push({pixel,x:pixel%request.width,y:Math.floor(pixel/request.width),delta});}
 assert.equal(coordinates.length,compared.interior_mismatched_pixels);
 if(step)assert.equal(compared.interior_mismatched_pixels,0);
 const coordinateFile=label+'.residual-coordinates.json';fs.writeFileSync(path.join(out,coordinateFile),JSON.stringify(coordinates,null,2)+'\n');
 const inputs=[label+'.correctness.json',label+'-before.json',label+'-after.json',label+'.candidate.rgba',label+'.reference.rgba',label+'.reference.edge',after.snapshot.path].map(n=>({path:path.relative(root,path.join(source,n)),sha256:hash(fs.readFileSync(path.join(source,n)))}));
 rows.push({step,analysis:'OFFLINE_DIAGNOSTIC_NOT_FRESH_OR_PAIRED',original,new_request:request,comparison:compared,expected_transform:step?undefined:fixedLiveExpectation(before).transform,residual_coordinates:{path:path.relative(root,path.join(out,coordinateFile)),sha256:hash(fs.readFileSync(path.join(out,coordinateFile))),count:coordinates.length},inputs,original_verdict_preserved:load(label+'.correctness.json').comparison.status==='FAILED'});
}
const report={status:'OFFLINE_HUD_DIAGNOSTIC_COMPLETE',rows,browser_runs:0,native_reference_reruns:0,original_live_public_capture:'MISSING_IN_ORIGINAL_ARCHIVE; no retrospective full live-subject qualification',AA_and_channel_thresholds:'UNCHANGED',candidate_independent_HUD:true};
fs.writeFileSync(path.join(out,'pixel-diagnostic.json'),JSON.stringify(report,null,2)+'\n');console.log(JSON.stringify(rows.map(r=>({step:r.step,old:r.original.interior_mismatched_pixels,offline_new:r.comparison.interior_mismatched_pixels,status:r.comparison.status}))));
