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

    if (${opts.drop==='1'}) {
      await command({action:2,objectId:b});await command({action:2,objectId:a});
      check(host.observation().objects===0,'drop fixture starts empty',host.observation());
      await host.control('setZoom',0,0,1);
      const camera=host.observation().camera;await host.control('pan',-camera.x,-camera.y);
      let timestamp=BigInt(Math.ceil(performance.now()*1000000))+1000000n,generation=0n;
      const point=async(phase,x,y)=>{
        const camera=host.observation().camera;
        const accepted=m._axiom_web_pointer(h,1,71,phase===0?0n:generation,++timestamp,phase,
          x*camera.scale+camera.x,y*camera.scale+camera.y,0,0,0,0,0,0,0);
        check(accepted===1,'confirmed pointer '+phase,{x,y,phase,accepted});
        if(phase===0)generation=m._axiom_web_pointer_generation(h,1,71);
        if(phase===2||phase===3)generation=0n;
        await host.flush();
      };
      const click=async(x,y)=>{await point(0,x,y);await point(2,x,y);};
      const drag=async(x,y,tx,ty)=>{await point(0,x,y);await point(1,tx,ty);await point(2,tx,ty);};
      for(const ink of [false,true]) {
        const targetId='f'.repeat(30)+(ink?'02':'01');
        await command({action:0,objectId:targetId,boundary:[0,0,240,220],transform:[1,0,0,1,300,200]});
        await host.control(ink?'tool':'shape',0);
        await drag(20,40,100,100);await host.control('tool',1);await click(60,70);
        const before=BigInt(snapshot(targetId).generation);
        await point(0,60,70);await point(1,390,290);
        check(snapshot(targetId).children.length===0&&BigInt(snapshot(targetId).generation)===before,'crossing is transient '+ink,snapshot(targetId));
        await point(2,390,290);let target=snapshot(targetId);
        check(target.children.length===1&&BigInt(target.generation)===before+1n,'one atomic membership/pose drop '+ink,target);
        const actor=target.children[0];
        // Real brush samples/outline already use content-world coordinates; its initial matrix is identity.
        // Shape creation starts at transform (20,40). Both displays translate by (330,220).
        const pose=target.members[0].worldTransform;
        check(Math.abs(pose[4]-(ink?330:350))<.001&&Math.abs(pose[5]-(ink?220:260))<.001,'last confirmed preview pose '+ink,pose);
        await host.control('undo');check(snapshot(targetId).children.length===0,'one-step drop undo '+ink,snapshot(targetId));
        await host.control('redo');check(snapshot(targetId).children.includes(actor),'one-step drop redo '+ink,snapshot(targetId));
        await command({action:6,objectId:targetId});await drag(310,210,340,250);target=snapshot(targetId);
        check(Math.abs(target.members[0].worldTransform[4]-(ink?360:380))<.001&&Math.abs(target.members[0].worldTransform[5]-(ink?260:300))<.001,'Section pointer drag carries member once '+ink,target);
        await click(420,330);const preExit=BigInt(snapshot(targetId).generation);await drag(420,330,100,120);
        check(snapshot(targetId).children.length===0&&BigInt(snapshot(targetId).generation)===preExit+1n,'drop out is one commit '+ink,snapshot(targetId));
        await host.control('undo');check(snapshot(targetId).children.includes(actor),'drop out undo restores membership '+ink,snapshot(targetId));
        for(let i=0;i<12&&host.observation().objects>0;i++)await host.control('undo');
        check(host.observation().objects===0,'bounded fixture undo cleanup '+ink,host.observation());
      }
    }
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
