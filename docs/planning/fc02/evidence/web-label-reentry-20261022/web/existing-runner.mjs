import fs from 'node:fs';
import path from 'node:path';
import {spawn} from 'node:child_process';
const profile=path.resolve('out/fc02-host/chrome-isolated');fs.mkdirSync(profile,{recursive:true});
const portFile=path.join(profile,'DevToolsActivePort');if(fs.existsSync(portFile))fs.unlinkSync(portFile);
const chrome=spawn('C:/Program Files/Google/Chrome/Application/chrome.exe',['--headless=new','--no-first-run','--no-default-browser-check',`--user-data-dir=${profile}`,'--remote-debugging-address=127.0.0.1','--remote-debugging-port=0','--enable-unsafe-swiftshader','--use-angle=swiftshader','about:blank'],{windowsHide:true,stdio:'ignore'});
for(let i=0;!fs.existsSync(portFile)&&i<100;i++)await new Promise(r=>setTimeout(r,100));
const port=fs.readFileSync(portFile,'utf8').split('\n')[0];
const browser=await fetch(`http://127.0.0.1:${port}/json/version`).then(r=>r.json());
const page=await fetch(`http://127.0.0.1:${port}/json/new?about:blank`,{method:'PUT'}).then(r=>r.json());
const socket=new WebSocket(page.webSocketDebuggerUrl);await new Promise(r=>socket.addEventListener('open',r,{once:true}));
let serial=0;const pending=new Map(),events=[];
socket.addEventListener('message',e=>{const m=JSON.parse(e.data);if(m.id){const p=pending.get(m.id);pending.delete(m.id);if(m.error)p.reject(m.error);else p.resolve(m.result);}else if(['Runtime.exceptionThrown','Runtime.consoleAPICalled','Log.entryAdded'].includes(m.method))events.push(m);});
const call=(method,params={})=>new Promise((resolve,reject)=>{const id=++serial;pending.set(id,{resolve,reject});socket.send(JSON.stringify({id,method,params}));});
await call('Emulation.setDeviceMetricsOverride',{width:1400,height:1000,deviceScaleFactor:2,mobile:false});
await call('Network.enable');await call('Network.setCacheDisabled',{cacheDisabled:true});
await call('Runtime.enable');await call('Page.enable');await call('Log.enable');
try {
for(const name of process.argv.slice(2)){
 events.length=0;await call('Page.navigate',{url:`http://127.0.0.1:4174/tests/${name}.html`});let result='PENDING';
 for(let i=0;i<250;i++){await new Promise(r=>setTimeout(r,200));const r=await call('Runtime.evaluate',{expression:'document.getElementById("result")?.textContent',returnByValue:true});result=r.result.value||'PENDING';if(result!=='PENDING')break;}
 const diagnostic=await call('Runtime.evaluate',{expression:'(()=>{const f=document.getElementById("app");const h=f?.contentWindow.axiomMinimalHost;const c=f?.contentDocument.getElementById("axiom-canvas");return {observation:h?.observation(),width:c?.width,height:c?.height,status:f?.contentDocument.getElementById("status")?.textContent}})()',returnByValue:true});
 const output={diagnostic:diagnostic.result,browser:browser.Browser,mode:'Chrome headless SwiftShader, synthetic DOM events',page:name,result,events};fs.writeFileSync(`out/fc02-host/${name}.json`,JSON.stringify(output,null,2));console.log(name,result.slice(0,700));if(result==='PENDING'||(JSON.parse(result).result||JSON.parse(result).status)!=='PASS')process.exitCode=1;
}
}finally{await Promise.race([call('Browser.close'),new Promise(r=>setTimeout(r,1000))]).catch(()=>{});socket.close();}
