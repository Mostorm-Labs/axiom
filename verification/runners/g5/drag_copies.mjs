// Public-root SDK diagnostic. Optical readbacks are separate from drag timing.
import {chromium} from '../../node_modules/playwright/index.mjs';
import {readFixtureModule,waitForNavigationProbe} from './fixture_modules.mjs';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {execFileSync} from 'node:child_process';
import {gzipSync} from 'node:zlib';

const [sdkArg,outArg,kind='drag',dprArg='2']=process.argv.slice(2);
if(!['drag','pixel'].includes(kind)||![1,2,4].includes(+dprArg))throw Error('fixture arguments');
const root=path.resolve(sdkArg),out=path.resolve(outArg),dpr=+dprArg;
const hash=b=>createHash('sha256').update(b).digest('hex');
const identity=()=>JSON.parse(execFileSync('python3.12',['-c','import json;from verification.tools.g5_preflight import source_identity,ROOT;print(json.dumps(source_identity(ROOT)))'],{encoding:'utf8',maxBuffer:16*1024*1024}));
async function artifacts(){const v={};for(const n of(await fs.readdir(root,{recursive:true})).sort()){const p=path.join(root,n);if((await fs.stat(p)).isFile())v[n]=hash(await fs.readFile(p));}return v;}
await fs.mkdir(out,{recursive:true});
const extension=String.raw`
Object.assign(probe,{
 async setupCopies(simple){
  sdk=await createRuntime({mount:document.getElementById('mount'),documentId:'99999999999999999999999999999999',permission:'localSingleUser',builtinFonts:false,onError:e=>errors.push(String(e)),onFrame:f=>frames.push({at:performance.now(),...f})});
  sdk.canvas.setPointerCapture=()=>{};sdk.canvas.hasPointerCapture=()=>false;
  await sdk.selectBrush('round-handdrawn',4);await sdk.selectTool('ink');
  let a=0;const n=frames.length,count=simple?128:7200;
  for(let i=0;i<count;++i){
   a+=.018+.085*(.5+.5*Math.sin(i*.009));
   const x=simple?90+620*i/(count-1):340+215*Math.sin(a)+25*Math.sin(a*.17);
   const y=simple?220+6*Math.sin(i*.13):280+140*Math.cos(a*.71)+20*Math.sin(a*.13);
   pointer(i===0?'pointerdown':i===count-1?'pointerup':'pointermove',x,y);if(i%16===0)await raf();
  }
  await settle(n);await sdk.selectTool('select');
  const first=simple?[90,220]:[340+215*Math.sin(.0605)+25*Math.sin(.0605*.17),280+140*Math.cos(.0605*.71)+20*Math.sin(.0605*.13)];
  await click(...first);if(sdk.readSelection().count!==1)throw Error('stroke selection');
  for(let i=0;i<8;++i){const n=frames.length;await sdk.duplicateSelection({offset:[1,1]});await settle(n);}
  await new Promise(r=>setTimeout(r,300));
  if(sdk.readState().objects!==9)throw Error('expected9strokes');
  return{state:sdk.readState(),build:sdk.readBuildInfo(),config:sdk.readBrushConfig(),start:first.map(v=>v+8),pixels:await pixels()};
 },
 async dragCopies(start){
  const before=sdk.readState(),prior=frames.length;
  const value={id:'drag8copies',before,started:performance.now(),samples:[],displayTimes:[],draws:0,colorClears:0,fences:[],longTasks:[],marks:[]};
  active=value;feedbackObserver.begin(value);
  let at=performance.now();pointer('pointerdown',...start);value.downMs=performance.now()-at;
  for(let i=0;i<48;++i){
   at=performance.now();const dx=48*i/47,dy=20*i/47;
   pointer('pointermove',start[0]+dx,start[1]+dy);const dispatchMs=performance.now()-at;
   await raf();value.samples.push({i,at,dispatchMs,frameMs:performance.now()-at,generation:sdk.readState().generation});
  }
  value.inputEnded=performance.now();value.receiptsDuringInput=frames.slice(prior);value.during=sdk.readState();
  at=performance.now();pointer('pointerup',start[0]+48,start[1]+20);value.upMs=performance.now()-at;value.upAt=at;
  await until(()=>frames.slice(prior).some(f=>f.at>=value.upAt),'drag canonical receipt');
  const quiet=performance.now();for(;;){const q=feedbackObserver.activity();if(!q.pending&&q.at-q.lastSdkActivity>=200){value.quiescence=q;break;}if(performance.now()-quiet>20000)throw Error('drag not quiescent');await new Promise(r=>setTimeout(r,25));}
  value.ended=performance.now();feedbackObserver.end(value);active=null;
  value.after=sdk.readState();value.receipts=frames.slice(prior);value.pixels=await pixels();
  value.checks={generationDuringInput:value.samples.every(s=>s.generation===before.generation),operationDuringInput:value.during.lastCommit.operationId===before.lastCommit.operationId,
   oneCommit:BigInt(value.after.generation)===BigInt(before.generation)+1n,objects:value.after.objects===9,selection:JSON.stringify(before.selected)===JSON.stringify(value.after.selected),
   terminal:value.after.activeContacts===0,presented:value.after.lastCommit.presentedGeneration===value.after.generation,camera:JSON.stringify(before.camera)===JSON.stringify(value.after.camera)};
  return value;
 },
 async beginOpticalErase(){await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');await raf();await raf();
  const before=sdk.readState(),value={draws:0,displayTimes:[],colorClears:0,fences:[],longTasks:[]};active=value;
  pointer('pointerdown',580,224);await raf();await raf();pointer('pointermove',585,220);await raf();await raf();active=null;
  return{before,during:sdk.readState(),framebufferWrites:value.draws};},
 async finishOpticalErase(){const before=sdk.readState();const n=frames.length;pointer('pointerup',590,220);await settle(n);return{before,after:sdk.readState()};},
 async closeCopies(){await sdk.close();return{closed:!document.querySelector('#mount canvas'),errors};},
 async opticalBytes(){const c=sdk.canvas,g=c.getContext('webgl2'),previous=g.getParameter(g.FRAMEBUFFER_BINDING),scale=c.width/800;
  const width=Math.round(100*scale),height=Math.round(80*scale),bytes=new Uint8Array(width*height*4);
  g.bindFramebuffer(g.FRAMEBUFFER,null);g.readPixels(Math.round(140*scale),c.height-Math.round(260*scale),width,height,g.RGBA,g.UNSIGNED_BYTE,bytes);g.bindFramebuffer(g.FRAMEBUFFER,previous);
  return{width,height,bytes:Array.from(bytes)};}
});`;
const result={schema:'g5-duplicate-drag-diagnostic-v1',kind,dpr,fixture:{rawSamples:kind==='pixel'?128:7200,duplicates:8,dragMoves:48,profile:'round-handdrawn',revision:4},profiled:process.env.G5_PROFILE==='1',scope:'Synthetic public-root SDK diagnostic, not physical/Gate acceptance',sourceBefore:identity(),artifactBefore:await artifacts(),errors:[]};
await fs.writeFile(path.join(out,'inputs.json.gz'),gzipSync(JSON.stringify(result)));
let server,browser;
try{
 const html=(await fs.readFile('verification/runners/g5/navigation_sampling.html','utf8')).replace('</script>',extension+'</script>');
 server=http.createServer(async(req,res)=>{try{const n=new URL(req.url,'http://local').pathname;const m=await readFixtureModule(n);res.setHeader('Content-Type',n.endsWith('.html')?'text/html':n.endsWith('.mjs')?'text/javascript':n.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(n==='/probe.html'?html:m??await fs.readFile(path.join(root,n)));}catch{res.statusCode=404;res.end();}});
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 const page=await browser.newPage({viewport:{width:900,height:650},deviceScaleFactor:dpr});page.on('pageerror',e=>result.errors.push(String(e)));
 await page.addInitScript(()=>{let word=0x12345678;crypto.getRandomValues=a=>{const b=new Uint8Array(a.buffer,a.byteOffset,a.byteLength);for(let i=0;i<b.length;++i){word^=word<<13;word^=word>>>17;word^=word<<5;b[i]=word&255;}return a;};});
 await page.goto('http://127.0.0.1:'+server.address().port+'/probe.html');await waitForNavigationProbe(page);
 console.log(JSON.stringify({stage:'setup',kind}));result.setup=await page.evaluate(simple=>probe.setupCopies(simple),kind==='pixel');
 await fs.writeFile(path.join(out,'setup.json.gz'),gzipSync(JSON.stringify(result.setup)));
 if(kind==='drag'){
  const cdp=await page.context().newCDPSession(page);if(result.profiled){await cdp.send('Profiler.enable');await cdp.send('Profiler.setSamplingInterval',{interval:1000});await cdp.send('Profiler.start');}
  console.log(JSON.stringify({stage:'drag'}));result.drag=await page.evaluate(start=>probe.dragCopies(start),result.setup.start);
  if(result.profiled){const p=await cdp.send('Profiler.stop');await fs.writeFile(path.join(out,'drag.cpu.json.gz'),gzipSync(JSON.stringify(p.profile)));}
  const values=result.drag.samples.map(s=>s.frameMs).sort((a,b)=>a-b);
  result.summary={downMs:result.drag.downMs,p95FrameMs:values[Math.floor(values.length*.95)],maxFrameMs:values.at(-1),upMs:result.drag.upMs,wholePhaseMs:result.drag.ended-result.drag.started,checks:result.drag.checks};
  if(!Object.values(result.drag.checks).every(Boolean))throw Error('drag input/operation/receipt invariant');
 }else{
  const before=await page.evaluate(()=>probe.opticalBytes());await page.screenshot({path:path.join(out,'before.png')});
  result.erase=await page.evaluate(()=>probe.beginOpticalErase());
  const during=await page.evaluate(()=>probe.opticalBytes());await page.screenshot({path:path.join(out,'during.png')});
  if(before.width!==during.width||before.height!==during.height)throw Error('optical dimensions');
  let interior=0,lost=0,changed=0,maxDelta=0;
  const index=(x,y)=>((before.height-1-y)*before.width+x)*4;
  const dark=(bytes,i)=>bytes[i]<160&&bytes[i+1]<160&&bytes[i+2]<160&&bytes[i+3]===255;
  for(let y=2;y<before.height-2;++y)for(let x=2;x<before.width-2;++x){
   let solid=true;for(let j=-2;j<=2;++j)for(let i=-2;i<=2;++i)solid&&=dark(before.bytes,index(x+i,y+j));
   if(!solid)continue;++interior;const i=index(x,y),delta=Math.max(...[0,1,2,3].map(k=>Math.abs(before.bytes[i+k]-during.bytes[i+k])));maxDelta=Math.max(maxDelta,delta);if(delta>2)++changed;
   if(during.bytes[i]>220&&during.bytes[i+1]>220&&during.bytes[i+2]>220)++lost;
  }
  result.optical={roiLogical:[140,180,240,260],interior,lost,changed,maxDelta,beforeSha256:hash(new Uint8Array(before.bytes)),duringSha256:hash(new Uint8Array(during.bytes))};
  await fs.writeFile(path.join(out,'before.rgba.gz'),gzipSync(new Uint8Array(before.bytes)));await fs.writeFile(path.join(out,'during.rgba.gz'),gzipSync(new Uint8Array(during.bytes)));
  result.finish=await page.evaluate(()=>probe.finishOpticalErase());
  result.optical.ok=interior>100&&changed===0&&result.erase.framebufferWrites>0&&result.erase.during.activeContacts===1&&result.erase.before.generation===result.erase.during.generation&&result.finish.after.activeContacts===0;
  if(!result.optical.ok)throw Error('live erase changed remote non-AA pixels');
 }
 result.close=await page.evaluate(()=>probe.closeCopies());if(!result.close.closed||result.close.errors.length)throw Error('SDK closure');
 result.status='MEASURED';
}catch(e){result.status='BLOCKED_EVIDENCE';result.error=String(e.stack??e);}
finally{
 if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));
 result.sourceAfter=identity();result.artifactAfter=await artifacts();result.sourceStable=JSON.stringify(result.sourceBefore)===JSON.stringify(result.sourceAfter);result.artifactStable=JSON.stringify(result.artifactBefore)===JSON.stringify(result.artifactAfter);
 if(!result.sourceStable||!result.artifactStable||result.errors.length)result.status='BLOCKED_EVIDENCE';
 await fs.writeFile(path.join(out,'observation.json.gz'),gzipSync(JSON.stringify(result)));
 console.log(JSON.stringify({stage:'complete',status:result.status,summary:result.summary,optical:result.optical,error:result.error}));if(result.status!=='MEASURED')process.exitCode=1;
}
