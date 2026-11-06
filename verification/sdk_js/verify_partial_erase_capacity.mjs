// A real root-SDK consumer. The RAF queue observes final presentation work, not
// only a canonical commit receipt. Latency budgets qualify this host fixture;
// they are not a public SDK SLO. No private bridge/state access is used.
import {chromium} from '../node_modules/playwright/index.mjs';
import {createHash} from 'node:crypto';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
const root=path.resolve(process.argv[2]);
const out=path.resolve(process.argv[3]).replace(/\.json$/,'');
const dpr=Number(process.argv[4]??2);
const result={checks:{},errors:[],source:root,deviceScaleFactor:dpr,gestures:[]};
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}</style><div id=board></div><script type=module>
import{createRuntime}from'/axiom.mjs';
const raf=requestAnimationFrame.bind(window),caf=cancelAnimationFrame.bind(window);
window.pendingFrames=new Set();
window.requestAnimationFrame=fn=>{let id=raf(t=>{pendingFrames.delete(id);fn(t);});pendingFrames.add(id);return id;};
window.cancelAnimationFrame=id=>{pendingFrames.delete(id);caf(id);};
window.sdk=await createRuntime({mount:document.getElementById('board'),permission:'localSingleUser',builtinFonts:false});
</script>`;
await fs.mkdir(path.dirname(out),{recursive:true});
result.files=Object.fromEntries(await Promise.all(['axiom.mjs','axiom.d.mts','axiom.wasm','axiom.data','internal/axiom.mjs'].map(async name=>[name,createHash('sha256').update(await fs.readFile(path.join(root,name))).digest('hex')])));
const server=http.createServer(async(req,res)=>{
 try{const u=new URL(req.url,'http://local').pathname;
  if(u==='/'){res.setHeader('Content-Type','text/html');res.end(html);return;}
  const file=path.resolve(root,'.'+u);if(!file.startsWith(root+path.sep))throw Error('path');
  res.setHeader('Content-Type',u.endsWith('.mjs')?'text/javascript':u.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
 }catch{res.statusCode=404;res.end();}
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
let browser,deadline;
const bounded=async(p,label)=>{let timer;try{return await Promise.race([p,new Promise((_,reject)=>{timer=setTimeout(()=>reject(Error('No response within 20s: '+label)),20000);})]);}finally{clearTimeout(timer);}};
const summary=values=>({meanMs:values.reduce((a,b)=>a+b,0)/values.length,p95Ms:values.toSorted((a,b)=>a-b)[Math.floor(values.length*.95)],maxMs:Math.max(...values)});
try{
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 deadline=setTimeout(()=>{result.errors.push('Overall 180s watchdog expired');void browser.close();},180000);
 result.browser=browser.version();
 const page=await browser.newPage({viewport:{width:1000,height:700},hasTouch:true,deviceScaleFactor:dpr});page.setDefaultTimeout(20000);
 page.on('pageerror',e=>result.errors.push(String(e)));
 page.on('console',m=>{if(m.text().includes('Axiom Web frame rejected:'))result.errors.push(m.text());});
 await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.sdk);
 const state=()=>bounded(page.evaluate(()=>sdk.readState()),'read state');
 const settle=async()=>{
  await page.waitForFunction(()=>{const s=sdk.readState();return !s.activeContacts&&s.generation===s.lastCommit.presentedGeneration;});
  await page.waitForFunction(()=>pendingFrames.size===0,null,{polling:20});
 };
 const strip=x=>bounded(page.evaluate(x=>{
  const c=sdk.canvas,sx=c.width/1000,sy=c.height/700,w=Math.max(1,Math.round(3*sx)),h=Math.max(1,Math.round(400*sy)),g=c.getContext('webgl2'),fb=g.getParameter(g.FRAMEBUFFER_BINDING),pixels=new Uint8Array(w*h*4);
  g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(Math.round((x-1)*sx),Math.round((700-610)*sy),w,h,g.RGBA,g.UNSIGNED_BYTE,pixels);g.bindFramebuffer(g.FRAMEBUFFER,fb);
  // Compare what the white host actually displays. Legacy canonical ink-only
  // kClear may leave transparent pixels; raw WebGL RGB alone treats them black.
  let minAlpha=255;for(let i=0;i<pixels.length;i+=4){minAlpha=Math.min(minAlpha,pixels[i+3]);for(let k=0;k<3;++k)pixels[i+k]=Math.min(255,pixels[i+k]+255-pixels[i+3]);pixels[i+3]=255;}
  return {minAlpha,width:c.width,height:c.height,rgba:[...pixels],dark:pixels.filter((v,i)=>i%4!==3&&v<220).length};
 },x),'read compositor pixels');
 const same=(a,b)=>a.length===b.length&&a.every((v,i)=>Math.abs(v-b[i])<=2);
 await page.evaluate(async()=>{await sdk.selectBrush('round-handdrawn',4);await sdk.setBrushConfig({...sdk.readBrushConfig(),size:18,thinning:0,streamline:0,smoothing:.6,color:[0,0,0,1]});await sdk.selectTool('ink');});
 await page.mouse.move(100,350);await page.mouse.down();
 await bounded(page.evaluate(async()=>{for(let i=1;i<=6000;++i){sdk.canvas.dispatchEvent(new PointerEvent('pointermove',{bubbles:true,pointerId:1,pointerType:'mouse',buttons:1,pressure:.5,clientX:100+i*1200/6000,clientY:350+100*Math.sin(i*80*Math.PI/6000)}));if(i%64===0)await new Promise(requestAnimationFrame);}}),'draw long stroke');
 await page.mouse.move(1300,350);await page.mouse.up();await settle();
 await page.evaluate(()=>sdk.selectTool('select'));await page.mouse.click(250,350);
 for(let i=0;i<15;++i){await bounded(page.evaluate(()=>sdk.duplicateSelection()),'duplicate '+i);await settle();}
 result.checks.sixteenLongStrokes=(await state()).objects===16;
 await page.evaluate(async()=>{await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');window.heartbeats=[];let previous=performance.now();window.heartbeat=setInterval(()=>{const now=performance.now();heartbeats.push(now-previous);previous=now;},20);});
 const before=await state();let beforeLast,committedLast;
 for(const x of [260,380,500,620,740]){
  const start=await state(),original=await strip(x);await page.mouse.move(x,190);const down=performance.now();await bounded(page.mouse.down(),'erase Down');const downMs=performance.now()-down,times=[];
  for(let i=1;i<=48;++i){const at=performance.now();await bounded(page.mouse.move(x,190+460*i/48),'erase Move');await bounded(page.evaluate(()=>new Promise(requestAnimationFrame)),'feedback RAF');times.push(performance.now()-at);}
  const preview=await strip(x),during=await state();
  const up=performance.now();await bounded(page.mouse.up(),'erase Up');const upMs=performance.now()-up;await settle();const settledMs=performance.now()-up;
  const committed=await strip(x),after=await state();
  result.checks['visibleBeforeUp'+x]=original.dark>20&&preview.dark===0;
  result.checks['transientOnly'+x]=during.generation===start.generation&&during.lastCommit.operationId===start.lastCommit.operationId;
  result.checks['previewMatchesCommit'+x]=same(preview.rgba,committed.rgba);
  result.gestures.push({x,downMs,upMs,settledMs,moves:summary(times),times,pixels:{canvas:[preview.width,preview.height],original:original.dark,preview:preview.dark,committed:committed.dark,rawMinAlpha:[preview.minAlpha,committed.minAlpha],maxDelta:Math.max(...preview.rgba.map((v,i)=>Math.abs(v-committed.rgba[i]))),previewFirst:preview.rgba.slice(0,16),committedFirst:committed.rgba.slice(0,16)}});
  console.log(JSON.stringify({stage:'erase',...result.gestures.at(-1),times:undefined}));
  if(x===740){beforeLast=original;committedLast=committed;}
 }
 const after=await state();result.checks.fiveAtomicCommits=BigInt(after.lastCommit.ordinal)===BigInt(before.lastCommit.ordinal)+5n;
 result.checks.feedbackP95Under75ms=result.gestures.every(g=>g.moves.p95Ms<75);
 result.checks.gesturesSettleWithin5s=result.gestures.every(g=>g.settledMs<5000);
 await page.screenshot({path:out+'-erased.png'});
 await bounded(page.evaluate(()=>sdk.undo()),'undo');await settle();const undone=await strip(740);result.undoPixels={dark:undone.dark,before:beforeLast.dark,maxDelta:Math.max(...undone.rgba.map((v,i)=>Math.abs(v-beforeLast.rgba[i])))};result.checks.oneUndoRestoresLastSweep=same(undone.rgba,beforeLast.rgba)&&(await strip(620)).dark===0;
 await bounded(page.evaluate(()=>sdk.redo()),'redo');await settle();result.checks.redoRestoresErase=same((await strip(740)).rgba,committedLast.rgba);
 const cancelBefore=await state(),cancelPixels=await strip(860);
 await page.mouse.move(860,190);await page.mouse.down();await page.mouse.move(860,650,{steps:4});await page.evaluate(()=>new Promise(requestAnimationFrame));
 result.checks.sparseSweepVisibleBeforeUp=cancelPixels.dark>20&&(await strip(860)).dark===0;
 await page.evaluate(()=>sdk.canvas.dispatchEvent(new PointerEvent('pointercancel',{pointerId:1,pointerType:'mouse',bubbles:true})));await page.mouse.up();await settle();
 result.checks.cancelRestoresWithoutCommit=(await state()).generation===cancelBefore.generation&&same((await strip(860)).rgba,cancelPixels.rgba);
 // More committed masks must not turn the next sweep into a never-ending frame.
 result.stress=[];
 for(const x of [300,420,540,660,780,880,920]){
  await page.mouse.move(x,190);const at=performance.now();await bounded(page.mouse.down(),'stress Down');await bounded(page.mouse.move(x,650,{steps:8}),'stress sweep');await bounded(page.mouse.up(),'stress Up');await settle();result.stress.push({x,totalMs:performance.now()-at});
 }
 result.checks.twelveSuccessiveSweepsComplete=(await state()).objects===16&&result.stress.every(g=>g.totalMs<5000);
 await page.evaluate(()=>clearInterval(heartbeat));result.heartbeat=summary(await page.evaluate(()=>heartbeats));result.checks.noMainThreadStallOver1500ms=result.heartbeat.maxMs<1500;
 await page.evaluate(()=>sdk.selectTool('select'));await settle();const navBefore=await state();
 const cdp=await page.context().newCDPSession(page),point=(id,x,y)=>({id,x,y,radiusX:4,radiusY:4,force:1});result.navigation={};
 for(const mode of ['pan','zoom']){
  const times=[];await cdp.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[point(11,350,550),point(12,650,550)]});
  for(let i=1;i<=32;++i){const at=performance.now(),dx=mode==='pan'?-i*3:0,spread=mode==='zoom'?(i<=16?i:32-i)*3:0;await bounded(cdp.send('Input.dispatchTouchEvent',{type:'touchMove',touchPoints:[point(11,350+dx-spread,550),point(12,650+dx+spread,550)]}),'two-finger '+mode);await bounded(page.evaluate(()=>new Promise(requestAnimationFrame)),'navigation RAF');times.push(performance.now()-at);}
  await cdp.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});result.navigation[mode]=summary(times);
 }
 await settle();const navAfter=await state();
 result.checks.navigationDoesNotMutate=navBefore.generation===navAfter.generation&&navBefore.lastCommit.operationId===navAfter.lastCommit.operationId;
 result.checks.navigationResponds=navBefore.camera.x!==navAfter.camera.x||navBefore.camera.scale!==navAfter.camera.scale;
 result.checks.navigationP95Under60ms=Object.values(result.navigation).every(v=>v.p95Ms<60);
 result.status=Object.values(result.checks).every(Boolean)&&!result.errors.length?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}
finally{clearTimeout(deadline);if(browser)await browser.close();await new Promise(r=>server.close(r));await fs.writeFile(out+'.json',JSON.stringify(result,null,2));console.log(JSON.stringify({...result,files:undefined,gestures:result.gestures.map(g=>({...g,times:undefined}))}));}
