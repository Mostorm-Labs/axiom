// Test corpus only. Every edit below calls the installed public SDK owner.
export async function imageBytes(alternate=false){
 const c=document.createElement('canvas');c.width=160;c.height=100;const x=c.getContext('2d');
 x.fillStyle=alternate?'#cf4433':'#2377bb';x.fillRect(0,0,160,100);x.fillStyle='#ffd355';x.fillRect(alternate?80:10,15,35,70);
 return new Uint8Array(await (await new Promise(r=>c.toBlob(r,'image/png'))).arrayBuffer());
}
export async function buildEditingScene(sdk,{pointer,settle,frames,click}){
 await sdk.selectBrush('round-handdrawn',4);sdk.data.attach({capacity:256,byteCapacity:64*1024*1024});
 const shape=async(name,x,y)=>{await sdk.selectShape(name);const n=frames.length;pointer('pointerdown',x,y);pointer('pointerup',x+120,y+80);await settle(n);};
 await shape('rectangle',40,200);await shape('ellipse',220,200);
 const table=(await sdk.table.create({rows:2,columns:2,x:400,y:170})).objectId;
 let t=sdk.table.read(table);await sdk.table.edit({objectId:table,rowId:t.rows[0].id,columnId:t.columns[0].id});await sdk.text.replace('G5 table before');await sdk.text.finish();
 const sticky=(await sdk.createSticky({size:140,x:40,y:340})).objectId;await sdk.text.edit(sticky);await sdk.text.replace('G5 sticky before');await sdk.text.finish();
 const image=(await sdk.importImage(await imageBytes(),'image/png')).objectId;
 await sdk.resizeImage(image,150,100,true);const b=sdk.readSelection().bounds;await sdk.transformSelection([1,0,0,1,610-b[0],350-b[1]]);
 await sdk.selectTool('ink');const n=frames.length;
 for(let i=0;i<4096;++i)pointer(i===0?'pointerdown':i===4095?'pointerup':'pointermove',50+i*700/4095,100+Math.sin(i*.015)*20);
 await settle(n);await sdk.selectTool('select');await click(50,100);
 if(!sdk.readState().selected.length)throw Error('long stroke target was not selected');
 return {table,sticky,image,line:sdk.readState().primary};
}
export async function applyEditingAction(sdk,targets,kind,{pointer,click}){
 if(kind==='transform'){await sdk.selectTool('select');await click(50,100);await sdk.transformSelection([.9,.05,-.03,1.1,10,4]);}
 else if(kind==='erase'){
  await sdk.setEraser({mode:2,diameter:28});await sdk.selectTool('partialEraser');pointer('pointerdown',390,55);
  for(let i=1;i<12;++i)pointer('pointermove',390,55+i*8);pointer('pointerup',390,151);
 }else if(kind==='text'){
  await sdk.text.edit(targets.sticky);await sdk.text.selectAll();await sdk.text.replace('G5 edited sticky\nShared runtime text, layout and history.');await sdk.text.finish();
 }else if(kind==='table'){
  const t=sdk.table.read(targets.table);await sdk.table.edit({objectId:targets.table,rowId:t.rows[0].id,columnId:t.columns[0].id});await sdk.text.selectAll();await sdk.text.replace('G5 table after\nCell editing');await sdk.text.finish();
 }else if(kind==='resource'){
  await sdk.selectTool('select');await click(680,400);const selected=sdk.readSelectedImage();if(!selected.available||selected.objectId!==targets.image)throw Error('image target was not selected');
  const before=sdk.resources.token(selected.content.resourceId);await sdk.replaceImage(await imageBytes(true),'image/png');
  const after=sdk.readSelectedImage();return {beforeToken:before,afterToken:sdk.resources.token(after.content.resourceId),beforeImage:selected,afterImage:after};
 }else throw Error('unknown complex edit');
 return {};
}
