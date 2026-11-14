import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {EvidenceStore} from './pending_navigation_evidence_io_store.mjs';
const request=JSON.parse(await fs.readFile(process.argv[2],'utf8')),out=process.argv[3];
await fs.mkdir(out,{recursive:true});
const expected=request.rows.map(value=>value.lane+'-'+value.action),store=new EvidenceStore(out,expected);
const lifecycle={started_unix:Date.now()/1000,errors:[],row_ACKs:[],expected};
const saveLifecycle=()=>fs.writeFile(path.join(out,'lifecycle.json'),JSON.stringify(lifecycle,null,2)+'\n');await saveLifecycle();
const server=http.createServer(async(req,res)=>{try{
 const name=new URL(req.url,'http://localhost').pathname;let body,type='text/javascript';
 if(req.method==='POST'){
  let text='',bytes=0;for await(const part of req){bytes+=part.length;if(bytes>300*1024*1024)throw Error('one-row transport quota');text+=part;}
  if(name==='/result'){
   const row=JSON.parse(text);text='';const ack=await store.save(row);
   lifecycle.row_ACKs.push({...ack,at_unix:Date.now()/1000});await saveLifecycle();
   res.setHeader('Content-Type','application/json');res.end(JSON.stringify(ack));return;
  }
  lifecycle.errors.push({at_unix:Date.now()/1000,message:text.slice(0,10000)});await saveLifecycle();res.end('recorded');return;
 }
 if(name==='/index.json'){body=JSON.stringify(await store.index());type='application/json';}
 else if(name==='/run-config.json'){body=JSON.stringify(request);type='application/json';}
 else if(name==='/'){body='<title>Axiom pending navigation evidence supplement</title><style>body{margin:0}#mount{position:relative;width:1200px;height:800px}#status{position:fixed;top:0;left:0}</style><div id="mount"></div><pre id="status"></pre><script type="module" src="/g5/pending_navigation_evidence_io_page.mjs"></script>';type='text/html';}
 else if(name.startsWith('/g5/')&&!name.includes('..'))body=await fs.readFile(new URL('./'+name.slice(4),import.meta.url));
 else{const match=name.match(/^\/lanes\/([^/]+)\/(sdk|engine)\/(.*)$/);if(!match||match[3].includes('..'))throw Error('path');const lane=request.lanes.find(value=>value.name===match[1]);if(!lane)throw Error('lane');
  if(match[2]==='sdk'&&match[3]==='internal/axiom.mjs')body="import factory from '/lanes/"+lane.name+"/engine/internal/axiom.mjs';export default async options=>{const m=await factory(options);window.idleObservation.engine(m);return m;};";
  else{body=await fs.readFile(path.join(lane.sdk,match[3]));type=match[3].endsWith('.wasm')?'application/wasm':match[3].endsWith('.mjs')?'text/javascript':'application/octet-stream';}}
 res.setHeader('Content-Type',type);res.end(body);
 }catch(error){lifecycle.errors.push({at_unix:Date.now()/1000,message:String(error.stack??error)});await saveLifecycle();res.statusCode=500;res.end(String(error));}
});
server.listen(0,'127.0.0.1',()=>console.log('IAB_URL=http://127.0.0.1:'+server.address().port));setTimeout(()=>server.close(),180000);
