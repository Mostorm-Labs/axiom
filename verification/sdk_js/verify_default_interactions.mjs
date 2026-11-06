// Real, copied public SDK: catches retained creation tools, missing editor entry,
// reversed Enter semantics, canonical drag previews, and inherited lasso mode.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
const sdkRoot=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-defaults-'));
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}</style><div id="board"></div><script type="module">import{createRuntime}from'./axiom.mjs';window.sdk=await createRuntime({mount:document.getElementById('board'),documentId:'ba710000000000000000000000000001',permission:'localSingleUser'});</script>`;
const result={checks:{},observations:{},errors:[]};let server,browser;
try{
 await fs.cp(sdkRoot,root,{recursive:true});await fs.writeFile(path.join(root,'index.html'),html);await fs.mkdir(path.dirname(output),{recursive:true});
 server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://local').pathname.replace(/\/$/,'/index.html'));if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:process.argv[4]??'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const page=await browser.newPage({viewport:{width:1000,height:750},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));
 const pause=()=>page.waitForTimeout(120);
 const open=async()=>{await page.goto('http://127.0.0.1:'+server.address().port);await page.waitForFunction(()=>window.sdk);await pause();};
 const capture=()=>page.evaluate(()=>{const s=sdk.readState(),t=sdk.text.read();return {tool:s.tool,creation:s.creationMode,mode:s.selectionMode,selected:s.selected,objects:s.objects,generation:s.generation,primary:s.primary,phase:t.phase,width:t.width,text:String.fromCharCode(...t.projection),textObject:t.token.object,row:t.token.row,column:t.token.column};});
 const pointSelected=s=>s.tool===1&&s.mode===0&&!s.creation&&s.selected.length===1;
 const start=async(x=250,y=300)=>{await page.mouse.move(x,y);await page.mouse.down();};
 const end=async(x=600,y=420)=>{await page.mouse.move(x,y,{steps:12});await page.mouse.up();await pause();};
 for(const name of ['rectangle','ellipse','triangle','line','arrow']){
  await open();await page.evaluate(async n=>{await sdk.setSelectionMode('lasso');await sdk.selectShape(n);},name);await start();await end();const s=await capture();result.observations[name]=s;result.checks[name+'SelectedInPointMode']=pointSelected(s)&&s.objects===1&&s.phase===0;
  if(name==='rectangle'){
   await page.evaluate(()=>sdk.text.edit(sdk.readState().primary));await pause();await page.keyboard.insertText('shape');await page.keyboard.press('Enter');await page.keyboard.insertText('label');await page.keyboard.press('Control+Enter');await pause();const label=await capture();result.checks.shapeLabelNewlineAndFinish=label.phase===0&&label.text==='shape\nlabel'&&pointSelected(label)&&label.primary===s.primary;
  }
 }
 await open();await page.evaluate(()=>sdk.selectTool('text'));await page.mouse.click(250,300);await pause();let s=await capture();result.checks.textClickDefaultWidth=s.phase===1&&s.width===320&&s.objects===0;
 await page.keyboard.insertText('hello');await page.keyboard.press('Enter');await pause();s=await capture();result.checks.enterInsertsParagraph=s.phase===1&&s.text==='hello\n';
 await page.keyboard.press('Control+Enter');await pause();s=await capture();result.observations.controlFinish=s;result.checks.controlEnterFinishesAndSelects=s.phase===0&&pointSelected(s)&&s.objects===1;
 await open();await page.evaluate(()=>sdk.selectTool('text'));await page.mouse.click(250,300);await pause();await page.keyboard.insertText('中文');await page.keyboard.press('Meta+Enter');await pause();s=await capture();result.checks.commandEnterFinishesAndSelects=s.phase===0&&pointSelected(s);
 await open();await page.evaluate(()=>sdk.selectTool('text'));await page.mouse.click(250,300);await pause();await page.keyboard.insertText('native');await page.evaluate(()=>document.activeElement.dispatchEvent(new InputEvent('beforeinput',{inputType:'insertParagraph',bubbles:true,cancelable:true})));await pause();s=await capture();result.checks.beforeInputParagraphIsNewline=s.phase===1&&s.text==='native\n';
 await page.evaluate(()=>document.activeElement.dispatchEvent(new KeyboardEvent('keydown',{key:'Enter',isComposing:true,bubbles:true,cancelable:true})));await pause();s=await capture();result.checks.composingEnterDoesNotFinish=s.phase===1&&s.text==='native\n';
 await page.mouse.click(900,650);await pause();s=await capture();result.checks.outsideClickFinishesWithoutExtraDraft=s.phase===0&&pointSelected(s)&&s.objects===1;
 await open();await page.evaluate(()=>sdk.selectTool('text'));const before=await capture();await start();await page.mouse.move(450,340,{steps:10});await pause();const during=await capture();result.checks.textDragIsTransient=during.objects===0&&during.generation===before.generation&&during.phase===0;
 const png=await page.screenshot();const pixels=await page.evaluate(async bytes=>{const i=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));const c=document.createElement('canvas');c.width=i.width;c.height=i.height;const g=c.getContext('2d');g.drawImage(i,0,0);i.close();const row=[];for(let x=270;x<430;x++){const p=g.getImageData(x*2,300*2,1,1).data;row.push(p[2]>p[0]+15&&p[0]<245);}return row;},[...png]);result.checks.textDragHasDashedPurpleOutline=pixels.filter(Boolean).length>25&&pixels.filter(v=>!v).length>25;
 await page.screenshot({path:output.replace(/\.json$/,'-text-drag.png')});await page.mouse.up();await pause();s=await capture();result.observations.dragEditor=s;result.checks.textDragUsesRequestedWidth=s.phase===1&&Math.abs(s.width-200)<1&&s.objects===0;
 await page.keyboard.insertText('drag width');await page.keyboard.press('Control+Enter');await pause();s=await capture();result.checks.dragTextCommitsAndSelects=s.phase===0&&pointSelected(s)&&s.objects===1;
 await open();await page.evaluate(()=>sdk.selectTool('text'));await start();await page.mouse.move(450,340);await page.evaluate(()=>sdk.canvas.dispatchEvent(new PointerEvent('pointercancel',{pointerId:1,pointerType:'mouse',bubbles:true})));await page.mouse.up();await pause();s=await capture();result.checks.textDragCancelLeavesNoDraft=s.phase===0&&s.objects===0;
 await open();await page.evaluate(()=>sdk.selectTool('text'));await page.mouse.click(250,300);await pause();await page.keyboard.press('Control+Enter');await pause();s=await capture();result.checks.emptyTextFinishLeavesNoObject=s.phase===0&&s.objects===0&&s.tool===1&&s.mode===0;
 await open();await page.evaluate(()=>sdk.selectShape('sticky'));await start();await end();s=await capture();result.observations.sticky=s;result.checks.stickyOpensEditor=s.phase===1&&s.objects===1;
 await page.keyboard.insertText('便签');await page.keyboard.press('Enter');await page.keyboard.insertText('第二行');await page.keyboard.press('Control+Enter');await pause();s=await capture();const sticky=await page.evaluate(()=>sdk.readSelectedSticky());result.checks.stickyFinishSelectsParent=s.phase===0&&pointSelected(s)&&sticky.available&&s.primary===sticky.objectId&&s.text==='便签\n第二行';
 await open();await page.evaluate(()=>sdk.table.create({rows:2,columns:2,x:250,y:300}));await pause();s=await capture();const table=await page.evaluate(()=>sdk.table.read());result.observations.table=s;result.checks.tableOpensFirstCell=s.phase===1&&s.row===table.rows[0].id&&s.column===table.columns[0].id;
 await page.keyboard.insertText('cell');await page.keyboard.press('Enter');await page.keyboard.insertText('line');await page.keyboard.press('Control+Enter');await pause();s=await capture();result.checks.tableFinishSelectsTable=s.phase===0&&pointSelected(s)&&s.primary===table.objectId&&s.text==='cell\nline';
 await open();await page.evaluate(async()=>{await sdk.zoomAt(0,0,2);await sdk.selectTool('text');});await start(600,300);await end(400,360);s=await capture();result.checks.reverseDragUsesWorldWidth=s.phase===1&&Math.abs(s.width-100)<1&&s.objects===0;
 await open();await page.evaluate(async()=>{await sdk.setSelectionMode('lasso');const c=document.createElement('canvas');c.width=c.height=32;c.getContext('2d').fillRect(0,0,32,32);const blob=await new Promise(r=>c.toBlob(r));await sdk.importImage(new Uint8Array(await blob.arrayBuffer()),'image/png');});await pause();s=await capture();result.checks.imageAlwaysSelectsInPointMode=pointSelected(s)&&s.objects===1;
 result.status=result.errors.length===0&&Object.values(result.checks).every(Boolean)?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify(result));}
