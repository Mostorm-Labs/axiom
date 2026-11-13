// Frozen sparse evidence. Geometry/validation remain shared C++/WASM only.
import fs from 'node:fs/promises';import http from 'node:http';import path from 'node:path';
import {chromium} from '../../node_modules/playwright/index.mjs';
import {validateChunk} from './ur2d_selection_up_contract.mjs';
export async function consumer(config){
 const row={id:config.caseId,status:'STARTED',jobs:[],executors:[],assertions:[],errors:[],cleanup:[],failure:null};
 let sdk,client,owner,heartbeat;let identity,executor,job=0;const beats=[];
 const pause=ms=>new Promise(r=>setTimeout(r,ms));const check=(x,m)=>{if(!x)throw Error(m);row.assertions.push(m);};
 const save=()=>window.cpuMetadata(row);
 const bytes=async(name,data)=>{for(let offset=0;offset<data.length;offset+=65536){const chunk=data.subarray(offset,offset+65536);let binary='';for(let i=0;i<chunk.length;i+=8192)binary+=String.fromCharCode(...chunk.subarray(i,i+8192));const ack=await window.cpuBytes({name,offset,total:data.length,base64:btoa(binary)});check(ack.nextOffset===offset+chunk.length,'binary ack');}};
 const stamp=(input,n=++job)=>{const b=input.slice(0),v=new DataView(b);[...identity,executor,String(n),'1','1'].forEach((x,i)=>v.setBigUint64(32+i*8,BigInt(x),true));return b;};
 const requests=new Map();const request=async index=>{if(!requests.has(index)){const response=await fetch('/requests/'+index+'.request');check(response.ok,'input fetch');requests.set(index,await response.arrayBuffer());}return requests.get(index);};
 const recordLedger=async(label,closing=false)=>{
  if(!client)return;
  const before=client.observation();const value={label,before,quotaBefore:owner.observe(),capturedAt:performance.now()};row.executors.push(value);await save();
  const scope=before.basis.join('/')+'/'+before.executor+'/';const admitted=row.jobs.filter(v=>v.key?.startsWith(scope)&&v.accepted).map(v=>v.key);
  const terminal=before.ledger.map(v=>v.key);check(before.admitted===admitted.length,'admission list count');check(before.terminals===terminal.length,'terminal list count');check(admitted.length===terminal.length&&new Set(terminal).size===terminal.length&&admitted.every(k=>terminal.includes(k)),'all accepted exactly one terminal');check(before.pending===0,'no orphaned tasks');
   if(closing){await client.close();value.after=client.observation();value.quotaAfter=owner.observe();await save();check(value.after.transportBytes===0&&(value.after.cleanupErrors?.length??0)===0&&value.quotaAfter.bytes===0,'closed quota and transport zero');client=null;}
 };
 const awaitReady=async()=>{const start=performance.now();while(sdk.readState().generation!==sdk.readState().lastCommit.presentedGeneration){check(performance.now()-start<10000,'canonical receipt bounded');await pause(16);}};
 const outcome=async(task,info,index,keepBytes=true)=>{
  const v=await task.result;Object.assign(info,{status:v.status,terminalReceivedAt:performance.now(),computeMs:v.computeMs,importMs:v.importMs,exportMs:v.exportMs,yieldMs:v.yieldMs??null,totalMs:v.totalMs,steps:v.steps,cursor:v.cursor,maxUnionNs:v.maxUnionNs,cancelObservedAt:v.cancelObservedAt,outputBytes:v.bytes?.byteLength??0});await save();
  if(v.bytes&&keepBytes)await bytes(info.label+'.rgba',new Uint8Array(v.bytes));
  if(v.status==='Ready'){
   const reference=new Uint8Array(await(await fetch('/requests/'+index+'.path')).arrayBuffer());const actual=new Uint8Array(v.bytes,128);info.nativeBytesEqual=actual.length===reference.length&&actual.every((x,i)=>x===reference[i]);v.release();await save();check(info.nativeBytesEqual,'independent native path equality');check(v.bytes.byteLength===0,'released result detached');
  }
  return v;
 };
 const beginJob=(input,label,index)=>{const envelope=stamp(input),at=performance.now();const task=client.submit(envelope),info={label,index,executor,key:task.key,accepted:true,inputBytes:envelope.byteLength,submittedAt:at};row.jobs.push(info);return{task,info};};
 const one=async(index,label='job-'+index)=>{const {task,info}=beginJob(await request(index),label,index);const start=performance.now();const v=await outcome(task,info,index);const end=performance.now();info.heartbeatDuringCompute=beats.filter(x=>x>=start&&x<=end);check(v.status==='Ready','real Worker Ready');return info;};
 const newClient=async()=>{client=await createCpuWorker(sdk);const v=client.observation();identity=v.basis;executor=v.executor;row.currentExecutor=v;await save();};
 const createRectangle=async()=>{await sdk.selectShape('rectangle');const r=sdk.canvas.getBoundingClientRect();sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;for(const phase of ['down','up'])sdk.canvas.dispatchEvent(new PointerEvent('pointer'+phase,{pointerId:1,pointerType:'mouse',clientX:r.left+300,clientY:r.top+200,button:0,buttons:phase==='down'?1:0,pressure:.5,bubbles:true,cancelable:true}));await awaitReady();check(sdk.readState().objects===1,'canonical rectangle created');};
 let createCpuWorker,cpuWorkerOwner;
 try{
  const root=await import('/app/axiom.mjs');({createCpuWorker}=await import('/app/internal/cpu-worker-client.mjs'));({cpuWorkerOwner}=await import('/app/internal/cpu-worker-owner.mjs'));
  sdk=await root.createRuntime({mount:document.getElementById('mount'),documentId:'00000000000000000000000000000001',assetBaseURL:config.assetBaseURL,builtinFonts:false,permission:'localSingleUser',onError:e=>row.errors.push({code:e.code,text:String(e)})});owner=cpuWorkerOwner(sdk);row.ownerBefore=owner.basis();row.before=sdk.readState();row.quotaBefore=owner.observe();await awaitReady();
  if(config.mode==='csp'){
   const start=performance.now();let error=null;try{client=await createCpuWorker(sdk);}catch(e){error=String(e);}row.cspFailure={elapsedMs:performance.now()-start,error,quotaAfter:owner.observe()};await save();check(error&&row.cspFailure.elapsedMs<5500,'CSP bounded explicit failure');check(row.cspFailure.quotaAfter.bytes===0,'CSP releases floor');await createRectangle();row.rootAfterCSP=sdk.readState();await sdk.close();sdk=null;row.status='OBSERVED';return{status:row.status};
  }
  await newClient();heartbeat=setInterval(()=>beats.push(performance.now()),5);
  if(config.mode==='sched'){
   for(let i=0;i<16;++i)await one(i,config.caseId+'-'+i);
   if(config.variant==='candidate'){await one(17,'candidate-fork');await one(18,'candidate-filled');}
   clearInterval(heartbeat);heartbeat=null;row.heartbeat={ticks:beats,maxGapMs:Math.max(0,...beats.slice(1).map((v,i)=>v-beats[i]))};row.after=sdk.readState();check(row.after.generation===row.before.generation&&row.after.objects===row.before.objects,'compute does not mutate canonical');await recordLedger('scheduler-complete',true);
  }else if(config.mode==='life'){
   const input=await request(0),{task,info}=beginJob(input,'started-cancel',0);const waiting=performance.now();while(!client.observation().active?.startedReceivedAt){check(performance.now()-waiting<5000,'Worker starts bounded');await pause(1);}info.startedIdentity=client.observation().active;await pause(10);info.cancelSentAt=performance.now();client.cancel(task.key);const cancelled=await outcome(task,info,0,false);await save();check(cancelled.status==='Cancelled'&&cancelled.cursor<1600&&cancelled.steps>0,'cancel interleaves before full drain');check(cancelled.steps<=200,'cancel step count finite');
   await one(1,'after-cancel');
   const first=row.jobs[0];let duplicate=false;try{client.submit(stamp(input,Number(BigInt(first.key.split('/')[5]))));}catch(e){duplicate=String(e);}row.duplicateRejected=duplicate;check(duplicate,'duplicate rejected');
   const wrong=stamp(input);new DataView(wrong).setBigUint64(32,999n,true);let foreign=null;try{client.submit(wrong);}catch(e){foreign=String(e);}row.foreignRejected=foreign;check(foreign,'foreign rejected');
   const malformed=stamp(input);new DataView(malformed).setUint32(12,1901,true);const bad=client.submit(malformed);const badInfo={label:'malformed',executor,key:bad.key,accepted:true,inputBytes:malformed.byteLength};row.jobs.push(badInfo);const rejected=await outcome(bad,badInfo,0,false);check(rejected.status==='Failed','malformed terminal failure');
   const queued=[];let refused=null;for(let i=0;i<9;++i){try{queued.push(beginJob(input,'queued-'+i,0));}catch(e){refused=String(e);break;}}row.queue={accepted:queued.map(x=>x.task.key),refused};await save();check(queued.length===7&&refused,'transport refuses bounded queue');for(const item of queued)client.cancel(item.task.key);for(const item of queued){const v=await outcome(item.task,item.info,0,false);check(v.status==='Cancelled','queued cancel terminal');}
   await recordLedger('before-next-generation',true);await newClient();await one(2,'generation-two');await recordLedger('generation-two-complete',true);
   // A real slow module holds startup custody while the root closes. The
   // standalone root remains until this explicit close succeeds.
   const init=createCpuWorker(sdk,{url:new URL('/slow/cpu-worker-thread.mjs',location.href)});init.catch(()=>{});await pause(10);row.initializationCustody={quota:owner.observe(),executor:owner.executor,basis:owner.basis(),acceptedJobs:[]};await save();await sdk.close();const stopped=await init.then(()=>null,e=>String(e));row.initializationClose={error:stopped};check(stopped,'root close during initialization rejects promise');sdk=null;
   // Fresh root DataPort holds a real canonical packet through failed close.
   const mount=document.createElement('div');mount.style.cssText='width:900px;height:700px';document.body.append(mount);sdk=await root.createRuntime({mount,documentId:'00000000000000000000000000000002',assetBaseURL:config.assetBaseURL,builtinFonts:false,permission:'localSingleUser'});owner=cpuWorkerOwner(sdk);row.publicationAttach=sdk.data.attach({capacity:8});await createRectangle();const packet=sdk.data.readNext(null);check(packet&&packet.bytes.length>0,'real retained canonical packet');row.packet={operationId:packet.operationId,commit:packet.commit,bytes:packet.bytes.length};await newClient();await one(3,'publication-owner');await recordLedger('before-pending-close',false);const priorOwner=cpuWorkerOwner(sdk);let closeError=null;try{await sdk.close();}catch(e){closeError={code:e.code,text:String(e)};}row.pendingClose={error:closeError,state:sdk.readState(),data:sdk.data.status(),bindingSame:cpuWorkerOwner(sdk)===priorOwner,terminated:client.observation(),quota:owner.observe()};await save();check(closeError?.code==='PUBLICATION_PENDING'&&row.pendingClose.bindingSame,'real pending close retains binding');check(row.pendingClose.data.pending===1&&row.pendingClose.quota.bytes===0,'pending packet preserved worker quota zero');
   let blocked=null;try{await sdk.selectTool('ink');}catch(e){blocked=e.code;}check(blocked==='DOCUMENT_CLOSING','close barrier forbids edits');let grantDenied=null;try{owner.reserve('999',1024);}catch(e){grantDenied=e.code;}check(grantDenied==='DOCUMENT_CLOSING','close barrier forbids new grants');row.pendingClose.editBlocked=blocked;row.pendingClose.grantBlocked=grantDenied;
   sdk.data.release(packet);row.released=sdk.data.status();check(row.released.pending===0,'release exact packet custody');await sdk.close();let absent=false;try{cpuWorkerOwner(sdk);}catch{absent=true;}row.retryClose={bindingAbsent:absent,worker:client.observation()};check(absent,'success unbinds only at final close');sdk=null;client=null;
  }else{
   await one(0,config.caseId+'-asset');await recordLedger('asset-complete',true);await createRectangle();row.after=sdk.readState();await sdk.undo();await awaitReady();check(sdk.readState().objects===0,'asset root Undo');await sdk.redo();await awaitReady();check(sdk.readState().objects===1,'asset root Redo');
  }
  row.status='OBSERVED';
 }catch(error){row.status='FAILED';row.failure={error:String(error.stack??error),at:performance.now()};try{row.failure.state=sdk?.readState();row.failure.client=client?.observation();row.failure.quota=owner?.observe();}catch(e){row.failure.captureError=String(e);}await save();}
 finally{
  if(heartbeat)clearInterval(heartbeat);
  if(client)try{row.finalExecutorBefore=client.observation();await client.close();row.finalExecutorAfter=client.observation();}catch(e){row.cleanup.push(String(e));}
  if(sdk)try{await sdk.close();}catch(e){row.cleanup.push(String(e));}
  row.completedAt=performance.now();await save();
 }
 return{status:row.status,jobs:row.jobs.length,failure:row.failure};
}
export function createServer(request,active){
 return http.createServer(async(req,res)=>{try{
  const url=new URL(req.url,'http://local'),name=url.pathname;active.requests.push({path:name,at:Date.now()});
  if(name==='/app/'||name==='/csp/'){
   if(name==='/csp/')res.setHeader('Content-Security-Policy',"default-src 'self'; script-src 'self' 'unsafe-inline' 'wasm-unsafe-eval'; style-src 'self' 'unsafe-inline'; worker-src 'none'; connect-src 'self'");
   res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:900px;height:700px"></div><script type="module">import{consumer}from"/driver.mjs";window.run=consumer;</script>');return;
  }
  if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+consumer.toString());return;}
  if(name==='/slow/cpu-worker-thread.mjs'){await new Promise(r=>setTimeout(r,250));res.setHeader('Content-Type','text/javascript');res.end('await new Promise(()=>{});');return;}
  let file;if(name.startsWith('/app/'))file=path.join(active.sdk,name.slice(5));else if(name.startsWith('/assets/'))file=path.join(active.sdk,name.slice(8));else if(name.startsWith('/requests/'))file=path.join(request.requests,name.slice(10));
  if(!file)throw Error('unknown asset');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
 }catch(error){res.statusCode=404;res.end(String(error));}});
}
export async function run(request,out){
 await fs.mkdir(out,{recursive:true});let server,browser,page;const active={sdk:null,requests:[]},result={id:request.id,status:'STARTED',rows:[],errors:[]};
 try{
  server=createServer(request,active);await new Promise(r=>server.listen(0,'127.0.0.1',r));const base='http://127.0.0.1:'+server.address().port;
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const spec of request.cases){
   active.sdk=spec.sdk;active.requests=[];const dir=path.join(out,spec.id);await fs.mkdir(dir);const chunks=new Map(),consoleLines=[];page=await browser.newPage({viewport:{width:1100,height:800},deviceScaleFactor:2});
   page.on('console',m=>{if(consoleLines.length<512)consoleLines.push({type:m.type(),text:m.text()});});
   await page.exposeFunction('cpuMetadata',value=>fs.writeFile(path.join(dir,'raw.json'),JSON.stringify(value,null,2)+'\n'));
   await page.exposeFunction('cpuBytes',async c=>{if(c.total>1048576)throw Error('oversized output');const old=chunks.get(c.name);if(old?.complete)throw Error('duplicatebuffer');const next=validateChunk(c,old),data=Buffer.from(c.base64,'base64'),file=path.join(dir,c.name+'.partial');const h=await fs.open(file,c.offset?'r+':'wx');try{const w=await h.write(data,0,data.length,c.offset);if(w.bytesWritten!==data.length)throw Error('shortwrite');}finally{await h.close();}if(next.complete)await fs.rename(file,path.join(dir,c.name));chunks.set(c.name,next);return{nextOffset:next.nextOffset};});
   console.log(JSON.stringify({stage:'case-start',id:spec.id}));
   await page.goto(base+(spec.mode==='csp'?'/csp/':'/app/'));await page.waitForFunction(()=>window.run);
   const config={...spec,caseId:spec.id,assetBaseURL:spec.assetBase==='absolute'?base+'/assets/':'../assets/'};const summary=await page.evaluate(c=>window.run(c),config);result.rows.push({id:spec.id,...summary});
   await fs.writeFile(path.join(dir,'http-requests.json'),JSON.stringify(active.requests,null,2)+'\n');await fs.writeFile(path.join(dir,'console.json'),JSON.stringify(consoleLines,null,2)+'\n');await page.close();page=null;console.log(JSON.stringify({stage:'case-end',id:spec.id,status:summary.status}));
   if(summary.status!=='OBSERVED')throw Error('frozen case failed; preserve raw, stop remaining rows');
  }
  result.status='OBSERVED';
 }catch(error){result.status='FAILED';result.error=String(error.stack??error);}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status}));}return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await run(request,path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
