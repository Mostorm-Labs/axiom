import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {chromium} from '../../node_modules/playwright/index.mjs';
export function createServer(request,result,save){
 return http.createServer(async(req,res)=>{try{
  const name=new URL(req.url,'http://localhost').pathname;let body,type='text/javascript';
  if(req.method==='POST'){let text='';for await(const part of req)text+=part;if(name==='/result')result.rows.push(JSON.parse(text));else result.errors.push(text);result.last_unix=Date.now()/1000;await save();res.end('ok');return;}
  if(name==='/'){body='<title>Axiom pending navigation and preparation</title><style>body{margin:0}#mount{position:relative;width:1200px;height:800px}#status{position:fixed;top:0;left:0}</style><div id="mount"></div><pre id="status"></pre>'+(request.iab?'<script type="module" src="/run.mjs"></script>':'');type='text/html';}
  else if(name==='/run-config.json'){body=JSON.stringify(request);type='application/json';}
  else if(name==='/run.mjs')body=await fs.readFile(new URL('./pending_navigation_progress_iab.mjs',import.meta.url));
  else if(name.startsWith('/g5/')&&!name.includes('..'))body=await fs.readFile(new URL('./'+name.slice(4),import.meta.url));
  else{const match=name.match(/^\/lanes\/([^/]+)\/(sdk|engine)\/(.*)$/);if(!match||match[3].includes('..'))throw Error('path');const lane=request.lanes.find(l=>l.name===match[1]);if(!lane)throw Error('lane');
   if(match[2]==='sdk'&&match[3]==='internal/axiom.mjs')body="import factory from '/lanes/"+lane.name+"/engine/internal/axiom.mjs';export default async options=>{const m=await factory(options);window.idleObservation.engine(m);return m;};";
   else{body=await fs.readFile(path.join(lane.sdk,match[3]));type=match[3].endsWith('.wasm')?'application/wasm':match[3].endsWith('.mjs')?'text/javascript':'application/octet-stream';}}
  res.setHeader('Content-Type',type);res.end(body);
 }catch(error){res.statusCode=404;res.end(String(error));}});
}
export async function run(request,out){
 await fs.mkdir(out,{recursive:true});const result={id:request.id,rows:[],errors:[],started_unix:Date.now()/1000,status:'STARTED'};
 const save=()=>fs.writeFile(path.join(out,'raw.json'),JSON.stringify(result,null,2)+'\n');let browser,server;
 try{
  server=createServer(request,result,save);await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  browser=await chromium.launch({headless:false,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const lane of request.lanes)for(const action of ['zoom','pan','nextdraw']){
   const page=await browser.newPage({viewport:{width:1200,height:800},deviceScaleFactor:2});page.on('pageerror',error=>result.errors.push(String(error)));
   await page.goto('http://127.0.0.1:'+server.address().port+'/');
   const row=await page.evaluate(async q=>{
    const {installPendingObserver}=await import('/g5/pending_navigation_progress_observer.mjs');installPendingObserver();
    const {installNavigationObserver}=await import('/g5/navigation_observer.mjs');window.installNav=installNavigationObserver;
    const {run}=await import('/g5/pending_navigation_progress_consumer.mjs');return run(q);
   },{fixture:request.fixture,documentId:request.documentId,action,lane:lane.name,sdkPrefix:'/lanes/'+lane.name+'/sdk/',pixel:false});
   result.rows.push(row);await save();console.log(JSON.stringify({stage:'case-done',lane:lane.name,action,status:row.status}));await page.close();if(row.status!=='OBSERVED')throw Error(row.failure);
  }
  result.status=result.errors.length?'FAILED':'OBSERVED';
 }catch(error){result.status='FAILED';result.failure=String(error.stack??error);}
 finally{if(browser)await browser.close();if(server)await new Promise(resolve=>server.close(resolve));result.ended_unix=Date.now()/1000;await save();}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await run(request,process.argv[3])).status!=='OBSERVED')process.exitCode=1;}
