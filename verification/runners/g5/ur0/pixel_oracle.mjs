// Cold, independent reference integration; never imported by the SDK/renderer.
import fs from 'node:fs/promises';import {execFileSync} from 'node:child_process';
export function comparePixels(actual,expected,boundary,hud=null){
 if(!actual.length||actual.length!==expected.length||actual.length!==boundary.length*4||hud&&hud.length!==boundary.length)throw Error('pixel artifact shape mismatch');
 if(boundary.some(v=>v!==0&&v!==1)||hud?.some(v=>v!==0&&v!==1))throw Error('invalid reference boundary');
 let compared=0,edge=0,excluded=0,differences=0,maximum=0;const witnesses=[];
 for(let p=0;p<boundary.length;++p){
  if(hud?.[p]){++excluded;continue;}if(boundary[p]){++edge;continue;}++compared;
  let delta=0;for(let c=0;c<4;++c)delta=Math.max(delta,Math.abs(actual[p*4+c]-expected[p*4+c]));
  maximum=Math.max(maximum,delta);if(delta>2){++differences;if(witnesses.length<8)witnesses.push({pixel:p,actual:Array.from(actual.subarray(p*4,p*4+4)),reference:Array.from(expected.subarray(p*4,p*4+4))});}
 }
 return{status:!compared||excluded>boundary.length/5?'MISSING':differences?'FAILED':'QUALIFIED',compared_pixels:compared,aa_boundary_pixels:edge,hud_excluded_pixels:excluded,interior_mismatched_pixels:differences,max_interior_delta:maximum,witnesses,rule:'<=2 channel levels outside independent CPU geometry AA boundary; HUD excluded only from public selection bounds'};
}
export function publicHudBand({width,height,logical_width,logical_height,hud_bounds=[]}){
 const band=new Uint8Array(width*height),sx=width/logical_width,sy=height/logical_height;
 for(const b of hud_bounds){if(!Array.isArray(b)||b.length!==4||b.some(v=>!Number.isFinite(v))||b[0]>b[2]||b[1]>b[3])throw Error('invalid public HUD bounds');
  const [l,t,r,z]=b;for(let y=0;y<height;++y)for(let x=0;x<width;++x){const a=x/sx,c=y/sy;
   if((a>=l-7&&a<=r+7&&Math.min(Math.abs(c-t),Math.abs(c-z))<=7)||(c>=t-7&&c<=z+7&&Math.min(Math.abs(a-l),Math.abs(a-r))<=7)||(Math.abs(a-(l+r)/2)<=7&&c>=t-34&&c<=t))band[y*width+x]=1;
  }
 }
 return band;
}
export async function compareSnapshotPixels({oracle,snapshot,request,actual,prefix}){
 await fs.writeFile(prefix+'.request.json',JSON.stringify(request));
 const reference=JSON.parse(execFileSync(oracle,['--pixels',snapshot,prefix+'.request.json',prefix],{encoding:'utf8',timeout:10000,maxBuffer:1024*1024}));
 if(reference.status!=='REFERENCE_RENDERED')throw Error('independent native reference unavailable');
 const result=comparePixels(actual,await fs.readFile(prefix+'.rgba'),await fs.readFile(prefix+'.edge'),publicHudBand(request));
 return{...result,reference,reference_snapshot:snapshot,candidate_geometry_source:'public root SDK WebGL2 framebuffer',readback:'correctness-only cold worker; not primary performance sampling'};
}
