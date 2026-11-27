// Copied public SDK and real Chrome input. Oracles catch origin-based image
// insertion, Shell-only placement, fixed-size Table stretching, and split Undo.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import http from 'node:http';
const sdkRoot=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-placement-'));
const html=`<!doctype html><style>body{margin:0}#board{position:fixed;inset:0}.other{position:absolute;left:1100px;width:800px;height:600px}</style><div id="board"></div><script type="module">import{createRuntime}from'./axiom.mjs';window.createRuntime=createRuntime;window.packets=[];window.sdk=await createRuntime({mount:document.getElementById('board'),documentId:'ba720000000000000000000000000001',permission:'localSingleUser',onOperation:b=>packets.push([...b])});</script>`;
let server,browser;const result={checks:{},observations:{},errors:[]};
try{
 await fs.cp(sdkRoot,root,{recursive:true});await fs.writeFile(path.join(root,'index.html'),html);await fs.mkdir(path.dirname(output),{recursive:true});
 const demo=await fs.readFile(path.join(root,'example.html'),'utf8');await fs.writeFile(path.join(root,'example.html'),demo.replace('sdk = await createRuntime','sdk = window.sdk = await createRuntime'));
 server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://local').pathname.replace(/\/$/,'/index.html'));if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:process.argv[4]??'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const page=await browser.newPage({viewport:{width:1000,height:750},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));
 const pause=()=>page.waitForTimeout(140);
 const open=async(name='/')=>{await page.goto('http://127.0.0.1:'+server.address().port+name);await page.waitForFunction(()=>window.sdk);await pause();};
 const read=()=>page.evaluate(()=>({state:sdk.readState(),table:sdk.table.read(),text:sdk.text.read(),section:sdk.readSection()}));
 const point=s=>s.tool===1&&s.selectionMode===0&&s.selected.length===1;
 const arm=async name=>{try{await page.evaluate(n=>sdk.selectShape(n),name);result.checks[name+'ToolAvailable']=true;return true;}catch(e){result.checks[name+'ToolAvailable']=false;result.observations[name+'ToolError']=String(e);return false;}};
 const down=async(x=200,y=200)=>{await page.mouse.move(x,y);await page.mouse.down();};
 const up=async(x=620,y=410)=>{await page.mouse.move(x,y,{steps:12});await page.mouse.up();await pause();};
 await open();await page.evaluate(async()=>{await sdk.panBy(180,-90);await sdk.zoomAt(0,0,2);const c=document.createElement('canvas');c.width=128;c.height=64;c.getContext('2d').fillRect(0,0,128,64);const b=await new Promise(r=>c.toBlob(r));await sdk.importImage(new Uint8Array(await b.arrayBuffer()),'image/png');});await pause();
 const image=await page.evaluate(()=>{const s=sdk.readState(),r=sdk.readSelection(),i=sdk.readSelectedImage();return{state:s,selection:r,image:i,center:[(r.transform[4]+r.transform[0]*i.content.width/2+r.transform[2]*i.content.height/2)*s.camera.scale+s.camera.x,(r.transform[5]+r.transform[1]*i.content.width/2+r.transform[3]*i.content.height/2)*s.camera.scale+s.camera.y]};});result.observations.image=image;result.checks.imageCenteredInCurrentViewport=Math.abs(image.center[0]-500)<.6&&Math.abs(image.center[1]-375)<.6;
 await open();await page.evaluate(()=>sdk.table.create({x:200,y:200}));await pause();const initial=await read();result.checks.programmaticDefault2x2=initial.table.rows.length===2&&initial.table.columns.length===2;await page.keyboard.insertText('A\nB\nC\nD\nE');await pause();const grown=await read();result.observations.rowGrowth=grown.table;
 result.checks.textGrowsEditedRow=grown.table.rows[0].size>48&&grown.table.rows[1].size===48;
 const below=await page.evaluate(async height=>{const png=await sdk.exportImage({format:'png',bounds:[205,249,310,200+height-3],padding:0,scale:1});const bitmap=await createImageBitmap(new Blob([png.bytes],{type:png.mimeType}));const c=document.createElement('canvas');c.width=bitmap.width;c.height=bitmap.height;const g=c.getContext('2d');g.drawImage(bitmap,0,0);bitmap.close();let dark=0;const p=g.getImageData(0,0,c.width,c.height).data;for(let i=0;i<p.length;i+=4)if(p[i]<150&&p[i+1]<150&&p[i+2]<150)dark++;return dark;},grown.table.rows[0].size);result.checks.exportIncludesTextBelowOldRowHeight=below>20;
 result.checks.growthPreservesAxes=grown.table.columns.every((c,i)=>c.id===initial.table.columns[i].id&&c.size===120)&&grown.table.rows.every((r,i)=>r.id===initial.table.rows[i].id);
 await page.keyboard.press('Control+Enter');await page.evaluate(()=>sdk.undo());await pause();let a=await read();result.checks.oneUndoRestoresTextAndHeight=a.table.rows[0].size===48&&a.table.cells[0].text==='';await page.evaluate(()=>sdk.redo());await pause();a=await read();result.checks.redoRestoresTextAndHeight=a.table.rows[0].size===grown.table.rows[0].size&&a.table.cells[0].text==='A\nB\nC\nD\nE';
 const persisted=await page.evaluate(async()=>{const state=sdk.readState(),table=sdk.table.read(),snapshot=await sdk.captureSnapshot({generation:state.generation,documentRevision:'1',recoveryFrontier:[8,1,16,1,24,0]});const mount=document.createElement('div');mount.className='other';document.body.append(mount);const restored=await createRuntime({mount,documentId:sdk.readState().documentId,builtinFonts:false,permission:'localSingleUser',restore:{bytes:snapshot.bytes,metadata:{documentRevision:'1',recoveryFrontier:[8,1,16,1,24,0],checkpointDigest:snapshot.checkpointDigest,resourceBinding:'77'}}});const saved=restored.table.read(table.objectId);await restored.close();mount.remove();return saved;});result.checks.snapshotRetainsHeight=persisted.rows[0].size===grown.table.rows[0].size&&persisted.cells[0].text==='A\nB\nC\nD\nE';
 const remote=await page.evaluate(async()=>{const mount=document.createElement('div');mount.className='other';document.body.append(mount);const r=await createRuntime({mount,documentId:sdk.readState().documentId,permission:'localSingleUser'});for(const b of packets)await r.applyRemoteOperation(new Uint8Array(b));const out={table:r.table.read(sdk.table.read().objectId),phase:r.text.read().phase};await r.close();mount.remove();return out;});result.checks.remoteRetainsHeightAndDoesNotOpenEditor=remote.table.rows[0].size===grown.table.rows[0].size&&remote.phase===0;
 await open();if(await arm('table')){
  const before=await read();await down();await page.mouse.move(620,410,{steps:10});await pause();const during=await read();result.checks.tablePreviewIsTransient=during.state.objects===0&&during.state.generation===before.state.generation&&during.table.available===false;
  const screenshot=await page.screenshot();const pixel=await page.evaluate(async bytes=>{const b=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'})),c=document.createElement('canvas');c.width=b.width;c.height=b.height;const g=c.getContext('2d');g.drawImage(b,0,0);b.close();return [...g.getImageData(320*2,220*2,1,1).data];},[...screenshot]);result.checks.tablePreviewHasGrid=pixel[0]<220&&pixel[2]<240;await page.screenshot({path:output.replace(/\.json$/,'-table-preview.png')});
  await page.mouse.up();await pause();a=await read();result.observations.dragTable=a.table;result.checks.tableDragAddsRowsAndColumns=a.table.rows.length===5&&a.table.columns.length===4&&a.table.rows.every(r=>r.size===48)&&a.table.columns.every(c=>c.size===120);result.checks.tableFinishEditsFirstCell=point(a.state)&&a.text.phase===1&&a.text.token.row===a.table.rows[0].id;
  await open();await arm('table');await page.mouse.click(200,200);await pause();a=await read();result.checks.tableClickMinimum2x2=a.table.rows.length===2&&a.table.columns.length===2;
  await open();await arm('table');await down(200,200);await page.mouse.move(240,220,{steps:4});await page.mouse.up();await pause();a=await read();result.checks.tableSmallDragMinimum2x2=a.table.rows.length===2&&a.table.columns.length===2;
  const tableId=a.table.objectId;await page.evaluate(()=>sdk.undo());await pause();result.checks.newTableUndoesAsOneObject=(await read()).state.objects===0;await page.evaluate(()=>sdk.redo());await pause();a=await read();result.checks.tableRedoDoesNotReenterEditor=a.state.objects===1&&a.text.phase===0&&await page.evaluate(id=>sdk.table.read(id).available,tableId);
  await open();await page.evaluate(()=>sdk.zoomAt(0,0,2));await arm('table');await down(800,600);await up(0,0);a=await read();result.checks.reverseZoomedTableDrag=a.table.rows.length===7&&a.table.columns.length===4&&a.table.columns[0].size===120;
  await open();await arm('table');await down();await page.mouse.move(600,400);await page.evaluate(()=>sdk.canvas.dispatchEvent(new PointerEvent('pointercancel',{pointerId:1,pointerType:'mouse',bubbles:true})));await page.mouse.up();await pause();a=await read();result.checks.cancelTableLeavesNoObject=a.state.objects===0&&a.text.phase===0;
  await open();await page.evaluate(()=>sdk.zoomAt(0,0,.01));await arm('table');await down(100,150);await up(900,650);a=await read();result.checks.tableDragIsBounded=a.table.available&&a.table.rows.length<=64&&a.table.columns.length<=64&&a.table.cells.length<=1024&&a.table.cells.length>100;
  await open();await arm('table');const touch=await page.context().newCDPSession(page);await touch.send('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:5});await touch.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{id:21,x:200,y:200}]});await touch.send('Input.dispatchTouchEvent',{type:'touchStart',touchPoints:[{id:21,x:200,y:200},{id:22,x:260,y:200}]});await touch.send('Input.dispatchTouchEvent',{type:'touchEnd',touchPoints:[]});await pause();result.checks.additionalTouchCancelsPlacement=(await read()).state.objects===0;await touch.detach();
  await open();await arm('table');await down();await page.mouse.move(600,400);await page.evaluate(async()=>{const mount=document.createElement('div');mount.className='other';document.body.append(mount);const ops=[];const r=await createRuntime({mount,documentId:sdk.readState().documentId,builtinFonts:false,permission:'localSingleUser',onOperation:b=>ops.push([...b])});await r.createSticky({size:100,x:20,y:20});await new Promise(r=>setTimeout(r,30));await sdk.applyRemoteOperation(new Uint8Array(ops[0]));await r.close();mount.remove();});await page.mouse.up();await pause();a=await read();result.checks.stalePlacementDoesNotCommitTable=a.state.objects===1&&!a.table.available;
 }
 await open();if(await arm('section')){
  result.checks.sectionToolDoesNotCreate=(await read()).state.objects===0;await page.mouse.click(250,220);await pause();a=await read();result.observations.clickSection=a.section;result.checks.sectionClickDefaultSize=a.section.available&&a.section.boundary[2]===320&&a.section.boundary[3]===240&&point(a.state);
  const sectionId=a.section.objectId;await page.evaluate(()=>sdk.undo());await pause();result.checks.sectionUndoRemovesOnlyCreation=(await read()).state.objects===0;await page.evaluate(()=>sdk.redo());await pause();result.checks.sectionRedoRestoresIdentity=await page.evaluate(id=>sdk.readSection(id).objectId===id,sectionId);
  await open();await arm('section');await down(600,400);await page.mouse.move(250,220,{steps:8});await pause();result.checks.sectionPreviewIsTransient=(await read()).state.objects===0;await page.mouse.up();await pause();a=await read();result.observations.dragSection=a.section;result.checks.sectionDragCustomRange=a.section.available&&Math.abs(a.section.boundary[2]-350)<.1&&Math.abs(a.section.boundary[3]-180)<.1&&point(a.state);
  await open();await arm('section');await down();await page.mouse.move(600,400);await page.evaluate(()=>sdk.canvas.dispatchEvent(new PointerEvent('pointercancel',{pointerId:1,pointerType:'mouse',bubbles:true})));await page.mouse.up();await pause();result.checks.cancelSectionLeavesNoObject=(await read()).state.objects===0;
 }
 await open('/example.html');await page.waitForFunction(()=>document.getElementById('status').textContent.includes('0 objects'));await page.locator('#table-open').click();await pause();result.checks.demoTableButtonArmsTool=await page.evaluate(()=>sdk.readState().creationMode&&sdk.readState().shape===6);await page.mouse.click(180,240);await pause();a=await read();result.checks.demoTableClickCreatesMinimum=a.table.available&&a.table.rows.length===2&&a.table.columns.length===2&&a.text.phase===1;
 await page.keyboard.press('Control+Enter');await page.keyboard.press('Escape');await page.locator('#section-create').click();await pause();result.checks.demoSectionButtonArmsTool=await page.evaluate(()=>sdk.readState().creationMode&&sdk.readState().shape===7);await down(650,200);await up(950,400);a=await read();result.checks.demoSectionDragCreatesCustom=a.section.available&&a.section.boundary[2]===300&&a.section.boundary[3]===200;await page.screenshot({path:output.replace(/\.json$/,'-demo.png')});
 // The inline editor owns the visible caption. Its text origin follows the Section,
 // while the selected perimeter remains visible and the transform handles disappear.
 await open('/example.html');
 await page.locator('#section-create').click();await down(300,330);await up(630,510);
 const beforeEdit=await read(),sectionId=beforeEdit.section.objectId;
 const sectionChrome = async () => {
   const png=await page.screenshot({clip:{x:295,y:270,width:378,height:69}});
   return page.evaluate(async bytes=>{
     const im=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
     const ctx=new OffscreenCanvas(im.width,im.height).getContext('2d');
     ctx.drawImage(im,0,0);const d=im.width/378;im.close();
     const count=(x,y,w,h)=>{
       const pixels=ctx.getImageData(x*d,y*d,w*d,h*d).data;let n=0;
       for(let i=0;i<pixels.length;i+=4)if(pixels[i+2]>pixels[i]+70 && pixels[i+2]>pixels[i+1]+70)n++;
       return n;
     };
     return {corner:count(330,53,10,6),rotation:count(344,29,22,22),oldRotation:count(158,0,24,40),edge:count(200,58,20,4)};
   },[...png]);
 };
 const idleChrome=await sectionChrome();
 const titleFocused=()=>document.activeElement?.getAttribute('aria-label')==='分区标题';
 await page.mouse.dblclick(342,311);await page.waitForFunction(titleFocused);
 const titleBox=()=>page.evaluate(()=>{
   const input=document.activeElement,r=input.getBoundingClientRect(),css=getComputedStyle(input),scale=sdk.readState().camera.scale;
   return {x:r.x,y:r.y,tag:input.tagName,
     width:r.width,height:r.height,bottom:r.bottom,radius:parseFloat(css.borderTopLeftRadius),ring:css.boxShadow};
 });
 const box=await titleBox();
 await pause();const editingChrome=await sectionChrome();
 result.observations.sectionChrome={idle:idleChrome,editing:editingChrome};
 result.checks.selectedSectionShowsResizeWithoutRotation=idleChrome.corner>0 && idleChrome.edge>0 && idleChrome.rotation===0 && idleChrome.oldRotation===0;
 result.checks.sectionEditingShowsOnlyFrame=editingChrome.corner===0 && editingChrome.rotation===0 && editingChrome.edge>0;
 result.observations.sectionEditor=box;
 result.checks.sectionTitleHasCompactRoundedBorder=box.tag==='INPUT' && box.radius>=3 && box.radius<=5 && box.ring!=='none' && box.width<130 && box.bottom<328;
 result.checks.sectionEditorAnchoredAtTitle=Math.abs(box.x-300)<=1&&Math.abs(box.y-298)<=1;
 await page.keyboard.press('Backspace');await pause();
 const emptyBox=await titleBox();
 result.checks.sectionTitleFrameFollowsDraft=emptyBox.width<box.width-8;
 const clip={x:304,y:300,width:56,height:22};
 const pixels=await page.screenshot({clip});
 result.checks.emptyDraftCoversOriginalGlyphs=await page.evaluate(async bytes=>{
   const bmp=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
   const ctx=new OffscreenCanvas(bmp.width,bmp.height).getContext('2d');ctx.drawImage(bmp,0,0);bmp.close();
   const data=ctx.getImageData(0,0,ctx.canvas.width,ctx.canvas.height).data;
   for(let i=0;i<data.length;i+=4)if(data[i]<110&&data[i+1]<110&&data[i+2]<110)return false;
   return true;
 },[...pixels]);
 const documentDuringEdit=await page.evaluate(async()=>{
   const state=sdk.readState(),exp=await sdk.exportImage({format:'png',bounds:[300,298,370,326],padding:0,scale:2});
   const im=await createImageBitmap(new Blob([exp.bytes],{type:exp.mimeType})),ctx=new OffscreenCanvas(im.width,im.height).getContext('2d');
   ctx.drawImage(im,0,0);const data=ctx.getImageData(0,0,im.width,im.height).data;im.close();
   let ink=0;for(let i=0;i<data.length;i+=4)if(data[i]<110&&data[i+1]<110&&data[i+2]<110)ink++;
   return {generation:state.generation,operation:state.lastCommit.operationId,ink};
 });
 result.checks.sectionTitleDraftIsViewLocal=documentDuringEdit.generation===beforeEdit.state.generation &&
   documentDuringEdit.operation===beforeEdit.state.lastCommit.operationId && documentDuringEdit.ink>40;
 await page.keyboard.press('Escape');
 await pause();const restoredChrome=await sectionChrome();
 result.checks.escapeRestoresSectionGrips=restoredChrome.corner===idleChrome.corner && restoredChrome.rotation===0;
 await page.mouse.click(90,435);await pause();const idleAfterDeselect=await sectionChrome();
 result.checks.deselectSectionClearsGrips=idleAfterDeselect.rotation===0 && idleAfterDeselect.corner===0;
 await page.evaluate(id=>sdk.selectSection(id),sectionId);await pause();
 result.checks.sectionEscapePreservesTitle=await page.evaluate(({id,generation})=>sdk.readSection(id).title==='Section' && sdk.readState().generation===generation,{id:sectionId,generation:beforeEdit.state.generation});
 await page.locator('#section-title').focus();await pause();const panelChrome=await sectionChrome();
 result.checks.sectionPanelHidesGrips=panelChrome.corner===0 && panelChrome.rotation===0 && panelChrome.edge===idleChrome.edge;
 await page.keyboard.press('Escape');await pause();
 result.checks.sectionEditorRejectsInvalidOpens=await page.evaluate(async id=>{
   const revision=sdk.readState().selectionRevision;
   const reject=async (target,mode,stamp)=>{try{await sdk.setSectionTitleEditor(target,mode,stamp);return false;}catch{return true;}};
   return await reject(id,'inline',String(BigInt(revision)+1n)) && await reject('00000000000000000000000000000001','inline',revision)
     && await reject(id,'unknown',revision);
 },sectionId);
 await page.mouse.dblclick(438,419);await pause();
 result.checks.sectionBodyDoesNotOpenTitleEditor=!(await page.evaluate(titleFocused));
 if(await page.evaluate(titleFocused))await page.keyboard.press('Escape');
 await page.evaluate(()=>sdk.zoomAt(300,330,2));await pause();
 await page.mouse.dblclick(323,286);await page.waitForFunction(titleFocused);
 const zoomed=await titleBox();
 result.checks.zoomedTitleEditorTracksHeader=Math.abs(zoomed.x-300)<=1&&Math.abs(zoomed.y-266)<=1&&Math.abs(zoomed.height-2*box.height)<=2;
 await page.keyboard.insertText('设计评审');await page.keyboard.press('Enter');await pause();
 result.checks.inlineSectionRenameCommits=await page.evaluate(id=>sdk.readSection(id).title==='设计评审',sectionId);
 await page.evaluate(()=>sdk.undo());await pause();
 result.checks.inlineSectionRenameIsOneUndo=await page.evaluate(id=>sdk.readSection(id).title==='Section',sectionId);
 // The title fits the Section even while selected; only its single-line editor expands.
 await open('/example.html');await page.locator('#section-create').click();await down(320,370);await up(435,535);
 const titleId=(await read()).section.objectId;
 const longTitle='项目评审：'+'甲乙丙丁'.repeat(8)+' END';
 await page.evaluate(([id,title])=>sdk.renameSection(id,title),[titleId,longTitle]);await pause();
 const selectedFrame=await page.evaluate(id=>sdk.readSection(id).titleFrame,titleId);
 // Sample both sides of the authored right edge through the middle of the title chip.
 const badgeEdge=await page.screenshot({clip:{x:430,y:346,width:12,height:6}});
 result.checks.longSectionTitleBorderAlignsAtRight=await page.evaluate(async bytes=>{
   const im=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
   const ctx=new OffscreenCanvas(im.width,im.height).getContext('2d');ctx.drawImage(im,0,0);
   const d=im.width/12, pixel=x=>ctx.getImageData(Math.floor(x*d),Math.floor(3*d),1,1).data;
   const inside=pixel(4.5),outside=pixel(5.5);im.close();
   return inside[0]<235 && inside[2]>inside[0] && outside[0]===255 && outside[1]===255 && outside[2]===255;
 },[...badgeEdge]);
 await page.mouse.click(90,430);await pause();
 const compactFrame=await page.evaluate(id=>sdk.readSection(id).titleFrame,titleId);
 result.observations.sectionCaption={selectedFrame,compactFrame};
 result.checks.sectionTitleStaysFittedOnSelection=compactFrame && selectedFrame && compactFrame.ellipsized && selectedFrame.ellipsized &&
   compactFrame.textWidth<=115 && compactFrame.textWidth===selectedFrame.textWidth &&
   compactFrame.textHeight<25 && selectedFrame.textHeight<25;
 await page.mouse.click(350,342);await pause();
 result.checks.sectionTitleClickSelectsFittedCaption=await page.evaluate(id=>sdk.readState().primary===id && sdk.readSection().titleFrame.ellipsized,titleId);
 await page.mouse.dblclick(350,347);await pause();
 result.checks.expandedSectionTitleIsEditable=await page.evaluate(([id,title])=>{const input=document.activeElement;
   return input?.id==='section-title-editor' && input.value===title && input.getBoundingClientRect().width>115 && !sdk.readSection(id).titleFrame.ellipsized;
 },[titleId,longTitle]);
 if(result.checks.expandedSectionTitleIsEditable){
   const field=page.locator('#section-title-editor');
   await page.keyboard.press('Meta+a');await page.keyboard.insertText('中'.repeat(51));await pause();
   result.checks.sectionTitleLimitsInput=(await field.inputValue())==='中'.repeat(50);
   await page.keyboard.press('Meta+a');await page.keyboard.insertText('一行标题');await page.keyboard.press('Shift+Enter');await pause();
   result.checks.sectionReturnFinishesSingleLine=await page.evaluate(id=>sdk.readSection(id).title==='一行标题' && !document.getElementById('section-title-editor'),titleId);
 }
 result.checks.sectionPublicCommandsEnforceTitle=await page.evaluate(async id=>{
   const invalid=async fn=>{try{await fn();return false;}catch{return true;}};
   await sdk.renameSection(id,'甲'.repeat(50));
   const generation=sdk.readState().generation;
   return await invalid(()=>sdk.renameSection(id,'甲'.repeat(51))) && await invalid(()=>sdk.createSection({title:'甲\n乙'})) &&
     await invalid(()=>sdk.renameSection(id,'甲\u2028乙')) && sdk.readState().generation===generation;
 },titleId);
 await page.evaluate(id=>sdk.selectSection(id),titleId);await pause();
 const beforeHandleDrag=await read();
 await down(455,350);await up(490,420);
 const afterHandleDrag=await page.evaluate(id=>({section:sdk.readSection(id),generation:sdk.readState().generation}),titleId);
 result.checks.formerSectionRotationHandleIsInert=afterHandleDrag.generation===beforeHandleDrag.state.generation &&
   afterHandleDrag.section.titleFrame.toWorld.every((coordinate,i)=>coordinate===beforeHandleDrag.section.titleFrame.toWorld[i]);
 result.status=result.errors.length===0&&Object.values(result.checks).every(Boolean)?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify(result));}
