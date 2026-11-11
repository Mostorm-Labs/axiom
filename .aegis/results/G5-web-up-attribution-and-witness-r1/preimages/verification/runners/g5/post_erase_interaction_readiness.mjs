import fs from'node:fs/promises';import path from'node:path';import http from'node:http';
import{chromium}from'../../node_modules/playwright/index.mjs';
import{longConsumer,localizeUndo}from'./erase_undo_readiness.mjs';
import{validateChunk}from'./ur2d_selection_up_contract.mjs';
import{execFileSync}from'node:child_process';
export function caseRoster(){return[
 ...['erase','pan','rectangle'].map(kind=>({kind,variant:'old'})),
 ...['ready','erase','pan','zoom','rectangle','sticky','table','image'].map(kind=>({kind,variant:'candidate'}))];}
export function classifyTiming(start,feedback){if(!Number.isFinite(start)||!Number.isFinite(feedback)||feedback<start)throw Error('invalid timing basis');const ms=feedback-start;return{ms,category:ms<=3000?'within3s':ms<=5000?'3to5s':'over5s',goal_ms:3000,reference_ms:5000};}
export function qualifyFeedback({admitted,positivePixels,correctPose}){return{status:!admitted||!correctPose?'FAILED':positivePixels>0?'QUALIFIED':'MISSING'};}
const facade=`import{createRuntime as original}from'/real/axiom.mjs';export *from'/real/axiom.mjs';export async function createRuntime(options){let sdk;const originalChange=options.onChange;const f=window.undoFixture,bytes=new Uint8Array(await(await fetch('/fixture.axsnap')).arrayBuffer());const result=await original({...options,assetBaseURL:new URL('/real/',location.href),builtinFonts:false,documentId:f.documentId,restore:{bytes,metadata:f.metadata},onChange(s){const v=sdk?.readState()??s;window.undoChanges.push({at:performance.now(),canUndo:v.canUndo,generation:v.generation,activeContacts:v.activeContacts});originalChange?.(s);}});sdk=result;window.rootRuntime=sdk;return sdk;}`;
export async function readinessWorker(request,out){
 await fs.mkdir(out,{recursive:true});const root=path.resolve(new URL('../../..',import.meta.url).pathname);let server,browser,page,sdk;
 const result={id:request.id,status:'STARTED',rows:[],errors:[]};
 try{
  server=http.createServer(async(req,res)=>{try{const name=decodeURIComponent(new URL(req.url,'http://local').pathname);let file;
   if(name==='/axiom.mjs'){res.setHeader('Content-Type','text/javascript');res.end(facade);return;}
   if(name==='/fixture.axsnap')file=request.fixture.path;else if(name.startsWith('/g5/'))file=path.join(root,'verification/runners/g5',name.slice(4));else file=path.join(sdk,name.startsWith('/real/')?name.slice(6):name.slice(1));
   res.setHeader('Content-Type',file.endsWith('.html')?'text/html':file.endsWith('.mjs')?'text/javascript':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');res.end(await fs.readFile(file));}catch(e){res.statusCode=404;res.end(String(e));}});await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({headless:true,executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome'});
  for(const spec of request.cases??caseRoster()){
   sdk=request.variants.find(x=>x.name===spec.variant).sdk;page=await browser.newPage({viewport:{width:1431,height:1010},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));await page.addInitScript(f=>{window.undoFixture=f;window.undoChanges=[];},request.fixture);await page.goto('http://127.0.0.1:'+server.address().port+'/example.html');await page.waitForFunction(()=>window.rootRuntime&&document.getElementById('undo'));
   const directory=path.join(out,spec.variant+'-'+spec.kind);await fs.mkdir(directory);const transfers=new Map();
   await page.exposeFunction('persistChunk',async c=>{const old=transfers.get(c.name);if(old?.complete)throw Error('replayed buffer');const next=validateChunk(c,old),bytes=Buffer.from(c.base64,'base64'),file=path.join(directory,c.name+'.partial');const h=await fs.open(file,c.offset?'r+':'wx');try{await h.write(bytes,0,bytes.length,c.offset);}finally{await h.close();}if(next.complete)await fs.rename(file,path.join(directory,c.name));transfers.set(c.name,next);return{nextOffset:next.nextOffset};});
   console.log(JSON.stringify({stage:'readiness-start',...spec}));
   const row=await page.evaluate(longConsumer,{moves:1600,busySuccessor:spec.kind!=='ready',action:spec.kind,mixed:true});
   row.variant=spec.variant;row.kind=spec.kind;row.localization=localizeUndo(row);
   row.timing=classifyTiming(row.up.start,row.actionFeedbackAt??row.readyAt);
   row.feedback=spec.kind==='ready'?{status:row.after.activeContacts===0&&row.after.canUndo&&row.after.generation===row.after.lastCommit.presentedGeneration&&row.activity.pending===0?'QUALIFIED':'FAILED'}:qualifyFeedback({admitted:row.actionAdmitted,positivePixels:row.actionPositivePixels??0,correctPose:row.actionPoseCorrect});
   row.snapshotRoundtrip=JSON.parse(execFileSync(path.join(root,'.aegis/results/G5-UR2d-control-r1/snapshot-roundtrip'),[path.join(directory,'final.axsnap'),path.join(directory,'redo.axsnap')],{encoding:'utf8',timeout:10000}));
   if(!row.snapshotRoundtrip.objects_equal||!row.snapshotRoundtrip.document_digest_equal)throw Error('readiness UndoRedo canonical mismatch');
   row.status=row.feedback.status==='QUALIFIED'?'QUALIFIED':'FAILED';
   await fs.writeFile(path.join(out,spec.variant+'-'+spec.kind+'.json'),JSON.stringify(row,null,2)+'\n');result.rows.push(row);await page.close();page=null;
   if(row.status!=='QUALIFIED')throw Error('successor actual feedback '+row.feedback.status+' '+spec.kind);
   console.log(JSON.stringify({stage:'readiness-complete',...spec,timing:row.timing}));
  }
  if(result.errors.length)throw Error('actual Demo errors');result.status='OBSERVED';
 }catch(e){result.status='FAILED';result.error=String(e.stack??e);if(page)try{result.partial=await page.evaluate(()=>window.undoPartial);}catch{}}
 finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.writeFile(path.join(out,'worker-result.json'),JSON.stringify(result,null,2)+'\n');console.log(JSON.stringify({stage:'closed',status:result.status,error:result.error}));}return result;
}
if(process.argv[1]===new URL(import.meta.url).pathname){const request=JSON.parse(await fs.readFile(process.argv[2],'utf8'));if((await readinessWorker(request,path.resolve(process.argv[3]))).status!=='OBSERVED')process.exitCode=1;}
