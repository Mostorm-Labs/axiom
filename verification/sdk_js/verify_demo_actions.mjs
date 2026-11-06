// Actual Demo and real Chrome keyboard/pointer input; SDK is copied outside
// the repository. UI commands must reach existing Runtime owners.
import {chromium} from '../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import path from 'node:path';import os from 'node:os';import http from 'node:http';
const sdkRoot=path.resolve(process.argv[2]),output=path.resolve(process.argv[3]);
const root=await fs.mkdtemp(path.join(os.tmpdir(),'axiom-demo-actions-'));
let browser,server;const result={checks:{},observations:{},errors:[]};
try {
 await fs.cp(sdkRoot,root,{recursive:true});const html=await fs.readFile(path.join(root,'example.html'),'utf8');await fs.writeFile(path.join(root,'example.html'),html.replace('sdk = await createRuntime','sdk = window.sdk = await createRuntime'));await fs.mkdir(path.dirname(output),{recursive:true});
 server=http.createServer(async(req,res)=>{try{const p=path.resolve(root,'.'+new URL(req.url,'http://local').pathname);if(!p.startsWith(root+path.sep))throw Error('path');res.setHeader('Content-Type',p.endsWith('.mjs')?'text/javascript':p.endsWith('.wasm')?'application/wasm':p.endsWith('.html')?'text/html':'application/octet-stream');res.end(await fs.readFile(p));}catch{res.statusCode=404;res.end();}});await new Promise(r=>server.listen(0,'127.0.0.1',r));
 browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});const page=await browser.newPage({viewport:{width:1000,height:800},deviceScaleFactor:2});page.on('pageerror',e=>result.errors.push(String(e)));
 const pause=()=>page.waitForTimeout(140);
 const open=async()=>{await page.goto('http://127.0.0.1:'+server.address().port+'/example.html');await page.waitForFunction(()=>window.sdk&&!document.getElementById('tools').inert);await pause();};
 const read=()=>page.evaluate(()=>({state:sdk.readState(),selection:sdk.readSelection(),text:sdk.text.read(),style:sdk.appearance.read()}));
 const draw=async(x=200,y=230,right=420,bottom=390)=>{await page.evaluate(()=>sdk.selectShape('rectangle'));await page.mouse.move(x,y);await page.mouse.down();await page.mouse.move(right,bottom,{steps:6});await page.mouse.up();await pause();};
 const clickIf=async id=>{if(!await page.locator('#'+id).count())return false;await page.locator('#'+id).click();await pause();return true;};
 await open();result.observations.primaryTools=await page.locator('#drawing-tools button').allTextContents();result.checks.primaryToolbarFits=await page.evaluate(()=>{const row=document.getElementById('drawing-tools');return row.scrollWidth<=row.clientWidth+1;});
 result.checks.shapeSecondaryChoiceWorks=false;
 if(await clickIf('shape-tool')){await page.locator('[data-tool="shape:1"]').click();await pause();result.checks.shapeSecondaryChoiceWorks=(await read()).state.shape===1;}
 result.checks.secondaryOptionsUseButtons=await page.locator('#tool-options select').count()===0;
 await page.locator('#eraser-tool').click();result.checks.eraserSecondaryChoiceWorks=false;if(await page.locator('[data-eraser="objectEraser"]').count()){await page.locator('[data-eraser="objectEraser"]').click();await pause();result.checks.eraserSecondaryChoiceWorks=(await read()).state.eraserMode===1&&await page.locator('[data-eraser="objectEraser"]').getAttribute('aria-pressed')==='true';}
 await page.locator('[data-tool="tool:0"]').click();result.checks.brushSecondaryChoiceWorks=false;if(await page.locator('[data-brush="laser-transient"]').count()){await page.locator('[data-brush="laser-transient"]').click();await pause();result.checks.brushSecondaryChoiceWorks=await page.evaluate(()=>sdk.readBrushConfig().family===6)&&await page.locator('[data-brush="laser-transient"]').getAttribute('aria-pressed')==='true';}
 result.checks.selectionSecondaryChoiceWorks=false;await page.locator('[data-tool="tool:1"]').click();if(await page.locator('[data-selection="lasso"]').count()){await page.locator('[data-selection="lasso"]').click();await pause();result.checks.selectionSecondaryChoiceWorks=(await read()).state.selectionMode===2&&await page.locator('[data-selection="lasso"]').getAttribute('aria-pressed')==='true';}
 await open();await draw();const created=await read();result.checks.editOpensExistingText=false;
 await page.screenshot({path:output.replace(/\.json$/,'-object.png')});
 if(await clickIf('object-edit')){await page.keyboard.insertText('ab');await page.keyboard.press('Backspace');await pause();let a=await read();result.checks.editOpensExistingText=a.text.phase===1&&a.text.token.object===created.state.primary&&String.fromCharCode(...a.text.projection)==='a';result.checks.textBackspaceDoesNotDeleteObject=a.state.objects===1;await page.keyboard.press('Control+Enter');await pause();}
 await open();await draw();result.checks.lockShowsOnlyUnlock=false;
 if(await clickIf('object-lock')) {
  const locked=await read();const names=await page.locator('#object-actions button:visible').allTextContents();result.observations.lockedActions=names;result.checks.lockShowsOnlyUnlock=names.length===1&&names[0]==='解锁';
  await page.screenshot({path:output.replace(/\.json$/,'-locked.png')});
  await page.evaluate(()=>sdk.canvas.focus());await page.keyboard.press('Backspace');await pause();result.checks.lockedBackspaceDoesNotDelete=(await read()).state.objects===1;
  const rejected=await page.evaluate(async()=>{const out=[];for(const f of [()=>sdk.deleteSelection(),()=>sdk.orderSelection(true),()=>sdk.text.edit(sdk.readState().primary),()=>sdk.setAppearance({strokeWidth:5})]){try{await f();out.push(false);}catch{out.push(true);}}return out;});result.checks.runtimeRejectsLockedMutations=rejected.every(Boolean);
  await page.mouse.move(300,300);await page.mouse.down();await page.mouse.move(380,340,{steps:6});await page.mouse.up();await pause();
  result.checks.lockPreservesPose=JSON.stringify((await read()).selection.transform)===JSON.stringify(locked.selection.transform);
  await clickIf('object-unlock');result.checks.unlockRestoresActions=await page.locator('#object-edit').isVisible()&&await page.locator('#object-delete').isEnabled();
 }
 await open();await draw();await page.evaluate(()=>sdk.canvas.focus());await page.keyboard.press('Backspace');await pause();result.checks.backspaceDeletesSelectedObject=(await read()).state.objects===0;
 if(result.checks.backspaceDeletesSelectedObject){await page.locator('#undo').click();await pause();result.checks.deleteIsUndoable=(await read()).state.objects===1;}
 await open();await draw();await page.locator('#object-width').focus();await page.keyboard.press('Backspace');await pause();result.checks.inputBackspaceDoesNotDeleteObject=(await read()).state.objects===1;
 await open();await draw();await page.locator('#background-open').click();await page.keyboard.press('Backspace');await pause();result.checks.modalBackspaceDoesNotDeleteObject=(await read()).state.objects===1;await page.locator('#background-cancel').click();
 await open();await draw();const first=(await read()).state.primary;await draw(300,280,500,440);const second=(await read()).state.primary;
 await page.locator('#object-back').click();await page.mouse.click(750,230);await page.mouse.click(350,330);await pause();result.checks.sendToBackChangesHitOrder=(await read()).state.primary===first;
 await page.mouse.click(470,420);await pause();await page.locator('#object-front').click();await page.mouse.click(750,230);await page.mouse.click(350,330);await pause();result.checks.bringToFrontChangesHitOrder=(await read()).state.primary===second;
 for(const shape of ['sticky','table']){await open();await page.evaluate(shape=>sdk.selectShape(shape),shape);await page.mouse.click(250,250);await pause();await page.keyboard.press('Control+Enter');await pause();await page.locator('#object-edit').click();await pause();result.checks[shape+'ActionOpensTextEditor']=(await read()).text.phase===1;}
 await open();await draw();await page.evaluate(()=>sdk.selectShape('ellipse'));await pause();const before=await read();await page.locator('#sketch-roughness').evaluate(el=>{el.value='2';el.dispatchEvent(new Event('input',{bubbles:true}));el.dispatchEvent(new Event('change',{bubbles:true}));});await pause();const after=await read();result.checks.creationDefaultsDoNotEditOldSelection=after.state.generation===before.state.generation&&after.style.values.roughness===before.style.values.roughness&&await page.evaluate(()=>sdk.appearance.readDefaults().values.roughness===2);
 await page.screenshot({path:output.replace(/\.json$/,'-desktop.png')});
 await page.setViewportSize({width:600,height:800});await open();result.checks.primaryToolbarFitsNarrow=await page.evaluate(()=>{const row=document.getElementById('drawing-tools');return row.scrollWidth<=row.clientWidth+1;});await page.screenshot({path:output.replace(/\.json$/,'-narrow.png')});
 await page.setViewportSize({width:360,height:800});await page.locator('#shape-tool').click();await page.locator('#path-curve').scrollIntoViewIfNeeded();result.checks.secondaryOverflowScrolls=await page.evaluate(()=>{const row=document.getElementById('tool-options');return row.scrollWidth>row.clientWidth&&row.scrollLeft>0;});await page.locator('#path-curve').click();await pause();result.checks.overflowChoiceIsReachable=(await read()).state.objects===1;await page.screenshot({path:output.replace(/\.json$/,'-overflow.png')});
 result.status=result.errors.length===0&&Object.values(result.checks).every(Boolean)?'PASS':'FAIL';if(result.status!=='PASS')process.exitCode=1;
}catch(e){result.status='ERROR';result.error=String(e.stack??e);process.exitCode=1;}finally{if(browser)await browser.close();if(server)await new Promise(r=>server.close(r));await fs.rm(root,{recursive:true,force:true});await fs.writeFile(output,JSON.stringify(result,null,2));console.log(JSON.stringify(result));}
