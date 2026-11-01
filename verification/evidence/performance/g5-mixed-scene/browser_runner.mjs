import {chromium} from '../../../node_modules/playwright/index.mjs';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const output=path.resolve(process.argv[2]),sdk=path.resolve(process.argv[3]);
const count=Number(process.argv[4]||64),dpr=Number(process.argv[5]||2),reverse=process.argv[6]==='reverse';
const brushRevision=Number(process.argv[7]||4);
const scenario=process.argv[8]||'zoom';
const inputPackets=[];
await fs.mkdir(output,{recursive:true});
const settleFrames=Number(process.env.AXIOM_TIMING_SETTLE_FRAMES||300);
let html=await fs.readFile('verification/evidence/performance/g5-world-navigation/browser-final/browser.html','utf8');
html=html.replaceAll('i<300;',`i<${settleFrames};`).replaceAll('i===299',`i===${settleFrames-1}`).replace('builtinFonts:false','builtinFonts:true').replace('capacity:1024','capacity:4096');
html=html.replace('onError:e=>observation.errors.push(String(e)),','onOperation:bytes=>{window.g5LastOperation=Array.from(bytes);window.g5RecordOperation?.(Array.from(bytes));},onError:e=>observation.errors.push(String(e)),');
// Input end belongs to the dispatch task, not a later Playwright round trip.
// A qualifying callback can already arrive before stop() is invoked.
html=html.replace('observation.inputEnded=performance.now();','observation.inputEnded=window.g5MeasuredEnded??performance.now();');
html=html.replace("selectBrush('round-handdrawn',1)",`selectBrush('round-handdrawn',${brushRevision})`);
html=html.replace("import {createRuntime} from '/sdk/axiom.mjs';","import {createRuntime} from '/sdk/axiom.mjs'; let fixedUUID=1;crypto.randomUUID=()=>('00000000000040008000'+(fixedUUID++).toString(16).padStart(12,'0'));");
const server=http.createServer(async(req,res)=>{try{
 const u=new URL(req.url,'http://localhost').pathname;
 const file=u.startsWith('/sdk/')?path.resolve(sdk,u.slice(5)):null;
 if(file&&!file.startsWith(sdk+path.sep))throw Error('path');
 res.setHeader('Content-Type',file?(file.endsWith('.wasm')?'application/wasm':file.endsWith('.mjs')?'text/javascript':'application/octet-stream'):'text/html');
 res.end(file?await fs.readFile(file):html);
}catch{res.statusCode=404;res.end();}});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
try{
 const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:dpr});
 const lines=[],exceptions=[];
 await page.exposeFunction('g5RecordOperation',bytes=>{inputPackets.push(bytes);});
 await page.addInitScript(()=>{
  window.g5Fences=[];const proto=WebGL2RenderingContext.prototype;
  for(const name of ['fenceSync','clientWaitSync']){const raw=proto[name];proto[name]=function(...args){const start=performance.now();const result=raw.apply(this,args);if(window.observation?.active)g5Fences.push({name,start,duration:performance.now()-start,status:name==='clientWaitSync'?result:null});return result;};}
 });
 page.on('console',m=>{if(m.text().startsWith('AXIOM_FRAME_TIMING '))lines.push(m.text());});
 page.on('pageerror',e=>exceptions.push(String(e)));

 await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.ready);await page.evaluate(()=>ready);console.log('ready');
 await page.evaluate(()=>{runtime.canvas.setPointerCapture=()=>{};runtime.canvas.hasPointerCapture=()=>false;});
 for(let n=0;n<count;++n){
  await page.evaluate(n=>{for(let i=0;i<16;++i){const e=new PointerEvent(i===0?'pointerdown':i===15?'pointerup':'pointermove',{pointerId:1,pointerType:'mouse',clientX:120+i*15,clientY:90+(n%20)*14+Math.sin(i*.4)*5,button:i===0||i===15?0:-1,buttons:i===15?0:1,pressure:i===15?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:1000+n*100+i});runtime.canvas.dispatchEvent(e);}},n);
  if((n+1)%25===0||n+1===count)await page.evaluate(()=>settle());
 }
 await page.evaluate(async()=>{
   await runtime.selectShape('rectangle');const send=(type,x,y)=>runtime.canvas.dispatchEvent(new PointerEvent(type,{pointerId:2,pointerType:'mouse',clientX:x,clientY:y,buttons:type==='pointerup'?0:1,bubbles:true,cancelable:true}));
   send('pointerdown',500,320);send('pointermove',640,430);send('pointerup',640,430);await settle();
   await runtime.selectTool('text');await runtime.text.create(430,75,180);await runtime.text.replace('Mixed scene baseline');await runtime.text.finish();await settle();
   const c=document.createElement('canvas');c.width=8;c.height=8;const g=c.getContext('2d');g.fillStyle='#c96543';g.fillRect(0,0,8,8);
   const bytes=new Uint8Array(await(await new Promise(r=>c.toBlob(r,'image/png'))).arrayBuffer());await runtime.importImage(bytes,'image/png');await settle();
   await runtime.selectTool('ink');await settle();
 });
 for(const measuredScenario of (scenario==='suite'?['zoom','drag','ink']:[scenario])){
 const scenario=measuredScenario;
 const destination=process.argv[8]==='suite'?path.join(output,scenario):output;
 await fs.mkdir(destination,{recursive:true});
 if(scenario==='ink'){await page.evaluate(async()=>{await runtime.selectTool('ink');await settle();});}
 lines.length=0;await page.evaluate(()=>{window.g5BeforeCamera=runtime.readState().camera;});
 await page.evaluate(()=>start());
 const stateBefore=await page.evaluate(()=>runtime.readState());
 let selectionBefore=null,selectionAfter=null;
 if(scenario==='drag'){
   await page.evaluate(async()=>{
    await runtime.selectTool('select');await runtime.setSelectionMode('point');
    window.sendDrag=(type,x,y)=>runtime.canvas.dispatchEvent(new PointerEvent(type,{pointerId:2,pointerType:'mouse',clientX:x,clientY:y,button:type==='pointermove'?-1:0,buttons:type==='pointerup'?0:1,bubbles:true,cancelable:true}));
    sendDrag('pointerdown',560,380);sendDrag('pointerup',560,380);await settle();
   });
   selectionBefore=await page.evaluate(()=>runtime.readSelection());
   lines.length=0;await page.evaluate(()=>start());
   await page.evaluate(async()=>{window.pointerTimes=[];for(let i=0;i<20;++i){await new Promise(requestAnimationFrame);const start=performance.now();sendDrag(i===0?'pointerdown':i===19?'pointerup':'pointermove',560+i*3,380-i*4);pointerTimes.push({start,duration:performance.now()-start});}window.g5MeasuredEnded=performance.now();});
   selectionAfter=await page.evaluate(()=>runtime.readSelection());
 }else{
   await page.evaluate(async ({reverse,scenario})=>{for(let i=0;i<(scenario==='ink'?30:120);++i){await new Promise(requestAnimationFrame);runtime.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:400,clientY:250,deltaY:scenario==='ink'?-3:(i<60?(reverse?2:-2):(reverse?-2:2)),ctrlKey:true,bubbles:true,cancelable:true}));}window.g5MeasuredEnded=performance.now();},{reverse,scenario});
   if(scenario==='ink')await page.evaluate(async()=>{window.pointerTimes=[];for(let i=0;i<24;++i){await new Promise(requestAnimationFrame);const start=performance.now();const e=new PointerEvent(i===0?'pointerdown':i===23?'pointerup':'pointermove',{pointerId:2,pointerType:'mouse',clientX:270+i*8,clientY:230+Math.sin(i*.3)*20,button:i===0||i===23?0:-1,buttons:i===23?0:1,pressure:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:200000+i*16});runtime.canvas.dispatchEvent(e);pointerTimes.push({start,duration:performance.now()-start});}window.g5MeasuredEnded=performance.now();});
 }
 const ended=await page.evaluate(()=>window.g5MeasuredEnded);
 let raw;
 try{raw=await page.evaluate(()=>stop());}
 catch(error){await fs.writeFile(path.join(destination,'failure.json'),JSON.stringify({error:String(error),state:await page.evaluate(()=>runtime.readState()),raw:await page.evaluate(()=>window.observation),timingRows:lines},null,2));throw error;}
 raw.pointerTimes=await page.evaluate(()=>window.pointerTimes||[]);raw.fences=await page.evaluate(()=>g5Fences);
 console.log('settled');const image=await page.evaluate(async()=>{const c=runtime.canvas,g=c.getContext('webgl2'),binding=g.getParameter(g.FRAMEBUFFER_BINDING),p=new Uint8Array(c.width*c.height*4);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(0,0,c.width,c.height,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,binding);const ext=g.getExtension('WEBGL_debug_renderer_info');const digest=await crypto.subtle.digest('SHA-256',p);return {rgbaSha256:Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,'0')).join(''),width:c.width,height:c.height,renderer:g.getParameter(ext.UNMASKED_RENDERER_WEBGL)};});
 const sha256={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
 await page.screenshot({path:path.join(destination,'screen.png')});
 const operation=await page.evaluate(()=>window.g5LastOperation||[]);
 await fs.writeFile(path.join(destination,'last-operation.bin'),new Uint8Array(operation));
 const observation={operationSha256:createHash('sha256').update(new Uint8Array(operation)).digest('hex'),runnerSha256:createHash('sha256').update(await fs.readFile(new URL(import.meta.url))).digest('hex'),schema:'g5-mixed-scene-v1',brushConfig:await page.evaluate(()=>runtime.readBrushConfig()),scenario,stateBefore,selectionBefore,selectionAfter,count,dpr,reverse,brush:{profile:'round-handdrawn',revision:brushRevision},scope:'fixed-timestamp synthetic pointer corpus; single-thread CPU wall elapsed, nested exclusive/inclusive; GL submission and exact receipt are not GPU execution or physical presentation',inputEnded:ended,exactReceiptDelayMs:raw.frames.at(-1).time-ended,raw,exceptions,timingRows:lines.map(s=>JSON.parse(s.slice('AXIOM_FRAME_TIMING '.length))),image:{width:image.width,height:image.height,rgbaSha256:image.rgbaSha256,renderer:image.renderer},sha256};
 await fs.writeFile(path.join(destination,'trace.log'),lines.join('\n')+'\n');await fs.writeFile(path.join(destination,'observation.json'),JSON.stringify(observation,null,2));
 console.log(JSON.stringify({count,dpr,reverse,timingRows:lines.length,delay:observation.exactReceiptDelayMs,pixelSha256:observation.image.rgbaSha256,errors:raw.errors,exceptions}));
 const checks={noErrors:!raw.errors.length&&!exceptions.length,
 objects:raw.after.objects===count+3+(scenario==='ink'?1:0),
 semanticGeneration:BigInt(raw.after.generation)===BigInt(stateBefore.generation)+(scenario==='zoom'?0n:1n),
 moved:scenario!=='drag'||(selectionBefore?.transform&&selectionAfter?.transform&&selectionAfter.transform[4]>selectionBefore.transform[4]+40&&selectionAfter.transform[5]<selectionBefore.transform[5]-40)};
 await fs.writeFile(path.join(destination,'checks.json'),JSON.stringify(checks,null,2));
 if(Object.values(checks).some(x=>!x))process.exitCode=1;
 }
}finally{await browser.close();await new Promise(r=>server.close(r));}
