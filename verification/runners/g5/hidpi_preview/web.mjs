import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {chromium} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/node_modules/playwright/index.mjs';
import {entropyStream,installEntropy} from '../exact_fixed_scene_contract.mjs';
import {BufferStore,taskGuard} from './storage.mjs';
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function run(q,out){
 await fs.mkdir(out,{recursive:true});let browser,server;const stores=[];
 const life={id:q.id,kind:q.kind,status:'STARTED',started:Date.now()/1000,errors:[],served:{},cases:[]};
 try{
  server=http.createServer(async(req,res)=>{try{
   const name=new URL(req.url,'http://localhost').pathname;let b;
   if(name==='/'){b=Buffer.from('<style>html,body{margin:0}#mount{position:relative;width:1431px;height:1010px}</style><div id="mount"></div>');res.setHeader('Content-Type','text/html');}
   else if(name==='/favicon.ico'){res.statusCode=204;res.end();return;}
   else if(name.startsWith('/sdk/')&&!name.includes('..'))b=await fs.readFile(path.join(q.sdk,name.slice(5)));
   else if(q.smokeSDK&&name.startsWith('/normal/')&&!name.includes('..'))b=await fs.readFile(path.join(q.smokeSDK,name.slice(8)));
   else if(name.startsWith('/g5/')&&!name.includes('..'))b=await fs.readFile(new URL('..'+name.slice(3),import.meta.url));
   else throw Error('unexpected route '+name);
   if(name!=='/')res.setHeader('Content-Type',name.endsWith('.mjs')?'text/javascript':name.endsWith('.wasm')?'application/wasm':'application/octet-stream');
   life.served[name]={bytes:b.length,sha256:sha(b)};res.end(b);
  }catch(e){life.errors.push({route:req.url,error:String(e)});res.statusCode=500;res.end(String(e));}});
  await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  life.browserVersion=browser.version();
  const system=await browser.newBrowserCDPSession();life.system=await system.send('SystemInfo.getInfo');
  if(q.smokeSDK){
   const context=await browser.newContext({viewport:{width:1431,height:1010},deviceScaleFactor:2});const page=await context.newPage();
   await page.goto('http://127.0.0.1:'+server.address().port+'/');
   life.normalSmoke=await page.evaluate(async()=>{
    const pause=ms=>new Promise(r=>setTimeout(r,ms)),{createRuntime}=await import('/normal/axiom.mjs');const errors=[];
    const sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'20000000000000000000000000000002',builtinFonts:false,permission:'localSingleUser',onError:e=>errors.push(e.code)});
    sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
    await sdk.selectBrush('crayon-wax-v1',3);await sdk.selectTool('ink');
    const event=(type,i)=>sdk.canvas.dispatchEvent(new PointerEvent('pointer'+type,{pointerId:1,pointerType:'mouse',clientX:100+i*2,clientY:200,button:type==='move'?-1:0,buttons:type==='up'||type==='cancel'?0:1,pressure:.5,bubbles:true,cancelable:true}));
    event('down',0);for(let i=1;i<=30;++i){event('move',i);await pause(17);}event('up',30);
    let state;for(let n=0;n<300;++n){state=sdk.readState();if(state.objects===1&&state.lastCommit.presentedGeneration===state.generation)break;await pause(16);}
    if(state.objects!==1)throw Error('normal smoke commit');
    event('down',0);event('move',1);event('cancel',1);await pause(100);const cancelled=sdk.readState();
    const result={build:sdk.readBuildInfo(),committedObjects:state.objects,cancelledObjects:cancelled.objects,errors,normalSDKRootOnly:true};
    await sdk.close();if(errors.length||cancelled.objects!==1)throw Error('normal smoke error');return result;
   });
   await fs.writeFile(path.join(out,'normal-smoke.json'),JSON.stringify(life.normalSmoke,null,2)+'\n');
   await page.close();await context.close();
  }
  for(const dpr of [2,4])for(const [profile,id] of [['crayon-wax-v1','wax'],['chalk-compact-v1','chalk']]){
   const context=await browser.newContext({viewport:{width:1431,height:1010},deviceScaleFactor:dpr});
   const page=await context.newPage();page.on('pageerror',e=>life.errors.push(String(e)));
   await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(324508639);'});
   const caseId=id+'-'+dpr,dest=path.join(out,caseId);await fs.mkdir(dest,{recursive:true});
   const store=new BufferStore(dest,{guard:taskGuard});stores.push(store);
   await page.exposeFunction('hdBytes',v=>store.chunk(v));
   await page.exposeFunction('hdMetadata',async value=>{const b=JSON.stringify(value)+'\n';if(Buffer.byteLength(b)>64*1048576)throw Error('trace cap');await fs.writeFile(path.join(dest,'raw.json'),b);console.log(JSON.stringify({stage:value.stage,id:q.id,case:caseId,status:value.status}));});
   await page.goto('http://127.0.0.1:'+server.address().port+'/');
   const result=await page.evaluate(async request=>{const {consume}=await import('/g5/hidpi_preview/consumer.mjs');return consume(request);},{...q,profile,dpr,caseId});
   life.cases.push({caseId,status:result.status,closed:result.closed,error:result.error});
   if(q.kind==='correctness')await page.screenshot({path:path.join(dest,'terminal.png')});
   await page.close();await context.close();
   if(result.status!=='OBSERVED'||!result.closed||life.errors.length)throw Error(result.error??'consumer/environment failure');
  }
  life.status='OBSERVED';
 }catch(e){life.status='FAILED';life.error=String(e.stack??e);}
 finally{for(const store of stores)await store.abort();if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));life.ended=Date.now()/1000;await fs.writeFile(path.join(out,'lifecycle.json'),JSON.stringify(life,null,2)+'\n');}
 return life;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2]));if((await run(request,process.argv[3])).status!=='OBSERVED')process.exitCode=1;}
