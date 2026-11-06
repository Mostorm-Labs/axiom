// Actual-browser qualification of the observer only;not Runtime/platform proof.
import {chromium}from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import {createHash}from 'node:crypto';
import {readFixtureModule}from './fixture_modules.mjs';
const output=process.argv[2];if(!output)throw Error('OUTPUT required');
const result={scope:'Observer qualification on actual Chrome/WebGL2;no Axiom Runtime performance or physical presentation claim;20ms forwarding GL-latency injection validates return-side timestamps',errors:[],artifacts:{}};
for(const n of ['navigation_observer.mjs','feedback_metrics.mjs','browser_metrics.mjs'])result.artifacts[n]=createHash('sha256').update(await fs.readFile(new URL(n,import.meta.url))).digest('hex');
const server=http.createServer(async(req,res)=>{const bytes=await readFixtureModule(req.url);res.writeHead(bytes?200:404,{'Content-Type':'text/javascript'});res.end(bytes??'');});let browser;
try{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
 const page=await browser.newPage();page.on('pageerror',e=>result.errors.push(String(e)));
 await page.goto(`http://127.0.0.1:${server.address().port}/browser_metrics.mjs`);
 result.observation=await page.evaluate(async()=>{
  const {installNavigationObserver}=await import('/navigation_observer.mjs');const {summarizeInputFeedback}=await import('/feedback_metrics.mjs');
  const check=(v,t)=>{if(!v)throw Error(t);};const canvas=document.createElement('canvas');canvas.width=canvas.height=32;document.body.append(canvas);
  const nativeDraw=WebGL2RenderingContext.prototype.drawArrays;let injectDelay=true;
  WebGL2RenderingContext.prototype.drawArrays=function(...args){if(injectDelay){const at=performance.now();while(performance.now()-at<20){}}return nativeDraw.apply(this,args);};
  const observer=installNavigationObserver(()=>canvas),gl=canvas.getContext('webgl2',{preserveDrawingBuffer:true,antialias:false});check(gl,'WebGL2 unavailable');
  const shader=(type,source)=>{const s=gl.createShader(type);gl.shaderSource(s,source);gl.compileShader(s);check(gl.getShaderParameter(s,gl.COMPILE_STATUS),gl.getShaderInfoLog(s));return s;};
  const p=gl.createProgram();gl.attachShader(p,shader(gl.VERTEX_SHADER,'#version 300 es\nin vec2 a;void main(){gl_Position=vec4(a,0.,1.);}'));
  gl.attachShader(p,shader(gl.FRAGMENT_SHADER,'#version 300 es\nprecision mediump float;out vec4 c;void main(){c=vec4(1.,0.,0.,1.);}'));gl.linkProgram(p);check(gl.getProgramParameter(p,gl.LINK_STATUS),'program link');gl.useProgram(p);
  const buffer=gl.createBuffer();gl.bindBuffer(gl.ARRAY_BUFFER,buffer);gl.bufferData(gl.ARRAY_BUFFER,new Float32Array([-1,-1,3,-1,-1,3]),gl.STATIC_DRAW);const loc=gl.getAttribLocation(p,'a');gl.enableVertexAttribArray(loc);gl.vertexAttribPointer(loc,2,gl.FLOAT,false,0,0);gl.viewport(0,0,32,32);
  const delay=ms=>new Promise(r=>setTimeout(r,ms)),raf=()=>new Promise(requestAnimationFrame);
  const pixel=()=>{const prior=gl.getParameter(gl.READ_FRAMEBUFFER_BINDING);gl.bindFramebuffer(gl.READ_FRAMEBUFFER,null);const b=new Uint8Array(4);gl.readPixels(16,16,1,1,gl.RGBA,gl.UNSIGNED_BYTE,b);gl.bindFramebuffer(gl.READ_FRAMEBUFFER,prior);return Array.from(b);};
  const begin=()=>{const x={started:performance.now(),receipts:[],longTasks:[]};observer.begin(x);return x;};
  const end=x=>{x.inputEnded=performance.now();observer.end(x);x.ended=x.observationEnded;return summarizeInputFeedback(x);};
  let busyDraw=true,dispatches=0;canvas.addEventListener('wheel',e=>{++dispatches;e.preventDefault();if(busyDraw){const at=performance.now();while(performance.now()-at<20){}gl.drawArrays(gl.TRIANGLES,0,3);}});
  const event=()=>new WheelEvent('wheel',{cancelable:true});
  const pre=begin();await delay(80);const returned=observer.dispatch(canvas,event());const preSummary=end(pre);
  check(returned===false&&dispatches===1,'input forwarding changed dispatch semantics');check(preSummary.preInputWaitMs>=70&&preSummary.firstContentResponseMs<75,'pre-input wait leaked into response');
  check(preSummary.firstContentResponseMs>=40&&pre.colorOps.find(o=>o.kind==='content').ended-pre.colorOps.find(o=>o.kind==='content').started>=20,'API return timestamp was recorded before slow GL call returned');
  check(JSON.stringify(pixel())==='[255,0,0,255]','draw forwarding changed actual pixels');injectDelay=false;
  busyDraw=false;const clears=begin();observer.dispatch(canvas,event());for(let i=0;i<8;++i){await raf();gl.clear(gl.COLOR_BUFFER_BIT);}
  const clearSummary=end(clears);check(JSON.stringify(pixel())==='[0,0,0,0]','clear forwarding changed actual pixels');check(clearSummary.status==='CENSORED'&&clearSummary.clearCalls===8&&clearSummary.censoredContentGapLowerBoundMs>=70,'clear-only stream hid content absence');
  const masked=begin();observer.dispatch(canvas,event());gl.colorMask(false,false,false,false,true);gl.drawArrays(gl.TRIANGLES,0,3);const maskedSummary=end(masked);check(JSON.stringify(pixel())==='[0,0,0,0]','colorMask forwarding changed actual pixels');
  check(maskedSummary.status==='CENSORED'&&maskedSummary.maskedCalls===1,'masked draw was counted as content');gl.colorMask(true,true,true,true);
  const tex=gl.createTexture();gl.bindTexture(gl.TEXTURE_2D,tex);gl.texImage2D(gl.TEXTURE_2D,0,gl.RGBA8,32,32,0,gl.RGBA,gl.UNSIGNED_BYTE,null);
  const fbo=gl.createFramebuffer();gl.bindFramebuffer(gl.FRAMEBUFFER,fbo);gl.framebufferTexture2D(gl.FRAMEBUFFER,gl.COLOR_ATTACHMENT0,gl.TEXTURE_2D,tex,0);check(gl.checkFramebufferStatus(gl.FRAMEBUFFER)===gl.FRAMEBUFFER_COMPLETE,'offscreen framebuffer');
  const off=begin();observer.dispatch(canvas,event());gl.clearColor(0,1,0,1);gl.clear(gl.COLOR_BUFFER_BIT);const offSummary=end(off);check(offSummary.contentCalls===0&&offSummary.clearCalls===0,'offscreen operation qualified default framebuffer');
  gl.bindFramebuffer(gl.DRAW_FRAMEBUFFER,null);gl.colorMask(false,false,false,false);const copy=begin();observer.dispatch(canvas,event());gl.blitFramebuffer(0,0,32,32,0,0,32,32,gl.COLOR_BUFFER_BIT,gl.NEAREST);const copySummary=end(copy);const copied=pixel();check(gl.getError()===gl.NO_ERROR,'blit GL error');check(JSON.stringify(copied)==='[0,255,0,255]','blit forwarding changed actual pixels:'+JSON.stringify(copied));check(copySummary.contentCalls===1,'READ framebuffer binding hid actual default-target blit');
  let fired=false;const cancelled=requestAnimationFrame(()=>{fired=true;});cancelAnimationFrame(cancelled);const before=observer.activity();await delay(80);const after=observer.activity();
  check(!fired&&before.pending===0&&after.pending===0&&before.sdkRequests===after.sdkRequests&&before.sdkCallbacks===after.sdkCallbacks,'observer scheduled idle work or broke cancellation');
  observer.restore();WebGL2RenderingContext.prototype.drawArrays=nativeDraw;return {preInput:preSummary,clearOnly:clearSummary,masked:maskedSummary,offscreen:offSummary,defaultBlit:copySummary,idle:{before,after},forwardedEvents:dispatches};
 });result.status=result.errors.length?'FAIL':'PASS';
}catch(error){result.status='FAIL';result.error=String(error.stack??error);}finally{
 if(browser)await browser.close();await new Promise(r=>server.close(r));await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify({status:result.status,error:result.error}));if(result.status!=='PASS')process.exitCode=1;
}
