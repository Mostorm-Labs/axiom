import {chromium} from '../../../node_modules/playwright/index.mjs';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const output=path.resolve(process.argv[2]),sdk=path.resolve(process.argv[3]);
const count=Number(process.argv[4]??64),dpr=2,scenario='rectangle-drag';
const reverse=false,brushRevision=3;
const small=process.argv[6]==='small',mountWidth=small?400:800,mountHeight=small?250:500;
await fs.mkdir(output,{recursive:true});
const settleFrames=Number(process.env.AXIOM_TIMING_SETTLE_FRAMES||300);
let html=await fs.readFile('verification/evidence/performance/g5-world-navigation/browser-final/browser.html','utf8');
html=html.replaceAll('i<300;',`i<${settleFrames};`).replaceAll('i===299',`i===${settleFrames-1}`);
html=html.replace("selectBrush('round-handdrawn',1)",`selectBrush('round-handdrawn',${brushRevision})`);
html=html.replace('width:800px;height:500px',`width:${mountWidth}px;height:${mountHeight}px`);
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
 const lines=[],navigationLines=[],exceptions=[];const received=[];
 await page.addInitScript(()=>{
  window.g5Fences=[];window.g5Draws=[];window.g5Ticks=[];const proto=WebGL2RenderingContext.prototype,bindings=new WeakMap();
  const bind=proto.bindFramebuffer;proto.bindFramebuffer=function(target,fb){if(target===this.FRAMEBUFFER||target===this.DRAW_FRAMEBUFFER)bindings.set(this,fb);return bind.call(this,target,fb);};
  for(const name of ['drawArrays','drawElements','drawArraysInstanced','drawElementsInstanced']){const raw=proto[name];proto[name]=function(...args){if(window.observation?.active&&!bindings.get(this))g5Draws.push({time:performance.now(),canonical:this.canvas===window.runtime?.canvas});return raw.apply(this,args);};}
  for(const name of ['fenceSync','clientWaitSync']){const raw=proto[name];proto[name]=function(...args){const start=performance.now();const result=raw.apply(this,args);if(window.observation?.active)g5Fences.push({name,start,duration:performance.now()-start,timeout:name==='clientWaitSync'?args[2]:null,status:name==='clientWaitSync'?result:null});return result;};}
 });
 page.on('console',m=>{if(m.text().startsWith('AXIOM_FRAME_TIMING ')){lines.push(m.text());received.push({time:Date.now(),line:m.text()});}if(m.text().startsWith('AXIOM_NAVIGATION_TIMING '))navigationLines.push(m.text());});
 page.on('pageerror',e=>exceptions.push(String(e)));

 await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.ready);await page.evaluate(()=>ready);console.log('ready');
 await page.evaluate(()=>{runtime.canvas.setPointerCapture=()=>{};runtime.canvas.hasPointerCapture=()=>false;});
 for(let n=0;n<count;++n){
  await page.evaluate(({n,w,h})=>{for(let i=0;i<16;++i){const e=new PointerEvent(i===0?'pointerdown':i===15?'pointerup':'pointermove',{pointerId:1,pointerType:'mouse',clientX:w*.28+i*w*.015,clientY:h*.28+(n%16)*h*.034+Math.sin(i*.4)*5,button:i===0||i===15?0:-1,buttons:i===15?0:1,pressure:i===15?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:1000+n*100+i});runtime.canvas.dispatchEvent(e);}}, {n,w:mountWidth,h:mountHeight});
  await page.evaluate(()=>settle());
 }
 await page.evaluate(async()=>{await runtime.selectShape('rectangle');window.shapePointer=(type,x,y)=>{const e=new PointerEvent(type,{pointerId:2,pointerType:'mouse',clientX:x,clientY:y,button:type==='pointermove'?-1:0,buttons:type==='pointerup'?0:1,bubbles:true,cancelable:true});runtime.canvas.dispatchEvent(e);};shapePointer('pointerdown',500,350);shapePointer('pointermove',640,440);shapePointer('pointerup',640,440);});await page.evaluate(()=>settle());
 await page.evaluate(async()=>{await runtime.selectTool('select');await runtime.setSelectionMode('point');shapePointer('pointerdown',560,380);shapePointer('pointerup',560,380);});await page.evaluate(()=>settle());
 const shapeBefore=await page.evaluate(()=>({state:runtime.readState(),selection:runtime.readSelection()}));
 lines.length=0;navigationLines.length=0;
 await page.evaluate(()=>start());
 await page.evaluate(async()=>{window.drawStart=performance.now();window.pointerTimes=[];for(let i=0;i<20;++i){await new Promise(requestAnimationFrame);const start=performance.now();shapePointer(i===0?'pointerdown':i===19?'pointerup':'pointermove',560+i*3,380-i*4);pointerTimes.push({start,duration:performance.now()-start,phase:i===0?'down':i===19?'up':'move'});}});
 const ended=await page.evaluate(()=>performance.now());
 const raw=await page.evaluate(()=>stop());Object.assign(raw,await page.evaluate(()=>({fences:g5Fences,draws:g5Draws,ticks:g5Ticks,drawStart,pointerTimes})));
 console.log('settled');const image=await page.evaluate(async()=>{const c=runtime.canvas,g=c.getContext('webgl2'),binding=g.getParameter(g.FRAMEBUFFER_BINDING),p=new Uint8Array(c.width*c.height*4);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(0,0,c.width,c.height,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,binding);const ext=g.getExtension('WEBGL_debug_renderer_info');const digest=await crypto.subtle.digest('SHA-256',p);return {rgbaSha256:Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,'0')).join(''),width:c.width,height:c.height,renderer:g.getParameter(ext.UNMASKED_RENDERER_WEBGL)};});
 const sha256={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
 const navigationRows=navigationLines.map(s=>JSON.parse(s.slice('AXIOM_NAVIGATION_TIMING '.length)));
 const preview=raw.draws.filter(d=>d.time>=raw.drawStart),ticks=raw.raf.filter(r=>r.start>=raw.drawStart&&r.start<=ended);
 const summary={drawStart:raw.drawStart,firstPreviewDelayMs:preview.length?preview[0].time-raw.pointerTimes[0].start:null,maxPointerMs:Math.max(...raw.pointerTimes.map(x=>x.duration)),maxDrawRafMs:Math.max(...ticks.map(x=>x.duration)),maxGapMs:Math.max(...raw.pointerTimes.slice(1).map((x,i)=>x.start-raw.pointerTimes[i].start))};
 const shapeAfter=await page.evaluate(()=>runtime.readSelection());
 const observation={shapeBefore,shapeAfter,schema:'g5-shape-drag-v1',count,dpr,scenario,small,brush:{profile:'round-handdrawn',revision:brushRevision},scope:'fixed-timestamp synthetic pointer corpus; single-thread CPU wall elapsed, nested exclusive/inclusive; GL submission and exact receipt are not GPU execution or physical presentation',inputEnded:ended,summary,raw,exceptions,navigationRows,timingRows:lines.map(s=>JSON.parse(s.slice('AXIOM_FRAME_TIMING '.length))),image:{width:image.width,height:image.height,rgbaSha256:image.rgbaSha256,renderer:image.renderer},sha256};
 await fs.writeFile(path.join(output,'trace.log'),lines.join('\n')+'\n');await fs.writeFile(path.join(output,'observation.json'),JSON.stringify(observation,null,2));
 console.log(JSON.stringify({count,dpr,scenario,small,summary,pixelSha256:observation.image.rgbaSha256,errors:raw.errors,exceptions}));
 const checks={singleCommit:BigInt(raw.after.generation)===BigInt(shapeBefore.state.generation)+1n,
 noCanonicalDuringDrag:observation.timingRows.every(t=>t.start_ns/1e6<raw.pointerTimes[0].start||t.start_ns/1e6>=raw.pointerTimes.at(-1).start),
 moved:shapeAfter.transform&&shapeBefore.selection.transform&&shapeAfter.transform[4]>shapeBefore.selection.transform[4]+40&&shapeAfter.transform[5]<shapeBefore.selection.transform[5]-40,
 feedback:preview.length>0,noErrors:!raw.errors.length&&!exceptions.length};
 await fs.writeFile(path.join(output,'checks.json'),JSON.stringify(checks,null,2));
 if(Object.values(checks).some(x=>!x))process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));}
