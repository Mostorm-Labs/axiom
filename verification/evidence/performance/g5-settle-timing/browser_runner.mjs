import {chromium} from '../../../node_modules/playwright/index.mjs';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
const output=path.resolve(process.argv[2]),sdk=path.resolve(process.argv[3]);
const count=Number(process.argv[4]||64),dpr=Number(process.argv[5]||2),reverse=process.argv[6]==='reverse';
await fs.mkdir(output,{recursive:true});
let html=await fs.readFile('verification/evidence/performance/g5-world-navigation/browser-final/browser.html','utf8');
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
 await page.addInitScript(()=>{
  window.g5Fences=[];const proto=WebGL2RenderingContext.prototype;
  for(const name of ['fenceSync','clientWaitSync']){const raw=proto[name];proto[name]=function(...args){const start=performance.now();const result=raw.apply(this,args);if(window.observation?.active)g5Fences.push({name,start,duration:performance.now()-start,status:name==='clientWaitSync'?result:null});return result;};}
 });
 page.on('console',m=>{if(m.text().startsWith('AXIOM_FRAME_TIMING '))lines.push(m.text());});
 page.on('pageerror',e=>exceptions.push(String(e)));

 await page.goto(`http://127.0.0.1:${server.address().port}/`);await page.waitForFunction(()=>window.ready);await page.evaluate(()=>ready);console.log('ready');
 await page.evaluate(()=>{runtime.canvas.setPointerCapture=()=>{};runtime.canvas.hasPointerCapture=()=>false;});
 for(let n=0;n<count;++n){
  await page.evaluate(n=>{for(let i=0;i<16;++i){const e=new PointerEvent(i===0?'pointerdown':i===15?'pointerup':'pointermove',{pointerId:1,pointerType:'mouse',clientX:230+i*12,clientY:140+(n%16)*17+Math.sin(i*.4)*5,button:i===0||i===15?0:-1,buttons:i===15?0:1,pressure:i===15?0:.5,bubbles:true,cancelable:true});Object.defineProperty(e,'timeStamp',{value:1000+n*100+i});runtime.canvas.dispatchEvent(e);}},n);
  await page.evaluate(()=>settle());
 }
 lines.length=0;await page.evaluate(()=>{window.g5BeforeCamera=runtime.readState().camera;});
 await page.evaluate(()=>start());
 await page.evaluate(async reverse=>{for(let i=0;i<120;++i){await new Promise(requestAnimationFrame);runtime.canvas.dispatchEvent(new WheelEvent('wheel',{clientX:400,clientY:250,deltaY:i<60?(reverse?2:-2):(reverse?-2:2),ctrlKey:true,bubbles:true,cancelable:true}));}},reverse);
 const ended=await page.evaluate(()=>performance.now());
 const raw=await page.evaluate(()=>stop());raw.fences=await page.evaluate(()=>g5Fences);
 console.log('settled');const image=await page.evaluate(async()=>{const c=runtime.canvas,g=c.getContext('webgl2'),binding=g.getParameter(g.FRAMEBUFFER_BINDING),p=new Uint8Array(c.width*c.height*4);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(0,0,c.width,c.height,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,binding);const ext=g.getExtension('WEBGL_debug_renderer_info');const digest=await crypto.subtle.digest('SHA-256',p);return {rgbaSha256:Array.from(new Uint8Array(digest),b=>b.toString(16).padStart(2,'0')).join(''),width:c.width,height:c.height,renderer:g.getParameter(ext.UNMASKED_RENDERER_WEBGL)};});
 const sha256={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
 const observation={schema:'g5-settle-timing-v1',count,dpr,reverse,scope:'fixed-timestamp synthetic pointer corpus; single-thread CPU wall elapsed, nested exclusive/inclusive; GL submission and exact receipt are not GPU execution or physical presentation',inputEnded:ended,exactReceiptDelayMs:raw.frames.at(-1).time-ended,raw,exceptions,timingRows:lines.map(s=>JSON.parse(s.slice('AXIOM_FRAME_TIMING '.length))),image:{width:image.width,height:image.height,rgbaSha256:image.rgbaSha256,renderer:image.renderer},sha256};
 await fs.writeFile(path.join(output,'trace.log'),lines.join('\n')+'\n');await fs.writeFile(path.join(output,'observation.json'),JSON.stringify(observation,null,2));
 console.log(JSON.stringify({count,dpr,reverse,timingRows:lines.length,delay:observation.exactReceiptDelayMs,pixelSha256:observation.image.rgbaSha256,errors:raw.errors,exceptions}));
 if(raw.errors.length||exceptions.length)process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));}
