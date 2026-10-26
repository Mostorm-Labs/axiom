// Actual Chrome executes the actual Minimal Host/WASM. Node transports CDP
// messages and immutable artifact bytes only; it never emulates the Runtime.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {spawn} from 'node:child_process';
import {createHash} from 'node:crypto';
const opts=Object.fromEntries(process.argv.slice(2).reduce((a,v,i,all)=>i%2?a:[...a,[v.replace(/^--/,''),all[i+1]]],[]));
const root=path.resolve(opts.repo??'.');
const build=path.resolve(root,opts.build??'out/fc08-section-de/web/apps/axiom_canvas_demo');
const out=path.resolve(root,opts.output??'.aegis/results/FC-08-SECTION-DE/browser-001');
await fs.mkdir(out,{recursive:false});
const profile=path.resolve(root,'out/fc08-section-de',`chrome-${Date.now()}`);
await fs.mkdir(profile,{recursive:true});
const hash=b=>createHash('sha256').update(b).digest('hex');
const delay=ms=>new Promise(resolve=>setTimeout(resolve,ms));
class CDP {
  constructor(ws){this.ws=ws;this.next=0;this.pending=new Map();this.events=[];
    ws.addEventListener('message',event=>{const m=JSON.parse(event.data);if(!m.id){this.events.push(m);return;}
      const p=this.pending.get(m.id);if(!p)return;this.pending.delete(m.id);clearTimeout(p.timer);
      m.error?p.reject(new Error(JSON.stringify(m.error))):p.resolve(m.result);});}
  static async connect(url){const ws=new WebSocket(url);await new Promise((resolve,reject)=>{ws.addEventListener('open',resolve,{once:true});ws.addEventListener('error',reject,{once:true});});return new CDP(ws);}
  send(method,params={}){const id=++this.next;return new Promise((resolve,reject)=>{const timer=setTimeout(()=>reject(new Error('CDP timeout '+method)),90000);this.pending.set(id,{resolve,reject,timer});this.ws.send(JSON.stringify({id,method,params}));});}
}
let server,browser,page;const journal=[];let failed=false;
try {
  server=http.createServer(async(req,res)=>{try{
    const url=new URL(req.url,'http://127.0.0.1');
    const file=path.resolve(build,'.'+decodeURIComponent(url.pathname==='/'?'/index.html':url.pathname));
    if(!file.startsWith(build+path.sep))throw new Error('outside build');
    const bytes=await fs.readFile(file);res.setHeader('Content-Type',file.endsWith('.wasm')?'application/wasm':file.endsWith('.js')?'text/javascript':file.endsWith('.html')?'text/html':'application/octet-stream');res.end(bytes);
  }catch{res.statusCode=404;res.end();}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const url=`http://127.0.0.1:${server.address().port}/index.html?rev=fc08-section-de`;
  const args=['--headless=new','--no-first-run','--no-default-browser-check','--enable-unsafe-swiftshader','--use-angle=swiftshader','--remote-debugging-port=0',`--user-data-dir=${profile}`,'about:blank'];
  const chrome=opts.chrome??'C:/Program Files/Google/Chrome/Application/chrome.exe';
  browser=spawn(chrome,args,{windowsHide:true,stdio:['ignore','pipe','pipe']});let stderr='';browser.stderr.on('data',chunk=>stderr+=chunk);
  let port;
  for(let i=0;i<300&&!port;i++){try{port=Number((await fs.readFile(path.join(profile,'DevToolsActivePort'),'utf8')).split('\n')[0]);}catch{await delay(100);}}
  if(!port)throw new Error('Chrome remote debugging not ready: '+stderr);
  const pages=await(await fetch(`http://127.0.0.1:${port}/json/list`)).json();
  page=await CDP.connect(pages.find(p=>p.type==='page').webSocketDebuggerUrl);
  await page.send('Runtime.enable');await page.send('Page.enable');
  journal.push({chrome,args,url,version:await page.send('Browser.getVersion')});
  await page.send('Page.navigate',{url});
  const evaluate=async(expression)=>{
    const result=await page.send('Runtime.evaluate',{expression,awaitPromise:true,returnByValue:true});
    if(result.exceptionDetails)throw new Error(JSON.stringify(result.exceptionDetails));return result.result.value;
  };
  await evaluate(`(async()=>{for(let i=0;i<600&&!globalThis.axiomMinimalReady;i++)await new Promise(r=>setTimeout(r,50));await globalThis.axiomMinimalReady;return !!globalThis.axiomMinimalHost;})()`);
  const observed=await evaluate(`(async()=>{
    const host=await globalThis.axiomMinimalReady,m=host.module,h=host.handle,rows=[];
    const check=(ok,name,actual)=>{rows.push({name,ok,actual});if(!ok)throw new Error(name+': '+JSON.stringify(actual));};
    const call=(name,...values)=>{const pointers=values.map(value=>m.stringToNewUTF8(value));try{return JSON.parse(m.UTF8ToString(m[name](h,...pointers)));}finally{pointers.forEach(p=>m._free(p));}};
    const snapshot=id=>call('_axiom_web_section_snapshot',id??'');
    let n=0;const clientId='880099';const target=snapshot().target;
    const command=async(value,expectedFailure=0)=>{
      const request={target,clientId,requestId:String(++n),...value};const queued=call('_axiom_web_section_intent',JSON.stringify(request));
      check(queued.state===0,'queued '+n,queued);await host.flush();
      const receipt=call('_axiom_web_section_receipt',clientId,String(n));
      check(receipt.section?.failure===expectedFailure,'terminal '+n,receipt);
      if(expectedFailure===0)check(receipt.state===1,'applied '+n,receipt);
      return {request,receipt};
    };
    const a='01000000000000000000000000fc0801',b='02000000000000000000000000fc0801',w='03000000000000000000000000fc0801';
    const malformed=call('_axiom_web_section_intent',JSON.stringify({target,clientId,requestId:'999',action:0,objectId:'00'}));
    check(!!malformed.transportError,'malformed ID rejected without abort',malformed);
    await command({action:0,objectId:a,title:'区域 A',boundary:[0,0,120,100],transform:[1,0,0,1,30,40]});
    await command({action:0,objectId:b,title:'Region B',transform:[0,2,-2,0,450,300]});
    const aBefore=snapshot(a);check(aBefore.titleLayoutReady&&aBefore.titleGlyphCount>0&&aBefore.titleDiagnostics.length===0,'real registered-font title glyphs',aBefore);check(aBefore.available&&aBefore.projectionReady,'create projection',aBefore);
    await command({action:1,objectId:w,objects:[a,b]});const wrapped=snapshot(w);
    check(wrapped.members.length===2,'wrapper children',wrapped);
    check(JSON.stringify(snapshot(a).worldTransform)===JSON.stringify(aBefore.worldTransform),'wrap world pose',snapshot(a));
    await command({action:5,objectId:w,title:'Section 世界'});check(snapshot(w).title==='Section 世界','UTF8 title',snapshot(w));
    await command({action:4,objectId:w,boundary:[-20,-30,160,120]});
    check(JSON.stringify(snapshot(a).worldTransform)===JSON.stringify(aBefore.worldTransform),'boundary leaves child pose',snapshot(a));
    await command({action:2,objectId:w});check(!snapshot(w).available&&host.observation().objects===2,'release preserves children',host.observation());
    const reparent=await command({action:3,objects:[a],parentId:b});
    const moved=snapshot(a);check(JSON.stringify(moved.worldTransform)===JSON.stringify(aBefore.worldTransform),'rotated reparent world',moved);
    check(JSON.stringify(moved.localTransform)===JSON.stringify([0,-.5,.5,0,-130,210]),'independent inverse matrix',moved.localTransform);
    const generation=snapshot().generation;
    const duplicate=call('_axiom_web_section_intent',JSON.stringify(reparent.request));await host.flush();
    check(duplicate.state===1&&snapshot().generation===generation,'duplicate no mutation',duplicate);
    await command({action:5,objectId:a,title:'stale',generation:String(Number(generation)+100)},3);
    check(snapshot().generation===generation,'stale zero mutation',snapshot());
    await host.control('undo');check(JSON.stringify(snapshot(a).worldTransform)===JSON.stringify(aBefore.worldTransform),'undo world',snapshot(a));
    await host.control('redo');check(snapshot(b).children.includes(a),'redo topology',snapshot(b));
    await command({action:6,objectId:b});await host.control(14,0,2,-2,0,480,340);
    const followed=snapshot(a);check(followed.worldTransform[4]===60&&followed.worldTransform[5]===80,'ancestor moves descendants once',followed);
    await host.control('fit');await host.control('tool',1);check(!host.observation().failure,'render/fit/tool remains live',host.observation());
    return {rows,final:host.observation(),section:snapshot(b),physicalInput:'NOT_RUN'};
  })()`);
  await fs.writeFile(path.join(out,'observations.json'),JSON.stringify(observed,null,2)+'\n');
  await fs.writeFile(path.join(out,'screenshot.png'),Buffer.from((await page.send('Page.captureScreenshot',{format:'png'})).data,'base64'));
}catch(error){failed=true;await fs.writeFile(path.join(out,'failure.json'),JSON.stringify({error:String(error),stack:error.stack},null,2)+'\n');}
finally{
  if(page){await fs.writeFile(path.join(out,'browser-events.json'),JSON.stringify(page.events,null,2)+'\n');page.ws.close();}
  browser?.kill();await new Promise(resolve=>server?server.close(resolve):resolve());
  const artifacts=[];for(const name of await fs.readdir(build))if(/\.(wasm|js|data|html)$/.test(name)){const bytes=await fs.readFile(path.join(build,name));artifacts.push({path:name,sha256:hash(bytes),bytes:bytes.length});}
  await fs.writeFile(path.join(out,'invocation.json'),JSON.stringify({journal,exit_code:failed?1:0,artifacts},null,2)+'\n');
}
console.log(JSON.stringify({output:out,exit_code:failed?1:0}));process.exitCode=failed?1:0;
