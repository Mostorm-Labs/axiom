// Test corpus only. Every edit below calls the installed public SDK owner.
export async function imageBytes(alternate=false,large=false){
 if(large){
  const c=document.createElement('canvas');c.width=2048;c.height=1024;const x=c.getContext('2d'),g=x.createLinearGradient(0,0,c.width,c.height);
  g.addColorStop(0,alternate?'#cf4433':'#345eb8');g.addColorStop(1,'#f19d42');x.fillStyle=g;x.fillRect(0,0,c.width,c.height);
  const noise=new ImageData(300,250);let seed=alternate?43:42;
  for(let i=0;i<noise.data.length;i+=4){seed=(Math.imul(seed,1664525)+1013904223)>>>0;noise.data[i]=seed>>>24;noise.data[i+1]=(seed>>>16)&255;noise.data[i+2]=(seed>>>8)&255;noise.data[i+3]=255;}x.putImageData(noise,0,0);
  return new Uint8Array(await(await new Promise(r=>c.toBlob(r,'image/png'))).arrayBuffer());
 }
 const c=document.createElement('canvas');c.width=160;c.height=100;const x=c.getContext('2d');
 x.fillStyle=alternate?'#cf4433':'#2377bb';x.fillRect(0,0,160,100);x.fillStyle='#ffd355';x.fillRect(alternate?80:10,15,35,70);
 return new Uint8Array(await (await new Promise(r=>c.toBlob(r,'image/png'))).arrayBuffer());
}
export async function buildEditingScene(sdk,{pointer,settle,frames,click},{largeImage=false}={}){
 await sdk.selectBrush('round-handdrawn',4);sdk.data.attach({capacity:256,byteCapacity:64*1024*1024});
 const shape=async(name,x,y)=>{await sdk.selectShape(name);const n=frames.length;pointer('pointerdown',x,y);pointer('pointerup',x+120,y+80);await settle(n);};
 await shape('rectangle',40,200);await shape('ellipse',220,200);
 const table=(await sdk.table.create({rows:2,columns:2,x:400,y:170})).objectId;
 let t=sdk.table.read(table);await sdk.table.edit({objectId:table,rowId:t.rows[0].id,columnId:t.columns[0].id});await sdk.text.replace('G5 table before');await sdk.text.finish();
 const sticky=(await sdk.createSticky({size:140,x:40,y:340})).objectId;await sdk.text.edit(sticky);await sdk.text.replace('G5 sticky before');await sdk.text.finish();
 const bytes=await imageBytes(false,largeImage),image=(await sdk.importImage(bytes,'image/png')).objectId;
 await sdk.resizeImage(image,150,100,true);const selection=sdk.readSelection(),b=selection.bounds,matrix=[...selection.transform];
 // transformSelection submits an absolute SetTransforms value. Account for
 // the importer's existing viewport-centred placement before moving the image.
 matrix[4]+=610-b[0];matrix[5]+=350-b[1];await sdk.transformSelection(matrix);
 const imageBounds=sdk.readSelection().bounds;
 if(Math.abs(imageBounds[0]-610)>.01||Math.abs(imageBounds[1]-350)>.01)throw Error('image fixture placement');
 await sdk.selectTool('ink');const n=frames.length;
 for(let i=0;i<4096;++i)pointer(i===0?'pointerdown':i===4095?'pointerup':'pointermove',50+i*700/4095,100+Math.sin(i*.015)*20);
 await settle(n);await sdk.selectTool('select');await click(50,100);
 if(!sdk.readState().selected.length)throw Error('long stroke target was not selected');
 return {table,sticky,image,line:sdk.readState().primary,largeImage,imageBytes:bytes.length,imageBounds};
}
export async function applyEditingAction(sdk,targets,kind,{pointer,click,eraseX=390}){
 if(kind==='copy'){
  const before=sdk.readState(),copy=sdk.copySelection(),copied=sdk.readState();
  if(copy.objectCount!==1||before.generation!==copied.generation||before.lastCommit.operationId!==copied.lastCommit.operationId)throw Error('copy must be read-only and select one source');
  await sdk.pasteObjects(copy.bytes,{offset:[24,30]});
  const roots=sdk.readState().selected;if(roots.some(id=>copy.roots.includes(id)))throw Error('paste reused source identity');
  return {copiedObjects:copy.objectCount,clipboardBytes:copy.bytes.length,sourceRoots:copy.roots,pastedRoots:roots};
 }else if(kind==='transform'){await sdk.selectTool('select');await click(50,100);await sdk.transformSelection([.9,.05,-.03,1.1,10,4]);}
 else if(kind==='erase'){
  await sdk.setEraser({mode:2,diameter:28});await sdk.selectTool('partialEraser');pointer('pointerdown',eraseX,55);
  for(let i=1;i<12;++i)pointer('pointermove',eraseX,55+i*8);pointer('pointerup',eraseX,151);
 }else if(kind==='text'){
  await sdk.text.edit(targets.sticky);await sdk.text.selectAll();await sdk.text.replace('G5 edited sticky\nShared runtime text, layout and history.');await sdk.text.finish();
 }else if(kind==='table'){
  const t=sdk.table.read(targets.table);await sdk.table.edit({objectId:targets.table,rowId:t.rows[0].id,columnId:t.columns[0].id});await sdk.text.selectAll();await sdk.text.replace('G5 table after\nCell editing');await sdk.text.finish();
 }else if(kind==='resource'){
  await sdk.selectTool('select');await click(680,400);const selected=sdk.readSelectedImage();if(!selected.available||selected.objectId!==targets.image)throw Error('image target was not selected');
  const before=sdk.resources.token(selected.content.resourceId);await sdk.replaceImage(await imageBytes(true,targets.largeImage),'image/png');
  const after=sdk.readSelectedImage();return {beforeToken:before,afterToken:sdk.resources.token(after.content.resourceId),beforeImage:selected,afterImage:after};
 }else throw Error('unknown complex edit');
 return {};
}
