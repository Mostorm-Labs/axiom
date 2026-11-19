import fs from 'node:fs/promises';import path from 'node:path';
import {createServer} from './pending_navigation_progress_web.mjs';
const request=JSON.parse(await fs.readFile(process.argv[2],'utf8')),out=process.argv[3];request.iab=true;await fs.mkdir(out,{recursive:true});
const result={id:request.id,rows:[],errors:[],started_unix:Date.now()/1000};const save=()=>fs.writeFile(path.join(out,'raw.json'),JSON.stringify(result,null,2)+'\n');
const server=createServer(request,result,save);server.listen(0,'127.0.0.1',()=>console.log('IAB_URL=http://127.0.0.1:'+server.address().port));setTimeout(()=>server.close(),180000);
