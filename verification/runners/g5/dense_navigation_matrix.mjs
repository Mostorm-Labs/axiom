// Dense, in-view mixed content. Uses installed root SDKs and isolated browsers.
// Each worker owns/bounds only its own process family. Never attaches to4218.
import {spawnSync} from 'node:child_process';
import fs from 'node:fs/promises';
import path from 'node:path';
const [before,after,output,mode='on']=process.argv.slice(2);
if(!before||!after||!output||!['on','off'].includes(mode))throw Error('BEFORE AFTER OUTPUT on|off');
const cases=[
  {id:'dense-pan',selected:false,pan:true,idle:1000},
  {id:'dense-selected-pan',selected:true,pan:true,idle:1000},
  {id:'dense-zoom-pan',selected:false,pan:false,idle:1000},
  {id:'dense-selected-zoom-pan',selected:true,pan:false,idle:60000}
];
const options={width:636,height:923,points:2048,lines:5,shapes:2,
  scribble:true,scribblePhase:.63,zoom:1.59,drawLast:true,uiReads:true};
const results=[];await fs.mkdir(output,{recursive:true});
for(const fixture of cases)for(const [side,sdk] of [['before',before],['after',after]]){
  const dest=path.resolve(output,fixture.id+'-'+side);
  const env={...process.env,AXIOM_NAV_MODES:'wheel',AXIOM_NAV_SELECTED:fixture.selected?'1':'0',
    AXIOM_NAV_PAN_ONLY:fixture.pan?'1':'0',AXIOM_NAV_QUIESCE:'1',AXIOM_NAV_OPTIONS:JSON.stringify(options)};
  const run=spawnSync('python3.12',['verification/runners/g5/bounded_worker.py','--prefix',dest+'-worker',
    '--timeout','180','--rss-mib','3072','--',process.execPath,'verification/runners/g5/navigation_lifecycle.mjs',
    path.resolve(sdk),dest,String(fixture.idle),'2'],{env,encoding:'utf8',maxBuffer:4*1024*1024});
  let summary;try{summary=JSON.parse(await fs.readFile(path.join(dest,'summary.json'),'utf8'));}catch{}
  const row={case:fixture.id,side,mode,exit:run.status,status:summary?.status??'BLOCKED_EVIDENCE',
    setup:summary?.results?.[0]?.setup,quiescence:summary?.results?.[0]?.setupQuiescence,
    phases:summary?.results?.[0]?.phases,artifacts:summary?.artifacts,sourceBefore:summary?.sourceBefore,
    error:run.error?.message};
  results.push(row);console.log(JSON.stringify({case:fixture.id,side,status:row.status,
    gaps:row.phases?.map(p=>[p.id,p.summary.input_display_submission_gap_ms.max])}));
  await fs.writeFile(path.join(output,'matrix.json'),JSON.stringify({schema:'g5-dense-navigation-v1',options,results},null,2));
}
const comparisons=cases.map(f=>{
  const rows=results.filter(r=>r.case===f.id),a=rows.find(r=>r.side==='before'),b=rows.find(r=>r.side==='after');
  return {case:f.id,measured:rows.length===2&&rows.every(r=>r.status==='MEASURED'),phases:a?.phases?.map((p,i)=>({
    id:p.id,beforeGapMs:p.summary.input_display_submission_gap_ms.max,
    afterGapMs:b?.phases?.[i]?.summary.input_display_submission_gap_ms.max,
    sameFinalPixels:p.pixels?.sha256===b?.phases?.[i]?.pixels?.sha256,
    beforeQuality:p.summary.navigation?.quality,afterQuality:b?.phases?.[i]?.summary.navigation?.quality}))};
});
await fs.writeFile(path.join(output,'comparison.json'),JSON.stringify(comparisons,null,2));
if(results.some(r=>r.status!=='MEASURED')||comparisons.some(c=>!c.measured||c.phases?.some(p=>!p.sameFinalPixels)))process.exitCode=1;
