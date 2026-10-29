// Actual Chrome -> DOM PointerEvents -> production WASM Minimal Host.
// CDP supplies synthetic mouse facts, not physical-device acceptance.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {spawn,execFileSync} from 'node:child_process';
import {createHash} from 'node:crypto';
const options=Object.fromEntries(process.argv.slice(2).reduce((a,v,i,all)=>i%2?a:[...a,[v.replace(/^--/,''),all[i+1]]],[]));
const root=path.resolve(options.repo??'.');
const build=path.resolve(root,options.build??'out/selection-batch1/web/apps/axiom_canvas_demo');
const output=path.resolve(root,options.output??'out/selection-batch3/browser');
if(!options.chrome)throw new Error('--chrome must name the locally verified Chrome executable');
if(!options.python)throw new Error('--python must name Python with Pillow for independent screenshot pixel checks');
await fs.mkdir(output,{recursive:false});
const profile=path.resolve(root,`out/selection-batch1/chrome-batch3-${Date.now()}`);await fs.mkdir(profile,{recursive:true});
const hash=b=>createHash('sha256').update(b).digest('hex');
const delay=ms=>new Promise(r=>setTimeout(r,ms));
class CDP {
  constructor(ws){this.ws=ws;this.next=0;this.pending=new Map();this.events=[];
    ws.addEventListener('message',e=>{const m=JSON.parse(e.data);if(!m.id){this.events.push(m);return;}
      const p=this.pending.get(m.id);if(!p)return;this.pending.delete(m.id);clearTimeout(p.timer);
      m.error?p.reject(new Error(JSON.stringify(m.error))):p.resolve(m.result);});}
  static async connect(url){const ws=new WebSocket(url);await new Promise((r,j)=>{ws.addEventListener('open',r,{once:true});ws.addEventListener('error',j,{once:true});});return new CDP(ws);}
  send(method,params={}){const id=++this.next;return new Promise((resolve,reject)=>{
    const timer=setTimeout(()=>reject(new Error('CDP timeout '+method)),60000);this.pending.set(id,{resolve,reject,timer});this.ws.send(JSON.stringify({id,method,params}));});}
}
const rows=[];let server,browser,page,error='';
const result={execution:'actual Chrome/WASM with CDP mouse delivery; synthetic facts, no PHYS claim',commands:process.argv,rows};
try {
  server=http.createServer(async(req,res)=>{try{
    const url=new URL(req.url,'http://127.0.0.1');const file=path.resolve(build,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
    if(!file.startsWith(build+path.sep))throw new Error('outside build');
    const bytes=await fs.readFile(file);res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':file.endsWith('.js')?'text/javascript':file.endsWith('.html')?'text/html':'application/octet-stream');res.end(bytes);
  }catch{res.statusCode=404;res.end();}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  const url=`http://127.0.0.1:${server.address().port}/index.html?rev=selection-batch3`;
  const args=['--headless=new','--no-first-run','--no-default-browser-check','--enable-unsafe-swiftshader','--use-angle=swiftshader','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  result.browser={executable:options.chrome,args,url};
  browser=spawn(options.chrome,args,{windowsHide:true,stdio:['ignore','pipe','pipe']});let stderr='';browser.stderr.on('data',b=>stderr+=b);
  let port;
  for(let i=0;i<300&&!port;++i){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);}catch{await delay(100);}}
  if(!port)throw new Error('Chrome unavailable '+stderr);
  const pages=await(await fetch(`http://127.0.0.1:${port}/json/list`)).json();page=await CDP.connect(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
  await page.send('Runtime.enable');await page.send('Page.enable');
  await page.send('Emulation.setDeviceMetricsOverride',{width:1100,height:850,deviceScaleFactor:1,mobile:false});
  result.browser.version=await page.send('Browser.getVersion');
  const evaluate=async expression=>{const r=await page.send('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});if(r.exceptionDetails)throw new Error(JSON.stringify(r.exceptionDetails));return r.result.value;};
  await page.send('Page.navigate',{url});
  await evaluate(`(async()=>{for(let i=0;i<600&&!globalThis.axiomMinimalReady;++i)await new Promise(r=>setTimeout(r,50));await globalThis.axiomMinimalReady;return true;})()`);
  await evaluate(`(async()=>{const h=await axiomMinimalReady;h.resize(900,600,1);await h.flush();})()`);
  await evaluate(`(()=>{globalThis.selectionDomEvents=[];for(const name of ['pointerdown','pointermove','pointerup','pointercancel','lostpointercapture'])document.addEventListener(name,e=>{selectionDomEvents.push({type:e.type,id:e.pointerId,buttons:e.buttons,button:e.button,target:e.target.id,x:e.clientX,y:e.clientY});if(selectionDomEvents.length>256)selectionDomEvents.shift();},true);})()`);
  const state=()=>evaluate('({...axiomMinimalHost.observation(),domEvents:selectionDomEvents.slice()})');
  const flush=()=>evaluate('axiomMinimalHost.flush().then(()=>true)');
  const control=(kind,...values)=>evaluate(`axiomMinimalHost.control(${JSON.stringify(kind)},${values.map(x=>JSON.stringify(x)).join(',')})`);
  const check=async(ok,name,actual)=>{rows.push({name,ok,actual});if(!ok)throw new Error(name+': '+JSON.stringify(actual));};
  const mouse=async(type,x,y,modifiers=0)=>{
    const p=await evaluate(`(()=>{const s=axiomMinimalHost.observation(),r=document.getElementById('axiom-canvas').getBoundingClientRect();return {x:r.left+(${x}*s.camera.scale+s.camera.x)*r.width/s.logicalWidth,y:r.top+(${y}*s.camera.scale+s.camera.y)*r.height/s.logicalHeight};})()`);
    await page.send('Input.dispatchMouseEvent',{type,x:p.x,y:p.y,button:'left',buttons:type==='mouseReleased'?0:1,clickCount:type==='mouseMoved'?0:1,modifiers});await flush();
  };
  const drag=async(x,y,r,b,shift=false)=>{await mouse('mousePressed',x,y,shift?8:0);await mouse('mouseMoved',r,b);await mouse('mouseReleased',r,b);};
  const initial=await state();result.initial=initial;
  await check(initial.selectionSummary?.count===0&&initial.selectionSummary.status===1,'empty public selection summary is available',initial);
  await check(initial.selectionSummary.properties.every(p=>p.state===0&&p.value===null),'empty properties never become uniform',initial.selectionSummary.properties);
  for(const [preset,l,r] of [[0,100,180],[1,260,340],[2,420,500]]){await control('shape',preset);await drag(l,100,r,160);}
  let s=await state();await check(s.objects===3,'three real Shape creations',s);
  const baselinePng=await page.send('Page.captureScreenshot',{format:'png'});
  await fs.writeFile(path.join(output,'unselected-baseline.png'),Buffer.from(baselinePng.data,'base64'));
  let operationCount=s.operations;
  await control('selection',0);await mouse('mousePressed',140,130);await mouse('mouseReleased',140,130);
  s=await state();await check(s.selectedCount===1,'point selection',s);
  await mouse('mousePressed',20,20);s=await state();await check(s.regionActive&&s.selectedCount===1,'blank Down retains formal selection',s);
  await mouse('mouseMoved',345,170);s=await state();await check(s.selectedCount===1&&s.regionCandidates===2,'Move only publishes candidates',s);
  const regionPng=await page.send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.join(output,'rectangle-preview.png'),Buffer.from(regionPng.data,'base64'));
  await mouse('mouseReleased',345,170);s=await state();await check(!s.regionActive&&s.selectedCount===2,'Up commits rectangle once',s);
  await check(s.selectionSummary.count===2&&s.selectionSummary.kindCounts.reduce((n,k)=>n+k.count,0)===2&&s.selectionSummary.roots.includes(s.selectionSummary.primary),'normalized public roots kinds and primary agree',s.selectionSummary);
  await check(s.selectionSummary.bounds.status===1&&s.selectionSummary.sceneGeneration===s.selectionSummary.semanticGeneration,'published bounds and summary generations agree',s.selectionSummary);
  await check(s.selectionSummary.properties.find(p=>p.fieldId===3)?.state===1&&s.selectionSummary.properties.find(p=>p.fieldId===3)?.value===1,'accepted Shape opacity default aggregates uniformly',s.selectionSummary.properties);
  await evaluate(`(async()=>{await axiomMinimalHost.toggleDebugUi();await axiomMinimalHost.flush();})()`);
  await delay(400);
  const originalBounds=s.selectionSummary.bounds.world.slice();
  const debugRect=await evaluate(`(()=>{const r=document.getElementById('debugUi').getBoundingClientRect();return {x:r.left,y:r.top};})()`);
  const debugClick=async(dx,dy)=>{for(const type of ['mousePressed','mouseReleased'])await page.send('Input.dispatchMouseEvent',{type,x:debugRect.x+dx,y:debugRect.y+dy,button:'left',buttons:type==='mousePressed'?1:0,clickCount:1});await flush();};
  await page.send('Input.dispatchMouseEvent',{type:'mouseMoved',x:debugRect.x+250,y:debugRect.y+480,button:'none',buttons:0});await flush();
  await page.send('Input.dispatchMouseEvent',{type:'mouseWheel',x:debugRect.x+250,y:debugRect.y+480,deltaX:0,deltaY:650});await delay(200);await flush();
  await page.send('Input.dispatchMouseEvent',{type:'mouseWheel',x:debugRect.x+250,y:debugRect.y+480,deltaX:0,deltaY:650});await delay(200);await flush();
  await debugClick(65,384);await delay(200);s=await state();await check(s.objects===4&&s.operations===operationCount+1&&s.selectionSummary.count===1&&s.selectionSummary.kindCounts[0].kind===9,'web debug Group commits one Group and selects it',s);
  await page.send('Input.dispatchMouseEvent',{type:'mouseWheel',x:debugRect.x+250,y:debugRect.y+480,deltaX:0,deltaY:-5000});await delay(200);await flush();
  const pre=await page.send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.join(output,'before-ungroup.png'),Buffer.from(pre.data,'base64'));
  await debugClick(190,506);await delay(200);s=await state();await check(s.objects===3&&s.operations===operationCount+2&&s.selectionSummary.count===2,'web debug Ungroup releases roots',s);
  await check(JSON.stringify(s.selectionSummary.bounds.world)===JSON.stringify(originalBounds),'ungroup retains selection bounds',s.selectionSummary.bounds);
  await control('undo');s=await state();await check(s.objects===4,'web Undo restores Group',s);
  await control('redo');s=await state();await check(s.objects===3,'web Redo releases Group',s);
  const pic=await page.send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.join(output,'debug.png'),Buffer.from(pic.data,'base64'));
  s=await state();result.final=s;
  result.artifacts=[];for(const name of ['axiom_canvas_demo_web.js','axiom_canvas_demo_web.wasm','host.js']){const bytes=await fs.readFile(path.join(build,name));result.artifacts.push({name,sha256:hash(bytes),bytes:bytes.length});}
}catch(e){error=String(e.stack??e);result.error=error;}
finally {
  result.exitCode=error?1:0;result.assertions=rows.length;result.failed=rows.filter(r=>!r.ok).length;
  result.source={head:execFileSync(options.git??'git',['rev-parse','HEAD'],{cwd:root,encoding:'utf8'}).trim()};
  if(page){await fs.writeFile(path.join(output,'cdp-events.json'),JSON.stringify(page.events,null,2));try{await page.send('Browser.close');}catch{}page.ws.close();}
  browser?.kill();if(server)await new Promise(r=>server.close(r));
  await fs.writeFile(path.join(output,'results.json'),JSON.stringify(result,null,2)+'\n');
  process.stdout.write(JSON.stringify({exitCode:result.exitCode,assertions:result.assertions,failed:result.failed,error:error.slice(0,250),output})+'\n');
}
process.exitCode=error?1:0;
