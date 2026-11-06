// Read compositor pixels before Up, including sparse sweeps and mixed scenes.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import http from 'node:http';
const sdkRoot=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]),root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-live-erase-'));
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}</style><div id="board"></div><script type="module">import{createRuntime}from'./axiom.mjs';window.sdk=await createRuntime({mount:document.getElementById('board'),permission:'localSingleUser',builtinFonts:false});</script>`;
let browser,server;const result={checks:{},observations:{},errors:[]};
try{
 await fs.cp(sdkRoot,root,{recursive:true});await fs.writeFile(path.join(root,'index.html'),html);await fs.mkdir(path.dirname(output),{recursive:true});
 server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://local').pathname.replace(/\/$/,'/index.html'));if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const page=await browser.newPage({viewport:{width:1000,height:700}});page.on('pageerror',e=>result.errors.push(String(e)));page.on('console',m=>{if(m.text().includes('Axiom Web frame rejected:'))result.errors.push(m.text());});
 const pause=()=>page.waitForTimeout(160),open=async()=>{await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.sdk);await pause();};
 const drag=async(a,b,steps=12)=>{await page.mouse.move(...a);await page.mouse.down();await page.mouse.move(...b,{steps});await page.mouse.up();await pause();};
 const state=()=>page.evaluate(()=>sdk.readState());
 const pixel=async()=>{const bytes=await page.screenshot({clip:{x:500,y:350,width:1,height:1}});return page.evaluate(async b=>{const im=await createImageBitmap(new Blob([new Uint8Array(b)],{type:'image/png'}));const c=document.createElement('canvas');c.width=c.height=1;const g=c.getContext('2d');g.drawImage(im,0,0);im.close();return [...g.getImageData(0,0,1,1).data];},[...bytes]);};
 for(const kind of ['mixed','sparse','colored']){
  await open();if(kind==='mixed'){await page.evaluate(()=>sdk.selectShape('rectangle'));await drag([100,200],[900,500]);}
  if(kind==='colored')await page.evaluate(()=>sdk.setBackground({mode:'plain',color:[.6,.8,.65,1],patternColor:[0,0,0,1]}));
  await page.evaluate(async()=>{await sdk.selectBrush('round-handdrawn',4);await sdk.setBrushConfig({...sdk.readBrushConfig(),size:60,thinning:0,streamline:0,smoothing:0,color:[0,0,0,1]});await sdk.selectTool('ink');});
  await drag(kind==='sparse'?[500,250]:[150,350],kind==='sparse'?[500,450]:[850,350]);
  await page.evaluate(()=>sdk.setEraser({mode:2,diameter:48}));const before=await state();
  const exportBefore=await page.evaluate(async()=>[...(await sdk.exportImage({bounds:[100,200,900,500],padding:0})).bytes]);
  await page.mouse.move(250,350);await page.mouse.down();await page.mouse.move(750,350,{steps:kind==='sparse'?1:8});await pause();const during=await state(),preview=await pixel();await page.screenshot({path:output.replace(/\.json$/,'-'+kind+'-preview.png')});
  const exportDuring=await page.evaluate(async()=>[...(await sdk.exportImage({bounds:[100,200,900,500],padding:0})).bytes]);result.checks[kind+'PreviewExcludedFromExport']=JSON.stringify(exportBefore)===JSON.stringify(exportDuring);
  await page.mouse.up();await pause();const after=await state(),committed=await pixel();
  const expected=kind==='mixed'?[66,139,216]:kind==='colored'?[153,204,166]:[255,255,255];const close=(a,b)=>a.slice(0,3).every((v,i)=>Math.abs(v-b[i])<=6);
  result.observations[kind]={preview,committed,expected};result.checks[kind+'PreviewErasesBeforeUp']=close(preview,expected);result.checks[kind+'CommitMatches']=close(committed,expected)&&close(preview,committed);result.checks[kind+'TransientOnly']=before.generation===during.generation;result.checks[kind+'OneCommit']=BigInt(after.lastCommit.ordinal)===BigInt(before.lastCommit.ordinal)+1n;
  await page.evaluate(()=>sdk.undo());await pause();result.checks[kind+'UndoRestoresInk']=(await pixel()).slice(0,3).every(v=>v<30);
  const cancelBefore=await state();await page.mouse.move(250,350);await page.mouse.down();await page.mouse.move(750,350,{steps:4});await pause();await page.evaluate(()=>sdk.canvas.dispatchEvent(new PointerEvent('pointercancel',{pointerId:1,pointerType:'mouse',bubbles:true})));await page.mouse.up();await pause();result.checks[kind+'CancelRestoresWithoutCommit']=(await state()).generation===cancelBefore.generation&&(await pixel()).slice(0,3).every(v=>v<30);
 }
 result.status=Object.values(result.checks).every(Boolean)&&!result.errors.length?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify(result));}
