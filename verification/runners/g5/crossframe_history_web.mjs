// Bounded ordinary/qualification verification. No native reference during browser.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {chromium} from '../../node_modules/playwright/index.mjs';
import {validateChunk} from './ur2d_selection_up_contract.mjs';
import {entropyStream,installEntropy} from './exact_fixed_scene_contract.mjs';
import {webConsumer as upConsumer,qualificationFactory} from './crossframe_history_up.mjs';
import {webConsumer as feedbackConsumer} from './crossframe_history_feedback.mjs';
export function createServer(request,active){
 const root=path.resolve(new URL('../../..',import.meta.url).pathname);
 return http.createServer(async(req,res)=>{try{
  const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
  if(name==='/'){res.setHeader('Content-Type','text/html');res.end('<style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div><script type="module">import{webConsumer}from"/driver.mjs";window.run=webConsumer;</script>');return;}
  if(name==='/driver.mjs'){res.setHeader('Content-Type','text/javascript');res.end('export '+(request.feedback?feedbackConsumer:upConsumer).toString());return;}
  if(request.qualification&&name==='/sdk/internal/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(qualificationFactory);return;}
  if(name==='/fixture.axsnap')file=request.fixture.path;
  else if(name.startsWith('/sdk/'))file=path.join(active.sdk,name.slice(5));
  else if(name.startsWith('/engine/'))file=path.join(active.sdk,name.slice(8));
  else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));
  if(!file)throw Error('unknown HTTP path');res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));
 }catch(error){res.statusCode=404;res.end(String(error));}});
}
export async function runWindow(request,out){
 await fs.mkdir(out,{recursive:true});const active={sdk:null};let server,browser,page;
 const result={id:request.id,status:'STARTED',rows:[],errors:[],cleanupErrors:[]};
 try{
  server=createServer(request,active);await new Promise(r=>server.listen(0,'127.0.0.1',r));
  browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const spec of request.cases){
   active.sdk=spec.sdk;const directory=path.join(out,spec.id);await fs.mkdir(directory);const transfers=new Map(),consoleLines=[];
   page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});
   await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy('+spec.seed+');'});
   page.on('console',m=>{if(consoleLines.length<4096)consoleLines.push({type:m.type(),text:m.text().slice(0,8192)});});
   page.on('pageerror',e=>result.errors.push({id:spec.id,error:String(e)}));
   const metadata=v=>fs.writeFile(path.join(directory,'raw-row.json'),JSON.stringify(v,null,2)+'\n');
   await page.exposeFunction('waMetadata',metadata);await page.exposeFunction('tpMetadata',metadata);
   await page.exposeFunction('tpRecord',async({name,value})=>{if(!/^[a-zA-Z0-9_-]{1,96}$/.test(name))throw Error('record name');await fs.writeFile(path.join(directory,name+'.json'),JSON.stringify(value,null,2)+'\n');});
   const persist=async chunk=>{
    const old=transfers.get(chunk.name);if(old?.complete)throw Error('completed buffer replay');
    const next=validateChunk(chunk,old),bytes=Buffer.from(chunk.base64,'base64'),file=path.join(directory,chunk.name+'.partial');
    const h=await fs.open(file,chunk.offset?'r+':'wx');try{const n=await h.write(bytes,0,bytes.length,chunk.offset);if(n.bytesWritten!==bytes.length)throw Error('short write');}finally{await h.close();}
    if(next.complete)await fs.rename(file,path.join(directory,chunk.name));transfers.set(chunk.name,next);return{nextOffset:next.nextOffset};
   };
   await page.exposeFunction('waChunk',persist);await page.exposeFunction('tpChunk',persist);
   await page.exposeFunction('hpTraceChunk',async value=>{const line=JSON.stringify(value)+'\n';if(Buffer.byteLength(line)>131072)throw Error('trace page transport limit');await fs.appendFile(path.join(directory,'trace-pages.jsonl'),line);return value.index;});
   console.log(JSON.stringify({stage:'case-start',id:spec.id,mode:spec.mode,qualification:!!request.qualification}));
   try{
    await page.goto('http://127.0.0.1:'+server.address().port+'/');await page.waitForFunction(()=>window.run);
    const summary=await page.evaluate(async config=>{const row=await window.run(config);return{status:row.status,id:config.id,cleanupError:row.cleanupError??null,cleanupErrors:row.cleanupErrors??[],feedbacks:row.feedbacks?.map(v=>({kind:v.kind,status:v.status})),traceParts:row.traceParts??0};},{...spec,fixture:request.fixture,tableReference:request.tableReference,diagnostic:!!request.qualification});
    result.rows.push(summary);
   }catch(error){result.rows.push({id:spec.id,status:'FAILED',error:String(error.stack??error)});}
   finally{await fs.writeFile(path.join(directory,'console.json'),JSON.stringify(consoleLines,null,2)+'\n');if(page){await page.close();page=null;}}
   console.log(JSON.stringify({stage:'case-end',...result.rows.at(-1)}));
   const last=result.rows.at(-1);if(last.status==='FAILED'||last.cleanupError||last.cleanupErrors?.length)throw Error('unsafe/incomplete row prevents remaining cases');
  }
  result.status=result.rows.every(v=>['OBSERVED','CLASSIFIED_FAILURE'].includes(v.status))&&!result.errors.length?'CAPTURED':'FAILED';
 }catch(error){result.status='FAILED';result.error=String(error.stack??error);}
 finally{
  try{if(page)await page.close();}catch(error){result.cleanupErrors.push({stage:'page',error:String(error)});}
  try{if(browser)await browser.close();}catch(error){result.cleanupErrors.push({stage:'browser',error:String(error)});}
  try{if(server)await new Promise(r=>server.close(r));}catch(error){result.cleanupErrors.push({stage:'server',error:String(error)});}
  await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status}));
 }
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await runWindow(request,path.resolve(process.argv[3]))).status!=='CAPTURED')process.exitCode=1;}
