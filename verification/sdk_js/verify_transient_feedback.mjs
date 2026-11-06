// Shared Runtime feedback via a copied root SDK and real Chrome pointer input.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import http from 'node:http';
const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-feedback-'));
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}</style><div id="board"></div><script type="module">import{createRuntime}from'./axiom.mjs';window.errors=[];window.sdk=await createRuntime({mount:document.getElementById('board'),documentId:'4f100000000000000000000000000001',permission:'localSingleUser',builtinFonts:false,onError:e=>errors.push(String(e))});</script>`;
let server,browser;const result={checks:{},observations:{},errors:[]};
try{
 await fs.cp(sdk,root,{recursive:true});await fs.writeFile(path.join(root,'index.html'),html);await fs.mkdir(path.dirname(output),{recursive:true});
 server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://local').pathname.replace(/\/$/,'/index.html'));if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});await page.addInitScript(()=>{window.fxDraws=0;window.fxRafs=0;const raf=window.requestAnimationFrame;window.requestAnimationFrame=cb=>raf.call(window,t=>{fxRafs++;cb(t);});for(const name of ['drawArrays','drawElements','blitFramebuffer']){const fn=WebGL2RenderingContext.prototype[name];WebGL2RenderingContext.prototype[name]=function(...args){fxDraws++;return fn.apply(this,args);};}});page.on('pageerror',e=>result.errors.push(String(e)));
 const open=async()=>{await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.sdk);};
 const settle=async()=>{await page.waitForFunction(()=>{const s=sdk.readState();return !s.activeContacts&&s.lastCommit.presentedGeneration===s.generation;});await page.waitForTimeout(60);};
 const sample=async(points)=>{const png=await page.screenshot();return page.evaluate(async({bytes,points})=>{const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'})),c=document.createElement('canvas');c.width=image.width;c.height=image.height;const g=c.getContext('2d');g.drawImage(image,0,0);image.close();return points.map(([x,y])=>[...g.getImageData(x*2,y*2,1,1).data]);},{bytes:[...png],points});};
 await open();
 for(const [y,color]of [[210,[0,0,0,1]],[330,[0,.3,.9,1]]]){
  await page.evaluate(async color=>{await sdk.selectBrush('round-handdrawn',4);await sdk.setBrushConfig({...sdk.readBrushConfig(),size:14,thinning:0,color});},color);
  await page.mouse.move(150,y);await page.mouse.down();await page.mouse.move(700,y,{steps:18});await page.mouse.up();await settle();
 }
 await page.evaluate(()=>sdk.setEraser({mode:1,diameter:24}));const before=await page.evaluate(()=>sdk.readState());const exportedBefore=await page.evaluate(async()=>[...(await sdk.exportImage({bounds:[200,180,280,360],padding:0})).bytes]);
 await page.mouse.move(300,210);await page.mouse.down();await page.mouse.move(310,210);await page.mouse.move(780,210,{steps:8});await page.mouse.move(780,450,{steps:6});await page.waitForTimeout(350);
 const one=await sample([[240,210],[240,330]]);result.observations.firstCandidate=one;
 result.checks.blackCandidateBecomesGray=one[0][0]>150&&one[0][0]<245&&Math.abs(one[0][0]-one[0][2])<4;
 result.checks.unhitBlueKeepsColor=one[1][2]>180&&one[1][0]<60;
 await page.mouse.move(500,330);await page.mouse.move(600,450,{steps:4});await page.waitForTimeout(100);
 const both=await sample([[240,210],[240,330]]);result.observations.bothCandidates=both;
 result.checks.blueCandidateBecomesLightBlue=both[1][0]>150&&both[1][2]>both[1][0]+10;
 const during=await page.evaluate(()=>sdk.readState());result.checks.feedbackIsTransient=during.generation===before.generation&&during.objects===before.objects&&during.lastCommit.operationId===before.lastCommit.operationId;
 const exportedDuring=await page.evaluate(async()=>[...(await sdk.exportImage({bounds:[200,180,280,360],padding:0})).bytes]);result.checks.feedbackExcludedFromExport=JSON.stringify(exportedBefore)===JSON.stringify(exportedDuring);
 await page.screenshot({path:output.replace(/\.json$/,'-candidates.png')});
 await page.mouse.up();await settle();const after=await page.evaluate(()=>sdk.readState());result.checks.atomicDelete=after.objects===0&&BigInt(after.lastCommit.ordinal)===BigInt(before.lastCommit.ordinal)+1n;
 await page.evaluate(()=>sdk.undo());await settle();const restored=await sample([[240,210],[240,330]]);result.checks.singleUndoRestoresColor=(await page.evaluate(()=>sdk.readState().objects))===2&&restored[0][0]<30&&restored[1][0]<60;
 // Pointer cancellation travels through the ordinary DOM adapter.
 await page.mouse.move(300,210);await page.mouse.down();await page.mouse.move(340,210,{steps:3});await page.waitForTimeout(60);
 const cancelBefore=await page.evaluate(()=>sdk.readState());await page.evaluate(()=>sdk.canvas.dispatchEvent(new PointerEvent('pointercancel',{pointerId:1,pointerType:'mouse',bubbles:true})));await page.mouse.up();await page.waitForTimeout(300);
 const cancelled=await page.evaluate(()=>sdk.readState()),cancelColor=await sample([[240,210]]);result.checks.cancelRestoresWithoutOperation=cancelled.generation===cancelBefore.generation&&cancelled.objects===2&&cancelColor[0][0]<30;
 // An empty sweep still has a short-lived, gray pointer trail.
 await page.mouse.move(180,500);await page.mouse.down();await page.mouse.move(500,500,{steps:12});await page.waitForTimeout(35);const eraserTail=await sample([[460,500]]);await page.mouse.up();await page.waitForTimeout(500);const eraserGone=await sample([[460,500]]);
 result.observations.eraserTail={eraserTail,eraserGone};result.checks.eraserTrailVisibleThenGone=eraserTail[0][0]<245&&eraserTail[0][0]>80&&eraserGone[0][0]>250;
 result.errors.push(...await page.evaluate(()=>errors));await page.evaluate(()=>sdk.close());await open();
 await page.evaluate(async()=>{await sdk.selectBrush('laser-transient',1);await sdk.setBrushConfig({...sdk.readBrushConfig(),size:12,laserFadeMs:700,laserTailLength:1000});});
 await page.mouse.move(180,450);await page.mouse.down();for(const x of [260,340,420,500,580]){await page.mouse.move(x,450);await page.waitForTimeout(70);}await page.mouse.up();await page.waitForTimeout(60);
 const laser=await sample([[260,450],[540,450]]);result.observations.laserAgeGradient=laser;
 result.checks.laserOlderPartFadesFirst=laser[0][1]>laser[1][1]+30&&laser[1][1]<230;
 await page.waitForTimeout(1000);result.checks.laserFullyDisappears=(await sample([[540,450]]))[0][1]>250;const idleBefore=await page.evaluate(()=>({draws:fxDraws,rafs:fxRafs}));await page.waitForTimeout(200);const idleAfter=await page.evaluate(()=>({draws:fxDraws,rafs:fxRafs}));result.observations.idle={idleBefore,idleAfter};result.checks.noIdleRedraw=idleBefore.draws===idleAfter.draws&&idleBefore.rafs===idleAfter.rafs;
 await page.mouse.move(200,450);await page.mouse.down();await page.mouse.move(300,450);await page.waitForTimeout(900);await page.mouse.move(600,450,{steps:5});await page.waitForTimeout(50);
 const resumed=await sample([[570,450]]);result.observations.laserResumed=resumed;result.checks.laserResumesAfterHeldPause=resumed[0][1]<230;
 await page.mouse.up();await page.waitForTimeout(850);result.checks.laserNeverMutatesDocument=(await page.evaluate(()=>sdk.readState().objects))===0;try{await page.evaluate(()=>sdk.close());result.checks.laserClosesCleanly=true;}catch(e){result.checks.laserClosesCleanly=false;result.observations.closeError=String(e);}
 result.errors.push(...await page.evaluate(()=>errors));result.status=result.errors.length===0&&Object.values(result.checks).every(Boolean)?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify(result));}
