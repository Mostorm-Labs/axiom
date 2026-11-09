// New performance windows use the frozen original public-SDK input consumer.
// No capture/readback/profile/trace in the primary phase.
import fs from 'node:fs/promises';import path from 'node:path';import http from 'node:http';
import {fileURLToPath}from 'node:url';import{createHash}from 'node:crypto';import{execFileSync}from 'node:child_process';
import{handoffConsumer}from './ur2a_handoff.mjs';import{PERF_CASES,validatePerformance}from './ur2b_exact_query_contract.mjs';
const HERE=path.dirname(fileURLToPath(import.meta.url)),ROOT=path.resolve(HERE,'../../..');
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function performanceWorker(sdkArg,outArg){
 const sdk=path.resolve(sdkArg),out=path.resolve(outArg);await fs.mkdir(out,{recursive:true});
 const fixtures=path.join(ROOT,'.aegis/results/G5-UR1a-attribution-r2/local-comparison/fixtures');const manifest=JSON.parse(await fs.readFile(path.join(fixtures,'fixtures.json')));
 const source=()=>JSON.parse(execFileSync('python3.12',[path.join(HERE,'ur2b_exact_query_runner.py'),'--source-identity'],{encoding:'utf8',maxBuffer:1024*1024}));
 const artifacts=async()=>{const v={};for(const n of(await fs.readdir(sdk,{recursive:true})).sort()){const p=path.join(sdk,n);if((await fs.stat(p)).isFile())v[n]=sha(await fs.readFile(p));}return v;};
 const write=(n,v)=>fs.writeFile(path.join(out,n),JSON.stringify(v,null,2)+'\n');let browser,server,page;
 const r={schema:'g5-ur2b-normal-performance-v1',status:'STARTED',profile:'normal',primary_readbacks:0,primary_snapshots:0,primary_trace:false,cases:[],page_errors:[],source_before:source(),artifacts_before:await artifacts()};
 try{
  server=http.createServer(async(req,res)=>{try{const u=new URL(req.url,'http://127.0.0.1');
   if(u.pathname==='/probe.html'){res.setHeader('Content-Type','text/html');res.end('<body style="margin:0"><div id="mount" style="width:800px;height:500px"></div><script type="module">await ('+handoffConsumer.toString()+')();</script>');return;}
   const [base,suffix]=u.pathname.startsWith('/g5/')?[HERE,u.pathname.slice(4)]:u.pathname.startsWith('/fixtures/')?[fixtures,u.pathname.slice(10)]:[sdk,u.pathname.slice(1)];const p=path.resolve(base,suffix);if(!p.startsWith(base+path.sep))throw Error('path');
   res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(p));
  }catch(e){res.statusCode=404;res.end(String(e));}});
  await new Promise(resolve=>server.listen(0,'127.0.0.1',resolve));
  const {chromium}=await import('../../node_modules/playwright/index.mjs');browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});r.browser={version:browser.version(),headless:true};
  for(const id of PERF_CASES){
   const row={id,failures:[],precondition:'Fresh page, original init and normal exact/idle settle; not cold restoration'};r.cases.push(row);
   page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:2});page.on('pageerror',e=>r.page_errors.push(String(e)));page.on('crash',()=>{r.crash=true;});
   await page.addInitScript(()=>{window.ur2bReadbacks={primary:false,count:0};const proto=WebGL2RenderingContext.prototype,raw=proto.readPixels;proto.readPixels=function(...args){if(window.ur2bReadbacks.primary)++window.ur2bReadbacks.count;return raw.apply(this,args);};});
   await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await page.waitForFunction(()=>!!window.ur2a);
   row.init=await page.evaluate(f=>ur2a.init(f),manifest[id.slice(0,2)]);await write(id+'-init.json',row.init);
   if(await page.evaluate(()=>typeof window.ur2aTrace!=='undefined'))throw Error('normal diagnostics leak');
   await page.evaluate(()=>{ur2bReadbacks.primary=true;});
   row.input=await page.evaluate(id=>ur2a.run(id),id);await write(id+'-input.json',row.input);
   row.settled=await page.evaluate(async()=>{const start=performance.now();for(;;){const s=ur2a.state();if(s.activeContacts===0&&s.lastCommit.presentedGeneration===s.generation&&ur2a.dump().activity.pending===0)return{at:performance.now(),state:s};if(performance.now()-start>20000)throw Error('20s exact/idle bound');await new Promise(resolve=>{resolve.g5SamplerRaf=true;requestAnimationFrame(resolve);});}});
   row.primary_readbacks=await page.evaluate(()=>{ur2bReadbacks.primary=false;return ur2bReadbacks.count;});r.primary_readbacks+=row.primary_readbacks;
   row.final=await page.evaluate(()=>ur2a.dump());await write(id+'-final.json',row.final);
   const expected=id.endsWith('E-reerase')?3n:1n;if(BigInt(row.input.after.generation)!==BigInt(row.input.before.generation)+expected)row.failures.push('non-noop exactly-once commit count');
   row.close=await page.evaluate(()=>ur2a.close());if(!row.close.closed)throw Error('owner close');await page.close();page=null;await write(id+'-result.json',row);
   if(row.failures.length||r.page_errors.length||r.crash)throw Error('performance functional failure');
  }
  r.status='OBSERVED';
 }catch(e){r.status='HARD_FAILURE';r.error=String(e.stack??e);if(page&&!page.isClosed())try{r.partial=await page.evaluate(()=>window.ur2a?.dump());await write('partial.json',r.partial);}catch(x){r.partial_error=String(x);}}
 finally{if(browser)await browser.close();if(server)await new Promise(resolve=>server.close(resolve));r.source_after=source();r.artifacts_after=await artifacts();r.source_stable=r.source_before.source_tree_sha256===r.source_after.source_tree_sha256;r.artifacts_stable=JSON.stringify(r.artifacts_before)===JSON.stringify(r.artifacts_after);r.validation=validatePerformance(r);if(r.validation.status!=='OBSERVED')r.status='HARD_FAILURE';await write('worker-result.json',r);}
 console.log(JSON.stringify({stage:'worker-closed',status:r.status,error:r.error??null}));if(r.status!=='OBSERVED')process.exitCode=2;
}
if(process.argv[1]&&path.resolve(process.argv[1])===fileURLToPath(import.meta.url)){const[sdk,out]=process.argv.slice(2);await performanceWorker(sdk,out);}
