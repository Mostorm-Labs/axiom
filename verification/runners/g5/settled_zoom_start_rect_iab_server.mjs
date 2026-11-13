import fs from'node:fs/promises';import path from'node:path';import http from'node:http';
const request=JSON.parse(await fs.readFile(process.argv[2],'utf8')),out=process.argv[3];await fs.mkdir(out,{recursive:true});const result={id:request.id,rows:[],errors:[],started_unix:Date.now()/1000};
const server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://local').pathname;if(req.method==='POST'){let text='';for await(const c of req)text+=c;if(n==='/result')result.rows.push(JSON.parse(text));else result.errors.push(text);result.last_unix=Date.now()/1000;await fs.writeFile(path.join(out,'raw.json'),JSON.stringify(result,null,2)+'\n');res.end('ok');return;}
 let body,type='text/javascript';if(n==='/'){body='<title>Axiom isolated rectangle comparison</title><style>html,body{margin:0}#mount{position:relative;width:1200px;height:800px}#status{position:fixed;top:0;left:0;pointer-events:none}</style><div id="mount"></div><pre id="status"></pre><script type="module" src="/run.mjs"></script>';type='text/html';}
 else if(n==='/run-config.json'){body=JSON.stringify(request);type='application/json';}
 else if(n==='/run.mjs')body=await fs.readFile(new URL('./settled_zoom_start_rect_iab_run.mjs',import.meta.url));
 else if(n==='/consumer.mjs')body=await fs.readFile(new URL('./settled_zoom_start_rect_consumer.mjs',import.meta.url));
 else if(n==='/observer.mjs')body=await fs.readFile(new URL('./idle_input_feedback_observer.mjs',import.meta.url));
 else if(n==='/navigation-observer.mjs')body=await fs.readFile(new URL('./navigation_observer.mjs',import.meta.url));
 else if(n==='/readback.mjs')body=await fs.readFile(new URL('./live_erase_feedback_pixels.mjs',import.meta.url));
 else{const m=n.match(/^\/lanes\/([^/]+)\/(sdk|engine)\/(.*)$/);if(!m||m[3].includes('..'))throw Error('path');const lane=request.lanes.find(v=>v.name===m[1]);if(!lane)throw Error('lane');
  if(m[2]==='sdk'&&m[3]==='internal/axiom.mjs')body="import factory from '/lanes/"+lane.name+"/engine/internal/axiom.mjs';export default async options=>{const m=await factory(options);window.idleObservation.engine(m);return m;};";
  else{body=await fs.readFile(path.join(lane.sdk,m[3]));type=m[3].endsWith('.mjs')?'text/javascript':m[3].endsWith('.wasm')?'application/wasm':'application/octet-stream';}}
 res.setHeader('Content-Type',type);res.end(body);
 }catch(e){res.statusCode=404;res.end(String(e));}});server.listen(0,'127.0.0.1',()=>console.log('IAB_URL=http://127.0.0.1:'+server.address().port));setTimeout(()=>server.close(),240000);
