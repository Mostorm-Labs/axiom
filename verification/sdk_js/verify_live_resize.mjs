// External demo + real WASM/WebGL: resize must reveal pixels, not move/stretch them.
import assert from 'node:assert/strict';
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import os from 'node:os';
import {createHash} from 'node:crypto';

const sdk=path.resolve(process.argv[2]), output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-live-resize-'));
await fs.cp(sdk,root,{recursive:true});
const demo=await fs.readFile(path.join(root,'example.html'),'utf8');
await fs.writeFile(path.join(root,'example.html'),demo.replace('sdk = await createRuntime','sdk = window.sdk = await createRuntime'));
const server=http.createServer(async(req,res)=>{
  try {
    const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
    if(!file.startsWith(root+path.sep))throw Error('path');
    res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':file.endsWith('.mjs')?'text/javascript':file.endsWith('.html')?'text/html':'application/octet-stream');
    res.end(await fs.readFile(file));
  }catch{res.statusCode=404;res.end();}
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({executablePath:process.env.CHROME_EXECUTABLE||'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const checks={}, observations={}, errors=[];
try {
  const page=await browser.newPage({viewport:{width:1100,height:900},deviceScaleFactor:2});
  page.on('pageerror',e=>errors.push(String(e)));
  await page.addInitScript(()=>{
    window.bufferResets=[];
    for(const name of ['width','height']) {
      const original=Object.getOwnPropertyDescriptor(HTMLCanvasElement.prototype,name);
      Object.defineProperty(HTMLCanvasElement.prototype,name,{...original,set(value){
        if(this.id.endsWith('-canonical'))window.bufferResets.push({name,value});
        original.set.call(this,value);
      }});
    }
  });
  await page.goto(`http://127.0.0.1:${server.address().port}/example.html`);
  await page.waitForFunction(()=>window.sdk&&!document.querySelector('#tools').inert);
  const twoFrames=()=>page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
  const display=()=>page.evaluate(()=>{
    const mount=document.getElementById('canvas'), canonical=sdk.canvas;
    const layers=[...mount.querySelectorAll('canvas')].filter(c=>getComputedStyle(c).visibility!=='hidden');
    const top=layers.sort((a,b)=>Number(getComputedStyle(a).zIndex||0)-Number(getComputedStyle(b).zIndex||0)).at(-1);
    const rect=top.getBoundingClientRect(), origin=mount.getBoundingClientRect(), pixel=document.createElement('canvas');pixel.width=pixel.height=1;
    const ctx=pixel.getContext('2d');ctx.drawImage(top,(origin.x+180-rect.x)*top.width/rect.width,(origin.y+240-rect.y)*top.height/rect.height,1,1,0,0,1,1);
    return {camera:sdk.readState().camera,surface:sdk.readState().surface,generation:sdk.readState().generation,
      objects:sdk.readState().objects,resets:window.bufferResets.length,buffer:[canonical.width,canonical.height],
      canonical:canonical.getBoundingClientRect().toJSON(),mount:mount.getBoundingClientRect().toJSON(),
      pixel:[...ctx.getImageData(0,0,1,1).data],retained:top.id.endsWith('-retained')};
  });
  const sameCamera=(a,b)=>a.scale===b.scale&&a.x===b.x&&a.y===b.y;
  const blue=s=>s.pixel[2]>s.pixel[0]+20&&s.pixel[3]>200;
  const initial=await display();
  checks.canvasCoversViewport=initial.mount.x===0&&initial.mount.y===0&&initial.mount.width===1100&&initial.mount.height===900;
  // Hiding/changing Shell chrome must never affect Runtime surface metrics.
  await page.evaluate(()=>{for(const id of ['tools','properties','status'])document.getElementById(id).style.display='none';});
  await twoFrames();const hidden=await display();
  await page.evaluate(()=>{for(const id of ['tools','properties','status'])document.getElementById(id).style.display='';});
  await twoFrames();
  checks.chromeDoesNotResizeSurface=hidden.mount.x===initial.mount.x&&hidden.mount.y===initial.mount.y&&hidden.mount.width===initial.mount.width&&hidden.mount.height===initial.mount.height&&hidden.resets===initial.resets;
  // Use real pointer input at a location outside the floating palettes.
  await page.evaluate(async()=>{await sdk.selectShape('rectangle');await sdk.setAppearance({selected:false,fill:[0,0,1,1]});});
  const mount=await page.locator('#canvas').boundingBox();
  await page.mouse.move(mount.x+130,mount.y+200);await page.mouse.down();
  await page.mouse.move(mount.x+300,mount.y+290,{steps:5});await page.mouse.up();
  await page.waitForFunction(()=>sdk.readState().objects===1&&sdk.readState().lastCommit.presentedGeneration===sdk.readState().generation);
  await twoFrames();
  const before=await display();observations.before=before;
  assert.ok(blue(before),'fixture has actual blue pixels');
  // A burst crosses multiple RAFs; each change must leave the GPU buffer alone.
  const burst=[];
  for(const width of [1040,980,920,860,800]) {
    await page.setViewportSize({width,height:900});await twoFrames();burst.push(await display());
  }
  observations.burst=burst;
  checks.liveResizeKeepsGpuBuffer=burst.every(s=>s.resets===before.resets);
  checks.liveResizeKeepsPixelScale=burst.every(s=>s.canonical.width===before.canonical.width&&s.canonical.height===before.canonical.height);
  checks.liveResizeKeepsCamera=burst.every(s=>sameCamera(s.camera,before.camera));
  checks.liveResizeKeepsVisiblePixels=burst.every(blue);
  await page.waitForFunction(()=>sdk.readState().surface.width===800&&![...document.querySelectorAll('canvas')].some(c=>c.id.endsWith('-retained')));
  const after=await display();observations.after=after;
  checks.resizeBurstCommitsOnce=after.resets===before.resets+2;
  checks.finalResizeKeepsCamera=sameCamera(after.camera,before.camera);
  checks.finalResizeKeepsVisiblePixels=blue(after);
  checks.finalBufferMatchesViewport=after.buffer[0]===1600&&after.buffer[1]===1800;
  checks.resizePreservesDocument=after.generation===before.generation&&after.objects===before.objects;
  // Starting to draw after a resize must use the final view coordinates.
  await page.setViewportSize({width:850,height:900});
  await page.mouse.move(440,240);await page.mouse.down();await page.mouse.move(540,300,{steps:4});await page.mouse.up();
  await page.waitForFunction(()=>sdk.readState().objects===2&&sdk.readState().surface.width===850&&sdk.readState().lastCommit.presentedGeneration===sdk.readState().generation);
  checks.drawImmediatelyAfterResize=(await page.evaluate(()=>sdk.readState().lastCommit.presentedGeneration===sdk.readState().generation));
  checks.noSdkErrors=await page.locator('#status').getAttribute('data-error')!=='true';
  await page.screenshot({path:output.replace(/\.json$/,'.png')});
  // Close must cancel a pending resize timer; no post-close callbacks/errors.
  await page.setViewportSize({width:820,height:900});await page.evaluate(()=>sdk.close());await page.waitForTimeout(250);
  checks.closeReleasesDisplay=await page.locator('#canvas').evaluate(mount=>mount.children.length===0);
  checks.closeRestoresMountStyle=await page.locator('#canvas').evaluate(mount=>mount.style.overflow==='');
  checks.noPageErrors=errors.length===0;
  const sha256={};for(const name of ['axiom.mjs','example.html','axiom.wasm','internal/axiom.mjs'])sha256[name]=createHash('sha256').update(await fs.readFile(path.join(sdk,name))).digest('hex');
  await fs.mkdir(path.dirname(output),{recursive:true});await fs.writeFile(output,JSON.stringify({checks,observations,errors,sha256},null,2));
  console.log(JSON.stringify({checks,errors}));if(Object.values(checks).some(x=>!x))process.exitCode=1;
}finally{await browser.close();await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});}
