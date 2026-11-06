// Public SDK + real Chrome input. Compare transient and committed erase silhouettes.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import os from 'node:os';
import path from 'node:path';
import {createHash} from 'node:crypto';

const sdk=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-erase-preview-'));
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}</style><div id="board"></div>
<script type="module">import{createRuntime}from'./axiom.mjs';window.probe={sdk:await createRuntime({mount:document.getElementById('board'),documentId:'5a000000000000000000000000000031',permission:'localSingleUser',builtinFonts:false})};</script>`;
const result={scope:'Chrome WebGL2, root SDK, synthetic mouse; compare opaque interior pixels, excluding antialiasing differences',cases:[],errors:[],sha256:{}};
let server,browser;
try {
 await fs.cp(sdk,root,{recursive:true});await fs.writeFile(path.join(root,'index.html'),html);
 for(const name of ['axiom.mjs','axiom.wasm','axiom.data','internal/axiom.mjs'])result.sha256[name]=createHash('sha256').update(await fs.readFile(path.join(root,name))).digest('hex');
 server=http.createServer(async(req,res)=>{try{
  const p=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname.replace(/\/$/,'/index.html'));
  if(!p.startsWith(root+path.sep))throw Error('path');
  res.setHeader('Content-Type',p.endsWith('.wasm')?'application/wasm':p.endsWith('.mjs')?'text/javascript':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));
 }catch{res.statusCode=404;res.end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});result.browser=browser.version();
 await fs.mkdir(path.dirname(output),{recursive:true});
 for(const dpr of [1,2])for(const [name,points,zoom] of [
  ['straight',[[380,350],[620,350]],1],
  ['bend',[[380,330],[460,330],[460,385],[600,385]],1],
  ['tap',[[500,350]],1],
  ['zoom',[[380,350],[620,350]],1.5],
 ]){
  const context=await browser.newContext({viewport:{width:1000,height:700},deviceScaleFactor:dpr});
  const page=await context.newPage();page.on('pageerror',e=>result.errors.push(String(e)));
  await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.probe?.sdk);
  const frames=()=>page.evaluate(()=>new Promise(r=>requestAnimationFrame(()=>requestAnimationFrame(r))));
  const settle=async()=>{await page.waitForFunction(()=>{const s=probe.sdk.readState();return !s.activeContacts&&s.lastCommit.presentedGeneration===s.generation;});await frames();};
  await page.evaluate(async()=>{await probe.sdk.selectBrush('round-handdrawn',4);await probe.sdk.setBrushConfig({...probe.sdk.readBrushConfig(),size:180,thinning:0,streamline:0,smoothing:0,startTaper:0,endTaper:0,color:[0,0,0,1]});});
  await page.mouse.move(100,350);await page.mouse.down();await page.mouse.move(900,350,{steps:20});await page.mouse.up();await settle();
  if(zoom!==1){await page.evaluate(z=>probe.sdk.zoomAt(500,350,z),zoom);await frames();}
  await page.evaluate(()=>probe.sdk.setEraser({mode:2,diameter:48}));await frames();
  const before=await page.evaluate(()=>probe.sdk.readState());
  await page.mouse.move(...points[0]);await page.mouse.down();for(const p of points.slice(1))await page.mouse.move(...p,{steps:6});await frames();
  const during=await page.evaluate(()=>probe.sdk.readState());
  const preview=await page.screenshot();await page.mouse.up();await settle();const committed=await page.screenshot();
  const pixels=await page.evaluate(async({a,b,dpr})=>{
   async function decode(bytes){const im=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));const c=document.createElement('canvas');c.width=im.width;c.height=im.height;const g=c.getContext('2d');g.drawImage(im,0,0);im.close();return {width:c.width,data:g.getImageData(0,0,c.width,c.height).data};}
   const p=await decode(a),q=await decode(b);let mismatchInterior=0,white=0;
   for(let y=285*dpr;y<430*dpr;y++)for(let x=320*dpr;x<680*dpr;x++){
    const i=(y*p.width+x)*4,first=p.data[i]+p.data[i+1]+p.data[i+2],last=q.data[i]+q.data[i+1]+q.data[i+2];
    if((first<96&&last>720)||(last<96&&first>720))++mismatchInterior;if(last>720)++white;
   }
   return {mismatchInterior,white};
  },{a:[...preview],b:[...committed],dpr});
  const after=await page.evaluate(()=>probe.sdk.readState());await page.evaluate(()=>probe.sdk.undo());await settle();const undone=await page.screenshot();
  await page.evaluate(()=>probe.sdk.redo());await settle();const redone=await page.screenshot();
  const row={name,dpr,zoom,...pixels,previewIsTransient:during.generation===before.generation,oneCommit:BigInt(after.lastCommit.ordinal)===BigInt(before.lastCommit.ordinal)+1n,undoChanged:!undone.equals(committed),redoRestores:committed.equals(redone),objectPreserved:after.objects===before.objects};
  result.cases.push(row);
  const prefix=output.replace(/\.json$/,'')+'-'+name+'-dpr'+dpr;await fs.writeFile(prefix+'-preview.png',preview);await fs.writeFile(prefix+'-committed.png',committed);
  await page.evaluate(()=>probe.sdk.close());await context.close();
 }
 result.status=result.errors.length===0&&result.cases.every(r=>r.mismatchInterior===0&&r.white>100&&r.previewIsTransient&&r.oneCommit&&r.undoChanged&&r.redoRestores&&r.objectPreserved)?'PASS':'FAIL';
 if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}
finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify(result));}
