// Real external SDK demo, public calls and DOM input; no private engine access.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';import os from 'node:os';import {createHash} from 'node:crypto';
const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-interaction-consumer-'));await fs.cp(sdk,root,{recursive:true});
let html=await fs.readFile(path.join(root,'example.html'),'utf8');html=html.replace('sdk = await createRuntime','sdk = window.sdk = await createRuntime');await fs.writeFile(path.join(root,'example.html'),html);
const server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.wasm')?'application/wasm':p.endsWith('.mjs')?'text/javascript':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const checks={},observations={},errors=[];
try{
 const page=await browser.newPage({viewport:{width:1100,height:1050},deviceScaleFactor:2});page.on('pageerror',e=>errors.push(String(e)));
 await page.addInitScript(()=>{window.surfaceResets=[];for(const name of ['width','height']){const d=Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype,name);Object.defineProperty(HTMLCanvasElement.prototype,name,{...d,set(v){if(this.id.includes('canonical'))surfaceResets.push({name,value:v,time:performance.now()});d.set.call(this,v);}});}});
 await page.goto(`http://127.0.0.1:${server.address().port}/example.html`);await page.waitForFunction(()=>window.sdk&&!document.querySelector('#tools').inert);
 const pause=()=>page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
 const settle=()=>page.evaluate(async()=>{for(let n=0;n<450;++n){const s=sdk.readState();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation){await new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r)));return;}await new Promise(requestAnimationFrame);}throw Error('settle');});
 const shape=async(x,y,w=100,h=70)=>{const b=await page.locator('#canvas').boundingBox();await page.mouse.move(b.x+x,b.y+y);await page.mouse.down();await page.mouse.move(b.x+x+w,b.y+y+h,{steps:6});await page.mouse.up();await settle();};
 const tool=async key=>{await page.locator(`[data-tool="${key}"]`).click();await pause();};
 await tool('shape:0');await shape(170,200);await shape(400,350);
 const sampleCanonical=()=>page.evaluate(()=>{const c=document.querySelector('#canvas canvas[id*="canonical"]'),g=c.getContext('webgl2'),p=new Uint8Array(4),f=g.getParameter(g.FRAMEBUFFER_BINDING);g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(420,Math.max(0,c.height-480),1,1,g.RGBA,g.UNSIGNED_BYTE,p);g.bindFramebuffer(g.FRAMEBUFFER,f);return [...p];});
 const downSampleBefore=await sampleCanonical();const creationBox=await page.locator('#canvas').boundingBox();
 await page.mouse.move(creationBox.x+210,creationBox.y+240);await page.mouse.down();await pause();const downSampleAfter=await sampleCanonical();
 checks.creationDownKeepsCanonicalPixels=JSON.stringify(downSampleAfter)===JSON.stringify(downSampleBefore);
 await page.mouse.up();await settle();
 observations.creationResets=await page.evaluate(()=>surfaceResets);checks.creationDoesNotResetDrawingBuffer=observations.creationResets.length===2;
 const start=await page.evaluate(()=>sdk.readState().camera);const b=await page.locator('#canvas').boundingBox();await page.mouse.move(b.x+650,b.y+400);await page.mouse.wheel(0,40);await pause();const after=await page.evaluate(()=>sdk.readState().camera);observations.pan={start,after};checks.wheelUsesNativeScrollDirection=after.y<start.y;
 await page.evaluate(()=>sdk.panBy(0,40));await settle();
 const workspace=page.locator('#workspace');const oldWorkspace=await workspace.boundingBox();const oldCamera=await page.evaluate(()=>sdk.readState().camera);
 await page.evaluate(()=>{const w=document.querySelector('#workspace');w.style.flex='none';w.style.width='700px';w.style.height='600px';});await page.waitForFunction(()=>sdk.readState().surface.width===700);await pause();
 const resizedCamera=await page.evaluate(()=>sdk.readState().camera);const newWorkspace=await workspace.boundingBox();
 observations.resizeCamera={old:oldWorkspace,new:newWorkspace,before:oldCamera,after:resizedCamera};
 checks.resizeKeepsCamera=Math.abs(resizedCamera.x-oldCamera.x)<.01&&Math.abs(resizedCamera.y-oldCamera.y)<.01&&resizedCamera.scale===oldCamera.scale;
 await page.evaluate(()=>{const w=document.querySelector('#workspace');w.style.flex='';w.style.width='';w.style.height='';});await page.waitForFunction(()=>sdk.readState().surface.width===1100);await pause();
 const restoredCamera=await page.evaluate(()=>sdk.readState().camera);checks.resizeRestoreKeepsCamera=Math.abs(restoredCamera.x-oldCamera.x)<.01&&Math.abs(restoredCamera.y-oldCamera.y)<.01;
 await tool('tool:1');await page.locator('#selection-mode').selectOption('rectangle');await pause();await shape(80,130,500,430);observations.rectangle=await page.evaluate(()=>({state:sdk.readState(),selection:sdk.readSelection(),status:document.querySelector('#status').textContent}));checks.rectangleSelectsBoth=observations.rectangle.state.selected.length===2;
 try{const r=await page.evaluate(()=>sdk.group());observations.group=r;checks.group=true;await settle();await page.evaluate(()=>sdk.ungroup());await settle();checks.ungroup=(await page.evaluate(()=>sdk.readState().objects))===2;}catch(e){observations.group=String(e);checks.group=false;}
 // Change acquisition mode through the public call; include an actual canvas
 // background click first because selected chrome can hide the Demo selector.
 await page.evaluate(()=>sdk.setSelectionMode('lasso'));await pause();const box=await page.locator('#canvas').boundingBox();
 await page.mouse.move(box.x+70,box.y+130);await page.mouse.down();for(const [x,y]of [[600,130],[600,560],[70,560],[70,130]])await page.mouse.move(box.x+x,box.y+y,{steps:4});await page.mouse.up();await settle();observations.lasso=await page.evaluate(()=>({state:sdk.readState(),selection:sdk.readSelection(),status:document.querySelector('#status').textContent}));checks.lassoSelectsBoth=observations.lasso.state.selected.length===2;
 // An ink scene exercises compact selection geometry, not shape-only success.
 await tool('tool:0');const c=await page.locator('#canvas').boundingBox();await page.mouse.move(c.x+680,c.y+200);await page.mouse.down();await page.mouse.move(c.x+820,c.y+260,{steps:12});await page.mouse.up();await settle();
 await page.evaluate(()=>sdk.setSelectionMode('rectangle'));await pause();await shape(50,130,850,500);observations.inkSelection=await page.evaluate(()=>({state:sdk.readState(),selection:sdk.readSelection()}));checks.regionSelectsCompactAndShapes=observations.inkSelection.state.selected.length===3;
 observations.compactGroup=await page.evaluate(async()=>{
   try{const receipt=await sdk.group();return {ok:true,receipt,state:sdk.readState()};}
   catch(error){return {ok:false,code:error.code,details:error.details};}
 });
 checks.groupAcceptsCompactAndShapes=observations.compactGroup.ok&&observations.compactGroup.state.selected.length===1;
 if(observations.compactGroup.ok){await settle();await page.evaluate(()=>sdk.ungroup());await settle();checks.ungroupPreservesCompactAndShapes=(await page.evaluate(()=>sdk.readState().selected.length))===3;}
 await page.screenshot({path:output.replace(/\.json$/,'.png')});
 const sha256={};for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
 await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({checks,observations,errors,sha256,consumer:root},null,2));console.log(JSON.stringify({checks,errors}));if(errors.length||Object.values(checks).some(x=>!x))process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});}
