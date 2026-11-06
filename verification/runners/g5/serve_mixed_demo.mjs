// External verification host. The installed consumer package remains unchanged.
import fs from 'node:fs/promises';
import path from 'node:path';
import http from 'node:http';
import {readFixtureModule} from './fixture_modules.mjs';
const sdk=path.resolve(process.argv[2]),port=Number(process.argv[3]??4212);
const fixture=await fs.readFile(new URL('./navigation_sampling.html',import.meta.url),'utf8');
const controls=`<style>#mixed-controls{position:fixed;right:8px;top:8px;z-index:100;background:white;border:1px solid #bbb;padding:8px;font:12px sans-serif;max-width:280px}#mixed-result{white-space:pre-wrap}</style>
<div id="mixed-controls">2 shapes + sticky + image + table + long stroke<br><button id="mixed-run" disabled>测量平移／缩放</button><pre id="mixed-result">正在加载…</pre></div>
<script type="module">
window.addEventListener('load',async()=>{
 const result=document.getElementById('mixed-result'),button=document.getElementById('mixed-run');
 try{
  const query=new URLSearchParams(location.search),moved=query.get('moved')==='1';
  const dense=query.get('dense')==='1';
  const lines=Number(query.get('lines')??(dense?5:1));if(!Number.isInteger(lines)||lines<1||lines>8)throw Error('lines must be1..8');
  const options=dense?{width:636,height:923,shapes:2,lines,points:2048,scribble:true,scribblePhase:.63,zoom:1.59,drawLast:true,uiReads:true}:
    {width:636,height:914,shapes:2,lines,points:8192,scribble:true,zoom:.57,drawZoom:.57,transformedStroke:moved,uiReads:true,brush:{thinning:.99,streamline:.13,smoothing:.61}};
  const started=performance.now();
  while(!window.probe){if(performance.now()-started>30000)throw Error('sampling fixture import timeout');await new Promise(r=>setTimeout(r,25));}
  const setup=await probe.setup('mixed',false,'round-handdrawn',4,options);
  result.textContent=setup.state.objects+' objects · Zoom '+setup.state.camera.scale.toFixed(2)+' · '+setup.build.sourceRevision.slice(0,8)+'\\n可直接使用触控板。'+(moved?'笔迹移动对照':'原位笔迹');button.disabled=false;
  button.onclick=async()=>{button.disabled=true;try{const p=await probe.measure('native-browser-mixed',{mode:'wheel',burst:4,panOnly:false});
    const gaps=p.displayTimes.filter(t=>t>=p.started&&t<=p.inputEnded);const bounded=[p.started,...gaps,p.inputEnded];
    const gap=Math.max(...bounded.slice(1).map((t,i)=>t-bounded[i]));
    const {summarizeInputFeedback,summarizeRuntimeSubmits}=await import('/feedback_metrics.mjs');
    result.textContent=JSON.stringify({legacy_gap_ms:gap,feedback_v2:summarizeInputFeedback(p),runtime_submit_v2:summarizeRuntimeSubmits(p),raf_max_ms:Math.max(...p.samples.map(s=>s.interval)),settle_ms:p.settleMilliseconds,checks:p.checks,pixel_sha256:p.pixels.sha256},null,2);
   }catch(e){result.textContent=String(e.stack||e);}finally{button.disabled=false;}};
 }catch(e){result.textContent=String(e.stack||e);}
});
</script>`;
const types={'.html':'text/html','.mjs':'text/javascript','.wasm':'application/wasm','.json':'application/json'};
const server=http.createServer(async(req,res)=>{try{
 const url=new URL(req.url,'http://localhost');
 if(url.pathname==='/mixed-navigation.html'){res.setHeader('Content-Type','text/html');res.end(fixture+controls);return;}
 const module=await readFixtureModule(url.pathname);
 if(module){res.setHeader('Content-Type','text/javascript');res.end(module);return;}
 const name=url.pathname.slice(1),file=path.resolve(sdk,name);
 if(!file.startsWith(sdk+path.sep))throw Error('path');
 res.setHeader('Content-Type',types[path.extname(name)]??'application/octet-stream');res.end(await fs.readFile(file));
}catch{res.statusCode=404;res.end();}});
server.listen(port,'127.0.0.1',()=>console.log('http://127.0.0.1:'+port+'/mixed-navigation.html'));
