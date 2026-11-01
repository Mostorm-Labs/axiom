import {chromium} from '../../../node_modules/playwright/index.mjs';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const output=path.resolve(process.argv[2]),sdk=path.resolve(process.argv[3]);
const count=Number(process.argv[4]??64),dpr=2,scenario='pinch-in-draw';
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
 lines.length=0;navigationLines.length=0;await page.evaluate(()=>{window.g5BeforeCamera=runtime.readState().camera;});
 await page.evaluate(()=>start());
 await page.evaluate(async()=>{for(let i=0;i<30;++i){await new Promise(requestAnimationFrame);runtime.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:400,clientY:250,deltaY:-3,ctrlKey:true,bubbles:true,cancelable:true}));}});
 await page.evaluate(async delay=>{await new Promise(r=>setTimeout(r,delay));window.drawStart=performance.now();window.pointerTimes=[];for(let i=0;i<24;++i){await new Promise(requestAnimationFrame);const start=performance.now();const e=new PointerEvent(i===0?'pointerdown':i===23?'pointerup':'pointermove',{pointerId:2,pointerType:'mouse',clientX:270+i*8,clientY:230+Math.sin(i*.3)*20,button:i===0||i===23?0:-1,buttons:i===23?0:1,pressure:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:20000+i*16});runtime.canvas.dispatchEvent(e);pointerTimes.push({start,duration:performance.now()-start,phase:e.type});}},Number(process.env.DRAW_DELAY||0));
 const ended=await page.evaluate(()=>performance.now());
 const raw=await page.evaluate(()=>stop());Object.assign(raw,await page.evaluate(()=>({fences:g5Fences,draws:g5Draws,ticks:g5Ticks,drawStart,pointerTimes})));
 console.log('settled');const image=await page.evaluate(async()=>{const c=runtime.canvas,g=c.getContext('webgl2'),binding=g.getParameter(g.FRAMEBUFFER_BINDING),p=new Uint8Array(c.width*c.height*4);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(0,0,c.width,c.height,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,binding);const ext=g.getExtension('WEBGL_debug_renderer_info');const digest=await crypto.subtle.digest('SHA-256',p);return {rgbaSha256:Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,'0')).join(''),width:c.width,height:c.height,renderer:g.getParameter(ext.UNMASKED_RENDERER_WEBGL)};});
 const sha256={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
 const navigationRows=navigationLines.map(s=>JSON.parse(s.slice('AXIOM_NAVIGATION_TIMING '.length)));
 const preview=raw.draws.filter(d=>!d.canonical&&d.time>=raw.drawStart),ticks=raw.raf.filter(r=>r.start>=raw.drawStart&&r.start<=ended);
 const summary={drawStart:raw.drawStart,firstPreviewDelayMs:preview.length?preview[0].time-raw.pointerTimes[0].start:null,maxPointerMs:Math.max(...raw.pointerTimes.map(x=>x.duration)),maxDrawRafMs:Math.max(...ticks.map(x=>x.duration)),maxGapMs:Math.max(...raw.pointerTimes.slice(1).map((x,i)=>x.start-raw.pointerTimes[i].start))};
 const observation={schema:'g5-draw-after-zoom-v1',count,dpr,scenario,small,brush:{profile:'round-handdrawn',revision:brushRevision},scope:'fixed-timestamp synthetic pointer corpus; single-thread CPU wall elapsed, nested exclusive/inclusive; GL submission and exact receipt are not GPU execution or physical presentation',inputEnded:ended,summary,raw,exceptions,navigationRows,timingRows:lines.map(s=>JSON.parse(s.slice('AXIOM_FRAME_TIMING '.length))),image:{width:image.width,height:image.height,rgbaSha256:image.rgbaSha256,renderer:image.renderer},sha256};
 await fs.writeFile(path.join(output,'trace.log'),lines.join('\n')+'\n');await fs.writeFile(path.join(output,'observation.json'),JSON.stringify(observation,null,2));
 console.log(JSON.stringify({count,dpr,scenario,small,summary,pixelSha256:observation.image.rgbaSha256,errors:raw.errors,exceptions}));
 const checks={exactSubmitted:observation.timingRows.some(t=>t.outcome==='accepted'),previewSubmitted:preview.length>0,singleSemanticCommit:raw.after.objects===raw.before.objects+1,cooperativeCanonical:observation.timingRows.every(t=>t.settle),
 noCanonicalDuringLiveStroke:observation.timingRows.every(t=>t.start_ns/1e6<raw.pointerTimes[0].start||t.start_ns/1e6>=raw.pointerTimes.at(-1).start),
 noErrors:!raw.errors.length&&!exceptions.length};
 await fs.writeFile(path.join(output,'checks.json'),JSON.stringify(checks,null,2));
 if(Object.values(checks).some(v=>!v))process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));}
