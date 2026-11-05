// Paired root-consumer reproduction. No measurements while builds run.
// Usage: node ... BEFORE_SDK AFTER_SDK OUTPUT [on|off|optical]
import {spawnSync} from 'node:child_process';
import fs from 'node:fs/promises';import path from 'node:path';
const [before,after,output,mode='on']=process.argv.slice(2);
if(!before||!after||!output||!['on','off','optical'].includes(mode))throw Error('arguments');
const mixed={shapes:2,lines:3,points:8192,scribble:true,zoom:.57,drawZoom:.57,drawLast:true,
  brush:{thinning:.99,streamline:.13,smoothing:.61}};
const cases=[
  {id:'nine-selected',selected:true,pan:false,idle:60000,options:mixed},
  {id:'nine-unselected',selected:false,pan:false,idle:0,options:mixed},
  {id:'frontier-selected',selected:true,pan:true,idle:0,options:{...mixed,points:2048,scribble:false,span:2400,zoom:1,drawZoom:1}},
  {id:'frontier-unselected',selected:false,pan:true,idle:0,options:{...mixed,points:2048,scribble:false,span:2400,zoom:1,drawZoom:1}},
  {id:'six-user-combination',selected:false,pan:false,idle:0,options:{...mixed,lines:1}}
];
const results=[];
await fs.mkdir(output,{recursive:true});
for(const fixture of mode==='on'?cases:cases.slice(0,1))for(const [side,sdk] of [['before',before],['after',after]]){
  const dest=path.resolve(output,fixture.id+'-'+side),args=['verification/runners/g5/navigation_lifecycle.mjs',sdk,dest,
    String(mode==='on'?fixture.idle:0),'2'];
  const env={...process.env,AXIOM_NAV_MODES:'wheel',AXIOM_NAV_SELECTED:fixture.selected?'1':'0',
    AXIOM_NAV_PAN_ONLY:fixture.pan?'1':'0',AXIOM_NAV_ZOOM_DELTA:'2',AXIOM_NAV_QUALITY_EVERY:mode==='optical'?'6':'0',AXIOM_NAV_OPTIONS:JSON.stringify(fixture.options)};
  const started=new Date().toISOString();
  const run=spawnSync(process.execPath,args,{env,encoding:'utf8',maxBuffer:16*1024*1024});
  await fs.writeFile(dest+'.log',(run.stdout??'')+(run.stderr??''));
  let summary;try{summary=JSON.parse(await fs.readFile(path.join(dest,'summary.json'),'utf8'));}catch{}
  const result={case:fixture.id,side,mode,started,exit:run.status,status:summary?.status??'BLOCKED_EVIDENCE',
    artifactIdentity:summary?.artifacts,sourceStable:summary?.sourceStable,error:run.error?.message};
  results.push(result);console.log(JSON.stringify({case:result.case,side,mode,status:result.status,exit:result.exit}));
  await fs.writeFile(path.resolve(output,'matrix.json'),JSON.stringify({mode,results},null,2));
}
const comparisons=[];
for(const fixture of mode==='on'?cases:cases.slice(0,1)){
  try{
    const a=JSON.parse(await fs.readFile(path.resolve(output,fixture.id+'-before/summary.json'),'utf8'));
    const b=JSON.parse(await fs.readFile(path.resolve(output,fixture.id+'-after/summary.json'),'utf8'));
    const beforePhases=a.results[0].phases,afterPhases=b.results[0].phases;
    comparisons.push({case:fixture.id,initialPixelsEqual:a.results[0].setup.initialPixels.sha256===b.results[0].setup.initialPixels.sha256,
      objects:[a.results[0].setup.state.objects,b.results[0].setup.state.objects],
      selectedKinds:[a.results[0].setup.selectedKind,b.results[0].setup.selectedKind],
      phases:beforePhases.map((p,i)=>({id:p.id,pixelsEqual:p.pixels.sha256===afterPhases[i]?.pixels.sha256,
        before:p.summary,after:afterPhases[i]?.summary}))});
  }catch(error){comparisons.push({case:fixture.id,error:String(error)});}
}
await fs.writeFile(path.resolve(output,'comparison.json'),JSON.stringify({mode,comparisons},null,2));
if(results.some(r=>r.exit!==0||r.status!=='MEASURED')||comparisons.some(c=>c.error||!c.initialPixelsEqual||c.phases.some(p=>!p.pixelsEqual)))process.exitCode=1;
