import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {chromium} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/node_modules/playwright/index.mjs';
import {entropyStream,installEntropy} from '/Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/runners/g5/exact_fixed_scene_contract.mjs';
import {HeavyStore} from '../heavy_scene_attribution_store.mjs';
export async function run(request,out){
 await fs.mkdir(out,{recursive:true});const result={inputCadence:'batched before RAF; separate focused post-Up case',id:request.id,status:'STARTED',started_unix:Date.now()/1000,rows:[],errors:[]};let browser,page,server,active;
 const save=()=>fs.writeFile(path.join(out,'session.json'),JSON.stringify(result,null,2)+'\n');
 try{
  server=http.createServer(async(req,res)=>{try{
   const name=new URL(req.url,'http://local').pathname;let body,type='application/octet-stream';
   if(name==='/'){body='<title>Axiom current heavy scene attribution</title><style>body{margin:0}</style><div id="mount" style="width:1431px;height:1010px"></div>';type='text/html';}
   else if(name==='/favicon.ico'){res.statusCode=204;res.end();return;}
   else if(name==='/fixture.axsnap')body=await fs.readFile(active.fixture.path);
   else if(name==='/image.png')body=await fs.readFile(request.image);
   else if(name==='/sdk/internal/axiom.mjs'){body="import factory from '/engine/internal/axiom.mjs';export default async options=>{const m=await factory(options);window.heavyObservation.engine(m);return m;};";type='text/javascript';}
   else if(name.startsWith('/g5/')&&!name.includes('..')){body=await fs.readFile(name==='/g5/heavy_scene_attribution_primary.mjs'?new URL('./primary_batched.mjs',import.meta.url):new URL('file:///Users/qing/.codex/worktrees/axiom-geometry-navigation/axiom/verification/runners/g5/'+name.slice(4)));type='text/javascript';}
   else{const prefix=name.startsWith('/sdk/')?'/sdk/':name.startsWith('/engine/')?'/engine/':null;if(!prefix||name.includes('..'))throw Error('route');const rel=name.slice(prefix.length);body=await fs.readFile(path.join(request.sdk,rel));type=rel.endsWith('.mjs')?'text/javascript':rel.endsWith('.wasm')?'application/wasm':'application/octet-stream';}
   res.setHeader('Content-Type',type);res.end(body);
  }catch(error){result.errors.push({route:req.url,error:String(error)});res.statusCode=500;res.end(String(error));}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  const cases=request.kind==='primary'?[{id:'primary'}]:request.rows.map(rowId=>({id:rowId,rowId}));
  for(const spec of cases){
   const directory=path.join(out,spec.id);const store=new HeavyStore(directory);await store.initialize();active=request;
   page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:request.dpr});page.setDefaultTimeout(request.seconds*1000);
   await page.addInitScript({content:entropyStream.toString()+';'+installEntropy.toString()+';installEntropy(324508639);'});
   page.on('pageerror',error=>result.errors.push(String(error)));
   await page.exposeFunction('haPersistRecords',value=>store.records(value));await page.exposeFunction('persistChunk',value=>store.chunk(value));
   await page.exposeFunction('persistMetadata',value=>store.metadata(value));
   console.log(JSON.stringify({stage:'case-start',id:request.id,row:spec.id}));await page.goto('http://127.0.0.1:'+server.address().port+'/');
   const row=await page.evaluate(async q=>{const {installHeavyObserver}=await import('/g5/heavy_scene_attribution_observer.mjs');installHeavyObserver();const m=await import(q.kind==='primary'?'/g5/heavy_scene_attribution_primary.mjs':'/g5/heavy_scene_attribution_mixed.mjs');return q.kind==='primary'?m.primaryConsumer(q):m.mixedConsumer(q);},{...request,...spec});
   const expected=request.kind==='primary'?['AB.axsnap','AB.rgba','undo.axsnap','undo.rgba','redo.axsnap','redo.rgba','followup.axsnap','followup.rgba']:['final.axsnap','undo.axsnap','redo.axsnap','final.rgba'];
   const index=await store.finish(row.status==='OBSERVED'?expected:[]);const entry={id:spec.id,status:row.status,error:row.error,cleanupError:row.cleanupError,closed:row.closed,records:index.records.count,criticalLost:row.observation?.criticalLost??null,metadata:'./'+spec.id+'/raw.json'};result.rows.push(entry);await save();
   console.log(JSON.stringify({stage:'case-complete',session:request.id,...entry}));await page.close();page=null;
   if(row.status!=='OBSERVED'||row.cleanupError||!row.closed||row.observation?.criticalLost||row.observation?.lastError)throw Error('rowfailed:'+JSON.stringify(entry));
  }
  if(result.errors.length)throw Error('page/server errors');result.status='OBSERVED';
 }catch(error){result.status='FAILED';result.error=String(error.stack??error);}
 finally{if(page)await page.close();if(browser)await browser.close();if(server)await new Promise(resolve=>server.close(resolve));result.ended_unix=Date.now()/1000;await save();console.log(JSON.stringify({stage:'closed',id:request.id,status:result.status,error:result.error}));}
 return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const q=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await run(q,process.argv[3])).status!=='OBSERVED')process.exitCode=1;}
