import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {chromium} from '../../node_modules/playwright/index.mjs';
import {EvidenceStore} from './pending_navigation_evidence_io_store.mjs';
import {verifyCompletion} from './pending_navigation_evidence_io_client.mjs';
export async function run(request,out){
 await fs.mkdir(out,{recursive:true});const keys=request.lanes.flatMap(lane=>['zoom','pan'].map(action=>lane.name+'-'+action));
 const store=new EvidenceStore(out,keys),lifecycle={id:request.id,started_unix:Date.now()/1000,errors:[],ACKs:[]};
 const save=()=>fs.writeFile(path.join(out,'lifecycle.json'),JSON.stringify(lifecycle,null,2)+'\n');let browser,server;
 try{
  server=http.createServer(async(req,res)=>{try{
   const name=new URL(req.url,'http://localhost').pathname;let body,type='text/javascript';
   if(name==='/'){body='<title>Axiom isolated postcommit navigation</title><style>body{margin:0}#mount{position:relative;width:1200px;height:800px}</style><div id="mount"></div>';type='text/html';}
   else if(name==='/favicon.ico'){res.statusCode=204;res.end();return;}
   else if(name.startsWith('/g5/')&&!name.includes('..'))body=await fs.readFile(new URL('./'+name.slice(4),import.meta.url));
   else{const m=name.match(/^\/lanes\/([^/]+)\/(sdk|engine)\/(.*)$/);if(!m||m[3].includes('..'))throw Error('path');const lane=request.lanes.find(value=>value.name===m[1]);if(!lane)throw Error('lane');
    if(m[2]==='sdk'&&m[3]==='internal/axiom.mjs')body="import factory from '/lanes/"+lane.name+"/engine/internal/axiom.mjs';export default async options=>{const module=await factory(options);window.idleObservation.engine(module);return module;};";
    else{body=await fs.readFile(path.join(lane.sdk,m[3]));type=m[3].endsWith('.wasm')?'application/wasm':m[3].endsWith('.mjs')?'text/javascript':'application/octet-stream';}}
   res.setHeader('Content-Type',type);res.end(body);
  }catch(error){lifecycle.errors.push({route:req.url,error:String(error)});res.statusCode=500;res.end(String(error));}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  browser=await chromium.launch({headless:false,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const lane of request.lanes)for(const action of ['zoom','pan']){
   const page=await browser.newPage({viewport:{width:1200,height:800},deviceScaleFactor:2});page.on('pageerror',error=>lifecycle.errors.push({error:String(error)}));
   await page.goto('http://127.0.0.1:'+server.address().port+'/');
   const row=await page.evaluate(async q=>{
    if(q.diagnostic){const {installPostcommitObserver}=await import('/g5/postcommit_navigation_observer.mjs');installPostcommitObserver();}
    else{const {installPendingObserver}=await import('/g5/pending_navigation_progress_observer.mjs');installPendingObserver();}
    const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');window.installNav=installNavigationObserver;
    const {run}=await import(q.pixel?'/g5/postcommit_navigation_pixel_consumer.mjs':'/g5/pending_navigation_progress_consumer.mjs');const result=await run(q);
    if(q.diagnostic)result.nativeObservation=window.postcommitObservation.read();return result;
   },{fixture:request.fixture,documentId:request.documentId,lane:lane.name,action,pixel:!!request.pixel,sdkPrefix:'/lanes/'+lane.name+'/sdk/',diagnostic:!!request.diagnostic});
   const ack=await store.save(row);lifecycle.ACKs.push(ack);await save();await page.close();
   console.log(JSON.stringify({stage:'case-done',action,lane:lane.name,status:row.status,saved:ack.saved}));
   if(row.status!=='OBSERVED'||!row.closed||row.errors.length)throw Error(row.failure??'row failed/closed/errors');
  }
  verifyCompletion(await store.index(),keys);if(lifecycle.errors.length)throw Error('server/page errors');lifecycle.status='OBSERVED';
 }catch(error){lifecycle.status='FAILED';lifecycle.failure=String(error.stack??error);}
 finally{if(browser)await browser.close();if(server)await new Promise(resolve=>server.close(resolve));lifecycle.ended_unix=Date.now()/1000;await save();}
 return lifecycle;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await run(request,process.argv[3])).status!=='OBSERVED')process.exitCode=1;}
