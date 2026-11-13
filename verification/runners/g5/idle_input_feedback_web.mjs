// Finite ordinary root-SDK draw/drag/foreground-idle diagnosis. No user-page attach.
import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';import crypto from 'node:crypto';import {gzipSync} from 'node:zlib';
import {chromium} from '../../node_modules/playwright/index.mjs';
import {installIdleObserver,summarize,qualifyIdle} from './idle_input_feedback_observer.mjs';
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
export async function run(q,out){
 await fs.mkdir(out,{recursive:true});let browser,page,server;const row={id:q.id,status:'STARTED',stages:[],pixels:[],sequences:[],errors:[],started_unix:Date.now()/1000};
 const write=async()=>fs.writeFile(path.join(out,'raw.json'),JSON.stringify(row,null,2)+'\n');
 const stage=async(name,extra={})=>{const v={stage:name,unix:Date.now()/1000,...extra};row.stages.push(v);console.log(JSON.stringify(v));await write();};
 const check=(v,m)=>{if(!v)throw Error(m);};const pause=ms=>new Promise(r=>setTimeout(r,ms));
 try{
  server=http.createServer(async(req,res)=>{try{const name=new URL(req.url,'http://local').pathname;let file;
   if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<!doctype html><title>Axiom isolated idle diagnosis</title><style>body{margin:0;background:white}#mount{position:relative;width:822px;height:900px}</style><div id="mount"></div>');return;}
   if(name==='/sdk/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end("import factory from '/engine/internal/axiom.mjs';export default async options=>{const m=await factory(options);window.idleObservation.engine(m);window.ifModule=m;return m;};");return;}
   if(name==='/fixture.axsnap')file=q.fixture.path;
   else if(name.startsWith('/engine/'))file=path.join(q.sdk,name.slice(8));
   else if(name.startsWith('/sdk/'))file=path.join(q.sdk,name.slice(5));
   else if(name.startsWith('/g5/'))file=path.join(process.cwd(),'verification/runners/g5',name.slice(4));
   if(!file)throw Error('unknown route');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
  }catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));row.url='http://127.0.0.1:'+server.address().port+'/';
  browser=await chromium.launch({headless:false,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',args:['--no-first-run']});
  page=await browser.newPage({viewport:{width:822,height:900},deviceScaleFactor:1});
  page.on('pageerror',e=>row.errors.push({host_unix:Date.now()/1000,text:String(e)}));
  await page.addInitScript(installIdleObserver);await page.goto(row.url);await page.bringToFront();
  await page.evaluate(async q=>{
   const {createRuntime}=await import('/sdk/axiom.mjs');const f=await(await fetch('/fixture.axsnap')).arrayBuffer();
   window.ifData={states:[],frames:[],operations:[],errors:[],buffers:[],snapshots:{},action:null};
   const d=window.ifData;const state=()=>window.ifSDK.readState();
   window.ifSDK=await createRuntime({mount:document.getElementById('mount'),documentId:q.fixture.documentId,restore:{bytes:new Uint8Array(f),metadata:q.fixture.metadata},builtinFonts:false,permission:'localSingleUser',
    onError:e=>d.errors.push({at:performance.now(),code:e.code,text:String(e)}),
    onOperation:b=>{const i=d.buffers.length;d.buffers.push(b);d.operations.push({at:performance.now(),bytes:b.length,index:i,state:state()});},
    onFrame:v=>{if(d.frames.length<8192)d.frames.push({at:performance.now(),value:v});}});
   window.ifState=label=>{const v={label,at:performance.now(),state:state(),observation:window.idleObservation.summary()};d.states.push(v);return v;};
  },q);
  const observe=label=>page.evaluate(label=>window.ifState(label),label);
  const settled=async(label,limit=45000)=>{
   const began=Date.now();let quiet=0,last=null;
   for(;;){const v=await observe(label),s=v.state,now=v.observation;
    if(!s.activeContacts&&!s.pending?.intents?.length&&s.generation===s.lastCommit.presentedGeneration&&last&&now.rafRequested===last.rafRequested&&now.rafCompleted===last.rafCompleted)++quiet;else quiet=0;
    if(quiet>=2)return v;check(Date.now()-began<limit,'settle timeout '+label);last=now;await pause(260);
   }
  };
  const pixel=async(label,rect)=>{
   const captures=await page.evaluate(async({rect})=>{
    const {readWebGLTarget}=await import('/g5/webgl_target_readback.mjs');
    const canonical=window.ifSDK.canvas,preview=canonical.parentElement.querySelector('canvas[id$="-preview"]');
    return [canonical,preview].map((canvas,i)=>{const r=readWebGLTarget(canvas,{role:i?'preview':'canonical',rect});let base64=null;if(r.bytes){let s='';for(let off=0;off<r.bytes.length;off+=8192)s+=String.fromCharCode(...r.bytes.subarray(off,off+8192));base64=btoa(s);}return{metadata:r.metadata,base64,visibility:canvas.style.visibility};});
   },{rect});
   const value={label,at:await page.evaluate(()=>performance.now()),captures:[]};
   for(const c of captures){const b=c.base64?Buffer.from(c.base64,'base64'):null;const filename=label+'-'+c.metadata.role+'.rgba.gz';if(b)await fs.writeFile(path.join(out,filename),gzipSync(b));value.captures.push({metadata:c.metadata,visibility:c.visibility,bytes:b?.length??0,sha256:b?sha(b):null,file:b?filename:null});check(c.metadata.status==='VALID','pixel unavailable '+label+' '+c.metadata.role);}
   row.pixels.push(value);return value;
  };
  const shot=async label=>{await page.screenshot({path:path.join(out,label+'.png')});};
  const capture=async label=>{
   const saved=await page.evaluate(async({label,metadata})=>{const s=window.ifSDK.readState();const r=await window.ifSDK.captureSnapshot({generation:s.generation,documentRevision:(BigInt(metadata.documentRevision)+BigInt(s.generation)).toString(),recoveryFrontier:metadata.recoveryFrontier});const d={state:s,generation:r.generation,checkpointDigest:r.checkpointDigest,throughCommit:r.throughCommit,bytes:r.bytes.length};let str='';for(let i=0;i<r.bytes.length;i+=8192)str+=String.fromCharCode(...r.bytes.subarray(i,i+8192));window.ifData.snapshots[label]=d;return{...d,base64:btoa(str)};},{label,metadata:q.fixture.metadata});
   const b=Buffer.from(saved.base64,'base64');delete saved.base64;await fs.writeFile(path.join(out,label+'.axsnap.gz'),gzipSync(b));saved.sha256=sha(b);row[label]=saved;await pixel(label);await shot(label);await write();return saved;
  };
  const action=async(name,fn)=>{await page.evaluate(name=>{window.ifData.action=name;window.idleObservation.phase(name);},name);const before=await observe(name+'-before');const at=await page.evaluate(()=>performance.now());await fn();const after=await observe(name+'-after');return{name,at,before,after};};
  const pathStroke=async(index,label)=>{
   const points=q.actions.draw[index];const v=await action(label,async()=>{
    await page.mouse.move(...points[0]);await page.mouse.down();
    for(let i=1;i<points.length;++i){await pause(16);await page.mouse.move(...points[i]);if(i===16||i===32){const beforeRead=await page.evaluate(()=>performance.now());await pixel(label+'-move'+i,q.actions.drawROI);if(!row.feedbackOverhead)row.feedbackOverhead=[];row.feedbackOverhead.push({label,step:i,started:beforeRead,finished:await page.evaluate(()=>performance.now())});}}
    await page.mouse.up();
   });return v;
  };
  const sequence=async name=>{
   await stage(name+'-begin');const before=await observe(name+'-initial');
   check(before.state.tool===0,'ink tool frozen before draw');const A=await pathStroke(0,name+'-draw-A');
   // Immediate successor: no settling, capture or command between first Up and second Down.
   const B=await pathStroke(1,name+'-draw-B');
   const drawSettled=await settled(name+'-draw-settled');check(drawSettled.state.objects===before.state.objects+2,'two strokes must commit');
   await pixel(name+'-draw-settled',q.actions.drawROI);
   await page.evaluate(()=>window.ifSDK.selectTool('select'));await page.evaluate(()=>window.ifSDK.setSelectionMode('point'));await settled(name+'-select-tool');
   const select=name==='post-idle'&&row.sequences.length?[q.actions.selection[0]+row.sequences[0].actual_view_displacement[0],q.actions.selection[1]+row.sequences[0].actual_view_displacement[1]]:q.actions.selection;await page.mouse.click(...select);await pause(300);
   const selected=await page.evaluate(()=>window.ifSDK.readSelection());check(selected.count===1&&q.fixture.objectIds.includes(selected.primary),'known original stroke selected by root/ID');
   const drag=await action(name+'-drag',async()=>{await page.mouse.move(...select);await page.mouse.down();for(let i=1;i<=32;++i){await pause(16);await page.mouse.move(select[0]+80*i/32,select[1]+30*i/32);if(i===16||i===32){await pixel(name+'-drag-move'+i,q.actions.dragROI);(row.dragWhileDown??=[]).push(await observe(name+'-drag-move'+i));}}await page.mouse.up();});
   const dragSettled=await settled(name+'-drag-settled');const afterSelection=await page.evaluate(()=>window.ifSDK.readSelection());
   check(afterSelection.primary===selected.primary,'same object retained through drag');check(BigInt(dragSettled.state.generation)>BigInt(drawSettled.state.generation),'drag must commit transform');
   await pixel(name+'-drag-settled',q.actions.dragROI);await shot(name+'-settled');
   await page.evaluate(()=>window.ifSDK.selectTool('ink'));await settled(name+'-ink-ready');
   const actual_view_displacement=[(afterSelection.transform[4]-selected.transform[4])*row.camera.scale,(afterSelection.transform[5]-selected.transform[5])*row.camera.scale];
   check(Math.abs(actual_view_displacement[0]-80)<8&&Math.abs(actual_view_displacement[1]-30)<8,'drag displacement differs beyond existing snap neighborhood');
   const result={name,before,A,B,drawSettled,selected,drag,dragSettled,afterSelection,requested_view_displacement:[80,30],actual_view_displacement,snap_note:'default selection snapping retained; requested and actual positions recorded, not claimed exact raw pointer transform'};row.sequences.push(result);await write();return result;
  };
  await settled('restore');row.build=await page.evaluate(()=>window.ifSDK.readBuildInfo());check((await observe('restored')).state.objects===40,'original Fixed40 restore');
  await page.evaluate(async c=>{await window.ifSDK.zoomAt(0,0,.34);const s=window.ifSDK.readState();await window.ifSDK.panBy(s.camera.x-c.x,s.camera.y-c.y);},q.camera);
  const camera=await settled('camera');row.camera=camera.state.camera;check(Math.abs(row.camera.scale-.34)<1e-8,'camera scale');
  await page.evaluate(async()=>{await window.ifSDK.selectBrush('round-handdrawn',4);await window.ifSDK.selectTool('ink');});await settled('brush-ready');row.brush=await page.evaluate(()=>window.ifSDK.readBrushConfig());check(row.brush?.family===1,'default round brush');await capture('initial');
  await sequence('warm');await capture('pre-idle');
  await settled('ready-idle');
  if(q.idle_ms){
   const before=await page.evaluate(()=>{window.idleObservation.phase('real-idle');return{state:window.ifSDK.readState(),observation:window.idleObservation.summary()};});
   row.idle={before,duration_ms:q.idle_ms,observer_policy:'No page evaluation, input, timers, state polling, snapshots, readback, RAF or prewarm from host during idle. Passive event wrappers only. Host waits.'};await stage('idle-start',{duration_ms:q.idle_ms,before});
   // One host timer; no browser evaluation until the real interval has elapsed.
   await pause(q.idle_ms);
   const after=await page.evaluate(()=>{const d=window.idleObservation.read();return{state:window.ifSDK.readState(),observation:d.summary,events:d.events};});
   row.idle.after=after;row.idle.qualification=qualifyIdle(before.observation,after.observation,after.events.values,q.idle_ms);
   check(after.state.documentId===before.state.documentId&&after.state.generation===before.state.generation&&after.state.objects===before.state.objects,'same canonical state throughout idle');
   await stage('idle-end',{qualification:row.idle.qualification});
   // Boundary reads above do not call frame/paint/resize. First input is draw.
   await sequence('post-idle');await capture('final');
  }else await capture('final');
  row.pageData=await page.evaluate(()=>{const d=window.ifData;return{...d,buffers:undefined,observation:window.idleObservation.read()};});
  for(const seq of row.sequences){const p=row.pageData.observation.terminals.values.filter(v=>v.command==='pointer');const up=p.find(v=>v.phase===seq.name+'-draw-A'&&v.request.phase===2);const down=p.find(v=>v.phase===seq.name+'-draw-B'&&v.request.phase===0);seq.up_to_next_down_ms=up&&down?down.at-up.end:null;seq.successor_policy='host mouse with no settle; includes recorded host/page boundary observations, not same DOM task';}
  for(let i=0;i<row.pageData.operations.length;++i){const base64=await page.evaluate(i=>{const b=window.ifData.buffers[i];let s='';for(let n=0;n<b.length;n+=8192)s+=String.fromCharCode(...b.subarray(n,n+8192));return btoa(s);},i);const b=Buffer.from(base64,'base64');await fs.writeFile(path.join(out,'operation-'+i+'.bin.gz'),gzipSync(b));row.pageData.operations[i].sha256=sha(b);}
  check(row.pageData.errors.length===0,'root SDK error');check(row.errors.length===0,'page exception');
  row.activeRAF=summarize(row.pageData.observation.frames.values.filter(x=>!['setup','real-idle'].includes(x.phase)).map(x=>x.end-x.at));
  row.status=q.idle_ms&&!row.idle.qualification.qualified?'OBSERVED_QUALIFICATION_LIMIT':'OBSERVED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{if(page){row.failure=await page.evaluate(()=>({state:window.ifSDK?.readState(),data:{...window.ifData,buffers:undefined},observation:window.idleObservation?.read()}));await page.screenshot({path:path.join(out,'failure.png')});}}catch(e){row.failureCaptureError=String(e);}}
 finally{
  if(page)try{await page.evaluate(async()=>{if(window.ifSDK)await window.ifSDK.close();});row.runtime_closed=true;}catch(e){row.closeError=String(e);}
  if(page)await page.close().catch(()=>{});if(browser)await browser.close().catch(()=>{});if(server)await new Promise(r=>server.close(r));row.ended_unix=Date.now()/1000;await write();console.log(JSON.stringify({stage:'closed',status:row.status,error:row.error}));
 }
 return row;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const q=JSON.parse(await fs.readFile(process.argv[2],'utf8'));const row=await run(q,process.argv[3]);if(row.status==='FAILED')process.exitCode=1;}
