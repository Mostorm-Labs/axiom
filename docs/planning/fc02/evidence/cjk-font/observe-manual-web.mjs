// User-operated Chrome only. This observer never dispatches input or controls Runtime.
import fs from 'node:fs';
import path from 'node:path';
import {spawn} from 'node:child_process';

const directory=path.resolve('out/fc02-font/physical');
fs.mkdirSync(directory,{recursive:true});
const profile=path.join(directory,'chrome-profile');
fs.mkdirSync(profile,{recursive:true});
const url='http://127.0.0.1:8769/index.html';
const chrome=spawn('C:/Program Files/Google/Chrome/Application/chrome.exe',[
  '--no-first-run','--no-default-browser-check',`--user-data-dir=${profile}`,
  '--remote-debugging-address=127.0.0.1','--remote-debugging-port=0','--new-window',url
],{windowsHide:false,stdio:'ignore'});
const stamp=new Date().toISOString().replace(/[:.]/g,'-');
const log=path.join(directory,`web-manual-${stamp}.jsonl`);
const append=value=>fs.appendFileSync(log,JSON.stringify({utc:new Date().toISOString(),...value})+'\n');
let port,browser,page;
for(let i=0;i<100;i++){
  try{
    port=fs.readFileSync(path.join(profile,'DevToolsActivePort'),'utf8').split('\n')[0];
    browser=await fetch(`http://127.0.0.1:${port}/json/version`).then(r=>r.json());
    page=(await fetch(`http://127.0.0.1:${port}/json/list`).then(r=>r.json())).find(p=>p.type==='page'&&p.url.startsWith(url));
    if(page)break;
  }catch{}
  await new Promise(r=>setTimeout(r,200));
}
if(!page)throw new Error('Owned Chrome page did not start');
const socket=new WebSocket(page.webSocketDebuggerUrl);
await new Promise(resolve=>socket.addEventListener('open',resolve,{once:true}));
let serial=0;
const pending=new Map();
const call=(method,params={})=>new Promise((resolve,reject)=>{
  const id=++serial;pending.set(id,{resolve,reject});socket.send(JSON.stringify({id,method,params}));
});
socket.addEventListener('message',event=>{
  const message=JSON.parse(event.data);
  if(message.id){const p=pending.get(message.id);pending.delete(message.id);message.error?p.reject(message.error):p.resolve(message.result);}
  else if(message.method==='Runtime.bindingCalled' && message.params.name==='__axiomManualLog'){
    try{append(JSON.parse(message.params.payload));}catch{append({kind:'invalid-observation'});}
  }
});
await call('Runtime.enable');
await call('Runtime.addBinding',{name:'__axiomManualLog'});
const read=`(()=>{
 const h=globalThis.axiomMinimalHost;if(!h)return null;
 const s=h.textInput.snapshot(),o=h.observation();
 return {revision:o.revision,dirty:o.dirty,text:{...s,projection:s.projection.slice(0,8192),preedit:s.preedit.slice(0,8192)},runtime:{objects:o.objects,operations:o.operations,presentedFrames:o.presentedFrames,retries:o.retries,pendingFrame:o.pendingFrame,activePointers:o.activePointers,lastPointer:o.lastPointer,camera:o.camera,error:o.error},display:{dpr:devicePixelRatio,width:innerWidth,height:innerHeight}};
})()`;
let initial;
for(let i=0;i<150;i++){
  initial=await call('Runtime.evaluate',{expression:read,returnByValue:true});
  if(initial.result.value)break;
  await new Promise(r=>setTimeout(r,200));
}
if(!initial.result.value)throw new Error('Actual Minimal Host did not become ready');
append({kind:'start',browser:browser.Browser,launcherPid:chrome.pid,pageId:page.id,url,state:initial.result.value});
fs.writeFileSync(path.join(directory,'web-session.json'),JSON.stringify({log,profile,port,pageId:page.id,launcherPid:chrome.pid,browser:browser.Browser,source:initial.result.value},null,2));
await call('Runtime.evaluate',{expression:`(()=>{
 const read=()=>${read};
 const observe=event=>{
  const id=event.target?.id||'';
  if(!['axiom-ime','axiom-canvas','debugUi','textWidth'].includes(id) && !['visibilitychange','blur','focus'].includes(event.type))return;
  const fact={kind:'native-event',type:event.type,trusted:event.isTrusted,target:id,inputType:event.inputType,data:event.data?.slice(0,2048),key:event.key,pointerId:event.pointerId,pointerType:event.pointerType,clientX:event.clientX,clientY:event.clientY,state:read()};
  globalThis.__axiomManualLog(JSON.stringify(fact));
  setTimeout(()=>globalThis.__axiomManualLog(JSON.stringify({kind:'after-event',type:event.type,state:read()})),0);
 };
 for(const type of ['compositionstart','compositionupdate','compositionend','beforeinput','input','keydown','pointerdown','pointerup','pointercancel','blur','focus','visibilitychange'])document.addEventListener(type,observe,true);
 return true;
})()`});
console.log(`Manual Chrome ready: ${initial.result.value.revision} dirty=${initial.result.value.dirty}; log=${log}`);
let last='';
const interval=setInterval(async()=>{
  try{const result=await call('Runtime.evaluate',{expression:read,returnByValue:true});const next=JSON.stringify(result.result.value);if(next!==last){last=next;append({kind:'state',state:result.result.value});}}catch{}
},1000);
const timeout=setTimeout(()=>socket.close(),1800000);
await new Promise(resolve=>socket.addEventListener('close',resolve,{once:true}));
clearInterval(interval);clearTimeout(timeout);
append({kind:'observer-ended',physicalVerdict:'Requires user observations; no automatic PASS'});
