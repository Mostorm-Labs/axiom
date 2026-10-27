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
const output=path.resolve(root,options.output??'out/selection-batch2/browser');
if(!options.chrome)throw new Error('--chrome must name the locally verified Chrome executable');
if(!options.python)throw new Error('--python must name Python with Pillow for independent screenshot pixel checks');
await fs.mkdir(output,{recursive:false});
const profile=path.resolve(root,`out/selection-batch1/chrome-batch2-${Date.now()}`);await fs.mkdir(profile,{recursive:true});
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
  const url=`http://127.0.0.1:${server.address().port}/index.html?rev=selection-batch2`;
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
  for(const [preset,l,r] of [[0,100,180],[1,260,340],[2,420,500]]){await control('shape',preset);await drag(l,100,r,160);}
  let s=await state();await check(s.objects===3,'three real Shape creations',s);
  const baselinePng=await page.send('Page.captureScreenshot',{format:'png'});
  await fs.writeFile(path.join(output,'unselected-baseline.png'),Buffer.from(baselinePng.data,'base64'));
  const operationCount=s.operations;
  await control('selection',0);await mouse('mousePressed',140,130);await mouse('mouseReleased',140,130);
  s=await state();await check(s.selectedCount===1,'point selection',s);
  await mouse('mousePressed',20,20);s=await state();await check(s.regionActive&&s.selectedCount===1,'blank Down retains formal selection',s);
  await mouse('mouseMoved',345,170);s=await state();await check(s.selectedCount===1&&s.regionCandidates===2,'Move only publishes candidates',s);
  const regionPng=await page.send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.join(output,'rectangle-preview.png'),Buffer.from(regionPng.data,'base64'));
  await mouse('mouseReleased',345,170);s=await state();await check(!s.regionActive&&s.selectedCount===2,'Up commits rectangle once',s);
  await drag(410,90,510,170,true);s=await state();await check(s.selectedCount===3,'Shift-at-Down appends triangle',s);
  await drag(410,90,510,170,true);s=await state();await check(s.selectedCount===3,'Add does not toggle an existing root',s);
  const multiPng=await page.send('Page.captureScreenshot',{format:'png'});await fs.writeFile(path.join(output,'multiple-roots.png'),Buffer.from(multiPng.data,'base64'));
  const roi=await evaluate(`(()=>{const r=document.getElementById('axiom-canvas').getBoundingClientRect();return [Math.round(r.left+276),Math.round(r.top+115),Math.round(r.left+324),Math.round(r.top+145)];})()`);
  const pixelObservation=JSON.parse(execFileSync(options.python,['-X','utf8','-c',`import json,sys
from PIL import Image
roi=tuple(map(int,sys.argv[3:]))
def dark(p): return p[0]<50 and p[1]<100 and p[2]<150
base=list(Image.open(sys.argv[1]).convert('RGB').crop(roi).getdata())
actual=list(Image.open(sys.argv[2]).convert('RGB').crop(roi).getdata())
print(json.dumps({'baseDark':sum(map(dark,base)),'actualDark':sum(map(dark,actual)),'roi':roi}))`,path.join(output,'unselected-baseline.png'),path.join(output,'multiple-roots.png'),...roi.map(String)],{encoding:'utf8'}));
  await check(pixelObservation.baseDark===0,'pixel fixture has a clean ellipse interior',pixelObservation);
  await check(pixelObservation.actualDark===0,'multiple selection has no primary-only label editing affordance',pixelObservation);
  await control('selection',1);await drag(120,120,330,155);s=await state();await check(s.selectedCount===2&&s.operations===operationCount,'explicit region over object does not move it',s);
  await control('selection',2);await mouse('mousePressed',90,90);
  for(const [x,y] of [[345,90],[345,95],[185,95],[185,180],[90,180]])await mouse('mouseMoved',x,y);
  await mouse('mouseReleased',90,90);s=await state();await check(s.selectedCount===1,'concave freeform excludes ellipse in notch',s);
  await mouse('mousePressed',20,20);await mouse('mouseMoved',550,200);
  await evaluate(`window.dispatchEvent(new Event('blur'));true`);await flush();s=await state();await check(!s.regionActive&&s.selectedCount===1,'focus cancellation preserves formal selection',s);
  await mouse('mouseReleased',550,200);s=await state();await check(s.selectedCount===1,'late Up cannot publish cancelled region',s);
  await control('tool',3);s=await state();await check(s.selectedCount===1&&s.tool===3,'Hand preserves selection',s);
  await control('selection',0);s=await state();await check(s.selectedCount===1&&s.selectionMode===0,'Select returns with formal selection',s);
  await control('selection',1);await drag(260,100,264,104);s=await state();await check(s.selectedCount===0,'ellipse AABB corner is not precise geometry',s);
  await drag(420,100,424,104);s=await state();await check(s.selectedCount===0,'triangle AABB corner is not precise geometry',s);
  await evaluate(`(async()=>{axiomMinimalHost.resize(900,600,2);await axiomMinimalHost.flush();})()`);
  await drag(90,90,185,165);s=await state();await check(s.selectedCount===1&&s.dpr===2,'same logical region at DPR 2',s);
  await check(s.operations===operationCount&&s.objects===3&&!s.failure&&!s.error,'selection leaves canonical operations and objects unchanged',s);

  const sectionSnapshot=()=>evaluate(`(()=>{const h=axiomMinimalHost;return JSON.parse(h.module.UTF8ToString(h.module._axiom_web_section_snapshot(h.handle,0)));})()`);
  let sectionRequest=0;
  const section=async(action,fields)=>{
    const input={target:(await sectionSnapshot()).target,clientId:'6008',requestId:String(++sectionRequest),action,...fields};
    const queued=await evaluate(`(()=>{const h=axiomMinimalHost,p=h.module.stringToNewUTF8(${JSON.stringify(JSON.stringify(input))});try{return JSON.parse(h.module.UTF8ToString(h.module._axiom_web_section_intent(h.handle,p)));}finally{h.module._free(p);}})()`);
    if(queued.transportError)throw new Error(JSON.stringify(queued));await flush();
    const receipt=await evaluate(`(()=>{const h=axiomMinimalHost,a=h.module.stringToNewUTF8('6008'),b=h.module.stringToNewUTF8('${sectionRequest}');try{return JSON.parse(h.module.UTF8ToString(h.module._axiom_web_section_receipt(h.handle,a,b)));}finally{h.module._free(a);h.module._free(b);}})()`);
    if(receipt.transportError||receipt.section?.failure!==0)throw new Error('Section receipt '+JSON.stringify(receipt));
    return receipt;
  };
  await control('selection',0);await mouse('mousePressed',140,130);await mouse('mouseReleased',140,130);
  const shapeA=(await sectionSnapshot()).selectedRoots[0];
  const sectionId='e9ff0000000000000000000000000001';
  await section(0,{objectId:sectionId,boundary:[40,40,520,200],title:'Selection batch2'});
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
