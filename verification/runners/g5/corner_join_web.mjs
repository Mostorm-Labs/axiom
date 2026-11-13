// Finite root-SDK correctness window for the shared v4 corner repair.
import fs from'node:fs/promises';import path from'node:path';import http from'node:http';import crypto from'node:crypto';import{gzipSync}from'node:zlib';
import{chromium}from'../../node_modules/playwright/index.mjs';
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
export async function run(q,out){
 await fs.mkdir(out,{recursive:true});let server,browser,page;const row={id:q.id,status:'STARTED',states:[],pixels:[],errors:[],stages:[],input:[],operations:[],snapshots:{}};
 const check=(v,m)=>{if(!v)throw Error(m);};const pause=ms=>new Promise(r=>setTimeout(r,ms));const save=()=>fs.writeFile(path.join(out,'raw.json'),JSON.stringify(row,null,2)+'\n');
 const stage=async v=>{row.stages.push({stage:v,unix:Date.now()/1000});console.log(JSON.stringify(row.stages.at(-1)));await save();};
 try{
  server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://local').pathname;let f;
   if(n==='/'){res.setHeader('Content-Type','text/html');res.end('<title>Axiom corner join verification</title><style>body{margin:0}#mount{position:relative;width:900px;height:650px}</style><div id="mount"></div>');return;}
   if(n.startsWith('/sdk/'))f=path.join(q.sdk,n.slice(5));else if(n==='/readback.mjs')f=path.join(process.cwd(),'verification/runners/g5/live_erase_feedback_pixels.mjs');if(!f)throw Error('route');res.setHeader('Content-Type',f.endsWith('.mjs')?'text/javascript':f.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(f));
  }catch(e){res.statusCode=404;res.end(String(e));}});await new Promise(r=>server.listen(0,'127.0.0.1',r));row.url='http://127.0.0.1:'+server.address().port;
  browser=await chromium.launch({headless:false,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});
  page.on('pageerror',e=>row.errors.push(String(e)));await page.goto(row.url);
  const setup=async restore=>{
   await page.evaluate(async restore=>{const{createRuntime}=await import('/sdk/axiom.mjs');window.cj={operations:[],buffers:[],errors:[],frames:[]};window.sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'77777777777777777777777777777777',builtinFonts:false,permission:'localSingleUser',...(restore?{restore:{bytes:Uint8Array.from(atob(restore.base64),v=>v.charCodeAt(0)),metadata:restore.metadata}}:{}),onOperation:b=>{window.cj.buffers.push(b);window.cj.operations.push({at:performance.now(),state:window.sdk.readState()});},onError:e=>window.cj.errors.push({code:e.code,message:String(e),at:performance.now()}),onFrame:f=>window.cj.frames.push({at:performance.now(),frame:f})});},restore);
  };
  await setup(null);
  const state=async label=>{const s=await page.evaluate(()=>({at:performance.now(),state:window.sdk.readState(),selection:window.sdk.readSelection()}));row.states.push({label,...s});return s;};
  const stable=async label=>{const t=Date.now();for(;;){const s=await state(label);if(!s.state.activeContacts&&!s.state.pending?.intents?.length&&s.state.generation===s.state.lastCommit.presentedGeneration)return s;check(Date.now()-t<45000,'canonical not settled '+label);await pause(260);}};
  const pixels=async label=>{
   const a=await page.evaluate(async()=>{const{readWebGLTarget}=await import('/readback.mjs');const c=window.sdk.canvas;return[c,c.parentElement.querySelector('canvas[id$="-preview"]')].map((canvas,i)=>{const v=readWebGLTarget(canvas,{role:i?'preview':'canonical'});let b='';if(v.bytes)for(let x=0;x<v.bytes.length;x+=8192)b+=String.fromCharCode(...v.bytes.subarray(x,x+8192));return{metadata:v.metadata,visible:canvas.style.visibility,base64:btoa(b)};});});
   const values=[];for(const v of a){check(v.metadata.status==='VALID','pixel unavailable');const b=Buffer.from(v.base64,'base64');const name=label+'-'+v.metadata.role+'.rgba.gz';await fs.writeFile(path.join(out,name),gzipSync(b));values.push({metadata:v.metadata,visible:v.visible,file:name,sha256:sha(b)});}row.pixels.push({label,values});await page.screenshot({path:path.join(out,label+'.png')});await save();
  };
  const snapshot=async label=>{await stable(label+'-settle');const s=await page.evaluate(async()=>{const state=window.sdk.readState();const r=await window.sdk.captureSnapshot({generation:state.generation,documentRevision:(BigInt(state.generation)+1n).toString(),recoveryFrontier:[8,1,16,1,24,1]});let b='';for(let i=0;i<r.bytes.length;i+=8192)b+=String.fromCharCode(...r.bytes.subarray(i,i+8192));return{state,base64:btoa(b),metadata:{documentRevision:(BigInt(state.generation)+1n).toString(),recoveryFrontier:[8,1,16,1,24,1],checkpointDigest:r.checkpointDigest,resourceBinding:'1'}};});await fs.writeFile(path.join(out,label+'.axsnap.gz'),gzipSync(Buffer.from(s.base64,'base64')));row.snapshots[label]={...s,base64:undefined,sha256:sha(Buffer.from(s.base64,'base64'))};return s;};
  const draw=async(label,profile,points)=>{
   await page.evaluate(async({profile})=>{await window.sdk.selectBrush(profile,4);if(profile==='marker-compact-v1'){const c=window.sdk.readBrushConfig();await window.sdk.setBrushConfig({...c,markerAngle:.7,markerFlatness:.25});}await window.sdk.selectTool('ink');},{profile});await stable(label+'-tool');
   const before=await state(label+'-before');await page.mouse.move(...points[0]);await page.mouse.down();
   for(let i=1;i<points.length;++i){await pause(16);await page.mouse.move(...points[i]);row.input.push({label,move:i,x:points[i][0],y:points[i][1]});if(i===points.length-2){row[label+'WhileDown']=await state(label+'-whileDown');await pixels(label+'-preview');}}
   await page.mouse.up();const after=await stable(label+'-after');check(after.state.objects===before.state.objects+1,'new stroke not committed');await pixels(label+'-canonical');await snapshot(label);await stage(label+'-done');
  };
  row.build=await page.evaluate(()=>window.sdk.readBuildInfo());await stable('initial');await stage('round-start');await draw('round','round-handdrawn',q.round);
  await draw('marker','marker-compact-v1',q.marker);const full=await snapshot('full');
  await page.evaluate(async()=>{await window.sdk.selectTool('select');await window.sdk.setSelectionMode('point');});await page.mouse.click(...q.select);row.selection=await page.evaluate(()=>window.sdk.readSelection());check(row.selection.count===1,'corrected stroke selection missing');
  await page.evaluate(async()=>{await window.sdk.selectTool('partialEraser');await window.sdk.setEraser({mode:2,diameter:24});});const before=await state('erase-before');await page.mouse.move(...q.erase[0]);await page.mouse.down();for(let i=1;i<q.erase.length;++i){await pause(16);await page.mouse.move(...q.erase[i]);}await page.mouse.up();const erased=await stable('erase-after');check(BigInt(erased.state.generation)>BigInt(before.state.generation),'partial erase did not commit');await pixels('erased');await snapshot('erased');
  await page.evaluate(()=>window.sdk.undo());await stable('undo');await pixels('undo');await snapshot('undo');await page.evaluate(()=>window.sdk.redo());await stable('redo');await pixels('redo');const redo=await snapshot('redo');
  row.operations=await page.evaluate(()=>window.cj.operations);for(let i=0;i<row.operations.length;++i){const b=await page.evaluate(i=>{const bytes=window.cj.buffers[i];let s='';for(let at=0;at<bytes.length;at+=8192)s+=String.fromCharCode(...bytes.subarray(at,at+8192));return btoa(s);},i);await fs.writeFile(path.join(out,'operation-'+i+'.bin.gz'),gzipSync(Buffer.from(b,'base64')));row.operations[i].sha256=sha(Buffer.from(b,'base64'));}
  row.rootErrors=await page.evaluate(()=>window.cj.errors);check(row.rootErrors.length===0,'SDK root errors');await page.evaluate(()=>window.sdk.close());await setup(redo);await stable('restore');await pixels('restored');await snapshot('restored');
  row.restoredErrors=await page.evaluate(()=>window.cj.errors);check(row.restoredErrors.length===0&&row.errors.length===0,'restore/page error');row.status='OBSERVED';
 }catch(e){row.status='FAILED';row.error=String(e.stack??e);try{row.failure=await page.evaluate(()=>({state:window.sdk?.readState(),errors:window.cj?.errors}));await page.screenshot({path:path.join(out,'failure.png')});}catch{}}
 finally{if(page)try{await page.evaluate(()=>window.sdk?.close());}catch{}if(page)await page.close();if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await save();console.log(JSON.stringify({stage:'closed',status:row.status,error:row.error}));}return row;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const q=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await run(q,process.argv[3])).status!=='OBSERVED')process.exitCode=1;}
