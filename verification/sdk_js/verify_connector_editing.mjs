import {chromium} from '../node_modules/playwright/index.mjs';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import path from 'node:path';

const root=path.resolve(process.argv[2]), out=path.resolve(process.argv[3]);
const dpr=Number(process.argv[4]??1);
await fs.mkdir(out,{recursive:true});
const server=http.createServer(async(req,res)=>{
  try {
    const file=path.resolve(root,'.'+new URL(req.url,'http://localhost').pathname);
    if(!file.startsWith(root+path.sep))throw Error('file');
    res.setHeader('Content-Type',file.endsWith('.mjs')?'text/javascript':file.endsWith('.html')?'text/html':file.endsWith('.wasm')?'application/wasm':'application/octet-stream');
    res.end(await fs.readFile(file));
  } catch {res.writeHead(404).end();}
});
await new Promise(r=>server.listen(0,'127.0.0.1',r));
const browser=await chromium.launch({channel:'chrome',args:['--enable-unsafe-swiftshader']});
const page=await browser.newPage({viewport:{width:1220,height:920},deviceScaleFactor:dpr});
const cases=[],errors=[];
page.on('pageerror',e=>errors.push(String(e)));
await page.route('**/example.html',async route=>{
  const r=await route.fetch();
  await route.fulfill({response:r,body:(await r.text()).replace('sdk = await createRuntime',
    'window.packets=[]; sdk = window.sdk = await createRuntime').replace("permission: 'localSingleUser', onError: fail",
    "permission: 'localSingleUser', onOperation:bytes=>window.packets.push(new Uint8Array(bytes)), onError: fail")});
});
const open=async()=>{await page.goto('http://127.0.0.1:'+server.address().port+'/example.html');await page.waitForFunction(()=>window.sdk&&!document.querySelector('#tools').inert);};
const drag=async(a,b,release=true)=>{await page.mouse.move(...a);await page.mouse.down();await page.mouse.move(...b,{steps:7});if(release)await page.mouse.up();await page.waitForTimeout(120);};
const read=id=>page.evaluate(id=>sdk.readConnector(id),id);
const count=()=>page.evaluate(()=>packets.length);
const ids=id=>page.evaluate(id=>sdk.readSection(id).children,id);
const near=(a,b,tol=.1)=>a.length===b.length&&a.every((v,i)=>Math.abs(v-b[i])<=tol);
const world=(t,[x,y])=>[t[0]*x+t[2]*y+t[4],t[1]*x+t[3]*y+t[5]];
async function draw(kind,a,b){
  const n=await page.evaluate(()=>sdk.readState().objects);
  await page.evaluate(kind=>sdk.selectShape(kind),kind);await drag(a,b);
  await page.waitForFunction(n=>sdk.readState().objects>n,n);
  return page.evaluate(()=>sdk.readSelection());
}
async function test(name,fn){try{await fn();cases.push({name,passed:true});}catch(e){cases.push({name,passed:false,error:String(e.stack)});throw e;}}

try {
  await open();
  await test('repeated port clicks preview and place separate connected siblings',async()=>{
    await page.locator('#shape-tool').click();
    const source=await draw('rectangle',[570,305],[670,375]);
    const port=[620,source.bounds[3]+28],children=[],wires=[],tips=[];
    for(let i=0;i<5;i++){
      if(i)await page.mouse.click(620,340);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
      await page.mouse.move(1060,590);
      const before=await count(),total=await page.evaluate(()=>sdk.readState().objects);
      await page.mouse.move(...port);
      await page.waitForTimeout(100);
      const preview=await page.screenshot();
      assert.equal(await count(),before,'hover does not publish');
      await page.mouse.down();await page.mouse.up();
      await page.waitForFunction(n=>sdk.readState().objects===n+2,total,{timeout:3000});
      const wire=await read();assert.equal(wire.start.objectId,source.primary);assert.equal(wire.start.port,3);
      assert.equal(wire.end.port,1);assert(!children.includes(wire.end.objectId));
      assert.equal(await count(),before+1,'one undoable creation');
      const tip=world(wire.worldTransform,wire.route.at(-1));
      if(i)assert.equal(tip[1],tips[0][1],'siblings stay on one row');
      for(const prior of tips)assert(Math.abs(tip[0]-prior[0])>=110,'shape footprints stay separate');
      const previewColor=await page.evaluate(async({png,x,y,dpr})=>{
        const image=await createImageBitmap(new Blob([new Uint8Array(png)],{type:'image/png'}));
        const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');ctx.drawImage(image,0,0);
        const pixel=Array.from(ctx.getImageData(Math.floor(x*dpr),Math.floor(y*dpr),1,1).data);image.close();return pixel;
      },{png:[...preview],x:tip[0],y:tip[1]+30,dpr});
      assert(previewColor[0]<255 && previewColor[2]>previewColor[0],'ghost occupies the chosen slot');
      tips.push(tip);children.push(wire.end.objectId);wires.push(wire.objectId);
    }
    for(const [i,id] of wires.entries()){
      const c=await read(id);assert.deepEqual(world(c.worldTransform,c.route.at(-1)),tips[i]);
      assert.equal(c.end.objectId,children[i]);
    }
    assert.equal(tips[0][0],620);assert(tips[1][0]>620 && tips[2][0]<620,'expand on both sides');
    await page.mouse.click(1080,620);
    await page.screenshot({clip:{x:225,y:290,width:795,height:335},path:path.join(out,'repeated-port-children.png')});
    await page.evaluate(()=>sdk.undo());assert.equal(await page.evaluate(()=>sdk.readState().objects),9);
    await page.evaluate(()=>sdk.redo());assert.equal(await page.evaluate(()=>sdk.readState().objects),11);
  });
  await open();
  await test('continuing the same row after camera zoom preserves its board coordinates',async()=>{
    const source=await draw('rectangle',[420,300],[500,360]),cx=460,cy=330;
    await page.mouse.click(cx,source.bounds[3]+28);
    await page.waitForFunction(()=>sdk.readConnector().available);
    const first=await read(),a=world(first.worldTransform,first.route.at(-1));
    await page.mouse.click(cx,cy);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
    await page.evaluate(()=>sdk.zoomAt(460,330,2));
    const cam=await page.evaluate(()=>sdk.readState().camera);
    await page.mouse.click(cx*cam.scale+cam.x,source.bounds[3]*cam.scale+cam.y+28);
    await page.waitForFunction(n=>sdk.readState().objects===n,5,{timeout:3000});
    const second=await read(),b=world(second.worldTransform,second.route.at(-1));
    assert.equal(second.start.objectId,source.primary);
    assert(Math.abs(a[1]-b[1])<.02 && Math.abs(a[0]-b[0])>80,'siblings keep one row across zoom: '+a+' / '+b);
  });
  await open();
  await test('a pre-existing locked shape sends a right-port click to a free slot in the column',async()=>{
    const source=await draw('rectangle',[335,345],[415,405]);
    const blocker=await draw('rectangle',[548,335],[659,415]);
    await page.evaluate(()=>sdk.lockSelection(true));
    const port=[source.bounds[2]+28,375],landings=[];
    for(let i=0;i<2;i++){
      await page.mouse.click(375,375);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
      const before=await count(),total=await page.evaluate(()=>sdk.readState().objects);
      await page.mouse.move(...port);await page.mouse.down();
      await page.mouse.move(port[0]+1,port[1]+1);await page.mouse.up();
      await page.waitForFunction(n=>sdk.readState().objects===n+2,total,{timeout:3000});
      const c=await read();assert.equal(c.start.objectId,source.primary);assert.equal(c.end.port,4);
      assert.notEqual(c.end.objectId,blocker.primary);
      assert.equal(await count(),before+1);
      const end=world(c.worldTransform,c.route.at(-1));
      assert(end[1]-30>=blocker.bounds[3]+7 || end[1]+30<=blocker.bounds[1]-7);
      landings.push(end);
    }
    assert.equal(landings[0][0],landings[1][0]);assert(landings[0][1]>375 && landings[1][1]<375);
  });
  await open();
  await test('an explicit plus belongs to its selected shape even over another port',async()=>{
    const source=await draw('rectangle',[335,305],[415,375]);
    const port=[source.bounds[2]+28,340];
    await draw('rectangle',[port[0],314],[port[0]+55,366]);
    await page.mouse.click(375,340);await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
    await page.mouse.click(...port);
    await page.waitForFunction(()=>sdk.readConnector().available,null,{timeout:3000});
    const c=await read();assert.deepEqual(c.start,{objectId:source.primary,port:2});
    assert.equal(c.end.port,4);
  });
  await open();
  await test('thick arrow shaft stays inside the triangular tip in canvas and PNG',async()=>{
    const source=await draw('rectangle',[245,370],[335,430]);
    const target=await draw('rectangle',[565,490],[655,560]);
    await page.mouse.click(1060,610);
    await page.evaluate(()=>sdk.appearance.setDefaults({strokeEnabled:true,stroke:[.14,.27,.40,1],strokeWidth:6}));
    const id=await page.evaluate(async({a,b})=>(await sdk.createConnector({
      start:{objectId:a,port:2},end:{objectId:b,port:1},endArrow:true})).objectId,{a:source.primary,b:target.primary});
    const wire=await read(id),tip=world(wire.worldTransform,wire.displayRoute.at(-1));
    const prev=world(wire.worldTransform,wire.displayRoute.at(-2));assert.equal(tip[0],prev[0]);assert(tip[1]>prev[1]);
    const clip={x:tip[0]-16,y:tip[1]-20,width:32,height:26};
    const screen=await page.screenshot({clip});
    const png=await page.evaluate(async c=>[...(await sdk.exportImage({format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:6})).bytes],clip);
    await fs.writeFile(path.join(out,'arrow-tip.png'),new Uint8Array(png));
    const pixels=async bytes=>page.evaluate(async({bytes,w})=>{
      const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
      const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');ctx.drawImage(image,0,0);
      const s=image.width/w;
      const sample=(x,y)=>ctx.getImageData(Math.floor(x*s),Math.floor(y*s),1,1).data[0];
      const profile=[sample(18.25,19),sample(13.75,19),sample(16,15),sample(16,7)];image.close();return profile;
    },{bytes:[...bytes],w:clip.width});
    const expectTip=values=>{
      assert(values[0]>235 && values[1]>235,'both flanks of the tip taper to a point: '+values);
      assert(values[2]<170 && values[3]<170,'head joins the shaft: '+values);
    };
    expectTip(await pixels(screen));expectTip(await pixels(png));
    await page.mouse.click(tip[0],tip[1]-18);
    assert.equal((await read()).objectId,id);
    for(const style of [{cornerRadius:0},{enabled:true,roughness:1.5},{cornerRadius:8}]){
      await page.evaluate(style=>sdk.appearance.patch(style),style);
      const exported=await page.evaluate(async c=>[...(await sdk.exportImage({format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:6})).bytes],clip);
      expectTip(await pixels(exported));
    }
  });
  await open();
  await test('perpendicular ports leave room for the bend and the incoming arrow',async()=>{
    await page.locator('#shape-tool').click();
    const source=await draw('rectangle',[250,350],[400,500]);
    const target=await draw('rectangle',[632,400],[782,550]);
    await page.mouse.click(325,425);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
    await drag([325,source.bounds[1]-28],[632,475]);
    await page.waitForFunction(()=>sdk.readConnector().available);
    const wire=await page.evaluate(()=>sdk.readSelection()),c=await read(wire.primary);
    assert.deepEqual(c.start,{objectId:source.primary,port:1});
    assert.deepEqual(c.end,{objectId:target.primary,port:4});
    const radius=await page.evaluate(()=>sdk.appearance.read().values.cornerRadius);
    await page.mouse.click(930,610);
    const clip={x:220,y:292,width:594,height:283};
    const screen=await page.screenshot({clip,path:path.join(out,'perpendicular-ports.png')});
    const points=c.displayRoute.map(p=>world(c.worldTransform,p));
    await fs.writeFile(path.join(out,'perpendicular-ports.json'),JSON.stringify({points,radius}));
    assert.equal(points.length,5);
    const [start,departure,turn,arrival,tip]=points;
    assert(start[1]-departure[1]>=16,'visible shaft before the first turn');
    assert(turn[0]>470 && turn[0]<562,'vertical segment uses the space between the two shapes');
    assert(tip[0]-arrival[0]>=24,'horizontal shaft remains visible behind the arrowhead');
    assert(radius>0,'new elbow connector has rounded bends');
    const exportBend=async()=>new Uint8Array(await page.evaluate(async([x,y])=>[
      ...(await sdk.exportImage({format:'png',bounds:[x-12,y-12,x+12,y+12],padding:0,scale:3})).bytes],turn));
    const arrowInk=()=>page.evaluate(async([x,y])=>{
      const {bytes}=await sdk.exportImage({format:'png',bounds:[x-20,y-12,x+4,y+12],padding:0,scale:4});
      const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
      const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');
      ctx.drawImage(image,0,0);
      const data=ctx.getImageData(0,0,image.width,image.height).data;
      image.close();
      let coverage=0;
      for(let i=0;i<data.length;i+=4) coverage+=(255-data[i])*data[i+3]/255;
      return coverage;
    },tip);
    const sampleBend=(bytes,origin,scale)=>page.evaluate(async({bytes,origin,scale,turn})=>{
      const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
      const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');
      ctx.drawImage(image,0,0);
      const values=[[-.5,.25],[-2.5,2.25]].map(([dx,dy])=>
        ctx.getImageData(Math.floor((turn[0]+dx-origin[0])*scale),
          Math.floor((turn[1]+dy-origin[1])*scale),1,1).data[0]);
      image.close();return values;
    },{bytes:[...bytes],origin,scale,turn});
    const rounded=await exportBend(),roundedArrow=await arrowInk();
    const exportOrigin=[turn[0]-12,turn[1]-12];
    for(const [apex,arc] of [await sampleBend(screen,[clip.x,clip.y],dpr),
                             await sampleBend(rounded,exportOrigin,3)]){
      assert(apex>240 && arc<210,'the painted arc clears the square apex, in the canvas and in PNG');
    }
    await page.mouse.click(turn[0]+45,(turn[1]+arrival[1])/2);
    assert.equal(await page.evaluate(()=>sdk.readSelection().count),0);
    await page.mouse.click(turn[0]-2,turn[1]+2);
    assert.equal((await read()).objectId,wire.primary,'pick the visible arc');
    await page.evaluate(()=>sdk.appearance.patch({cornerRadius:0}));
    assert.deepEqual((await read()).displayRoute,c.displayRoute);
    const square=await exportBend();
    assert.notDeepEqual(rounded,square,'radius changes the painted elbow in the export');
    const squareArrow=await arrowInk();
    assert(squareArrow>0 && Math.abs(roundedArrow/squareArrow-1)<.08,
      'rounding the elbows preserves the arrow silhouette: '+roundedArrow+'/'+squareArrow);
    await page.evaluate(()=>sdk.undo());
    assert.equal(await page.evaluate(()=>sdk.appearance.read().values.cornerRadius),radius);
  });
  await open();
  await test('elbow radius defaults respect straight tools and an explicit zero',async()=>{
    const defaults=()=>page.evaluate(()=>sdk.appearance.readDefaults().values.cornerRadius);
    await page.evaluate(()=>sdk.selectShape('rectangle'));assert.equal(await defaults(),0);
    await page.evaluate(()=>sdk.selectShape('line'));assert.equal(await defaults(),0);
    await page.evaluate(()=>sdk.selectShape('arrow'));assert.equal(await defaults(),8);
    const n=await count();
    await page.evaluate(()=>sdk.appearance.setDefaults({cornerRadius:0}));
    assert.equal(await defaults(),0);assert.equal(await count(),n);
    await draw('arrow',[325,347],[535,498]);
    assert.equal(await page.evaluate(()=>sdk.appearance.read().values.cornerRadius),0);
    await page.evaluate(()=>sdk.appearance.setDefaults({cornerRadius:17}));
    await draw('arrow',[310,301],[690,416]);
    assert.equal(await page.evaluate(()=>sdk.appearance.read().values.cornerRadius),17);
  });
  await open();
  await test('straight tool and distant alignment',async()=>{
    await draw('rectangle',[760,300],[860,380]);
    await page.mouse.click(1090,640);
    const s=await draw('line',[220,480],[360,340]),c=await read(s.primary);
    assert.equal(c.routing,1);assert.equal(c.endArrow,false);assert.deepEqual(c.end,{point:[360,340]});
    assert.deepEqual(c.route,[[220,480],[360,340]]);
  });
  await open();
  const section=async(boundary,title)=>page.evaluate(async({boundary,title})=>{await sdk.createSection({boundary,title});return sdk.readSection().objectId;},{boundary,title});
  const left=await section([110,235,430,385],'Left'),right=await section([620,235,430,385],'Right');
  const a=await draw('rectangle',[185,300],[275,370]),b=await draw('rectangle',[375,460],[465,530]);
  const c=await draw('ellipse',[715,350],[805,420]);
  const side=(s,right)=>[right?s.bounds[2]:s.bounds[0],(s.bounds[1]+s.bounds[3])/2];
  const from=side(a,true),to=side(b,false),third=side(c,false);
  const selection=await draw('arrow',from,to),id=selection.primary;
  const first=await read(id);
  const pose=first.worldTransform;
  const grip=wire=>world(pose,wire.displayRoute.at(-1));
  await test('same-section parents, orthogonal default, two painted gaps',async()=>{
    assert((await ids(left)).includes(a.primary)&&(await ids(left)).includes(b.primary)&&(await ids(left)).includes(id));
    assert((await ids(right)).includes(c.primary));
    assert.deepEqual(first.start,{objectId:a.primary,port:2});assert.deepEqual(first.end,{objectId:b.primary,port:4});
    assert.equal(first.routing,2);assert.equal(first.endArrow,true);assert.equal(first.startArrow,false);
    assert(first.route.length>=3);
    assert(first.route.slice(1).every(([x,y],i)=>x===first.route[i][0]||y===first.route[i][1]));
    const gaps=[Math.hypot(first.displayRoute[0][0]-first.route[0][0],first.displayRoute[0][1]-first.route[0][1]),
      Math.hypot(first.displayRoute.at(-1)[0]-first.route.at(-1)[0],first.displayRoute.at(-1)[1]-first.route.at(-1)[1])];
    assert(gaps.every(g=>g>2&&g<=8.01));
  });
  await test('A to B reconnects to C once, with cancel and no-op',async()=>{
    const n=await count();await drag(grip(first),third,false);
    assert.equal((await read(id)).endpointEdit.state,'editing');assert.deepEqual((await read(id)).end,first.end);assert.equal(await count(),n);
    await page.screenshot({path:path.join(out,'reconnect-preview.png')});
    await page.mouse.up();await page.waitForTimeout(180);
    const committed=await read(id);assert.deepEqual(committed.end,{objectId:c.primary,port:4});
    assert.deepEqual(committed.start,first.start);assert.equal(committed.objectId,id);assert.equal(await count(),n+1);
    assert((await ids(left)).includes(id));assert.equal(committed.endpointEdit.state,'applied');
    await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(id)).end,first.end);
    await page.evaluate(()=>sdk.redo());assert.deepEqual((await read(id)).end,committed.end);
    const before=await count();await page.mouse.click(...grip(committed));await page.waitForTimeout(100);
    assert.equal(await count(),before);assert.equal((await read(id)).endpointEdit.state,'unchanged');
    await drag(grip(committed),[555,555],false);await page.keyboard.press('Escape');await page.mouse.up();await page.waitForTimeout(120);
    assert.equal(await count(),before);assert.deepEqual((await read(id)).end,committed.end);assert.equal((await read(id)).endpointEdit.state,'cancelled');
    await drag(grip(committed),[555,555]);assert.deepEqual((await read(id)).end,{point:[555,555]});
    await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(id)).end,committed.end);
  });
  await test('automatic perimeter, explicit endpoint command and stale generation',async()=>{
    const before=await read(id);
    const angle=.68*Math.PI,bb=c.bounds;
    const edge=[(bb[0]+bb[2])/2+Math.cos(angle)*(bb[2]-bb[0])/2,(bb[1]+bb[3])/2+Math.sin(angle)*(bb[3]-bb[1])/2];
    await drag(grip(before),edge);
    const auto=await read(id);assert.deepEqual(auto.end,{objectId:c.primary});
    const n=await count();
    const result=await page.evaluate(async({id,auto})=>{
      const unchanged=await sdk.setConnectorEndpoint({objectId:id,side:'end',endpoint:auto.end,generation:auto.generation});
      const changed=await sdk.setConnectorEndpoint({objectId:id,side:'end',endpoint:{objectId:auto.end.objectId,port:4},generation:auto.generation});
      let stale;try{await sdk.setConnectorEndpoint({objectId:id,side:'start',endpoint:{point:[17,18]},generation:auto.generation});}catch(e){stale=e.code;}
      return {unchanged:unchanged.changed,changed:changed.changed,stale};
    },{id,auto});
    assert.deepEqual(result,{unchanged:false,changed:true,stale:'STALE'});assert.equal(await count(),n+1);
  });
  await test('move target into other section without losing the connection',async()=>{
    const center=[(c.bounds[0]+c.bounds[2])/2,(c.bounds[1]+c.bounds[3])/2];
    await page.mouse.click(...center);await drag(center,[451,340]);
    assert((await ids(left)).includes(c.primary));assert(!(await ids(right)).includes(c.primary));
    const wire=await read(id);assert.deepEqual(wire.end,{objectId:c.primary,port:4});
    assert((await ids(left)).includes(id));assert(grip(wire)[0]<500);
  });
  await test('transformed clipboard preserves visible positions',async()=>{
    const wire=await read(id),a=world(pose,wire.displayRoute[0]),b=world(pose,wire.displayRoute[1]);
    await page.mouse.click((a[0]+b[0])/2,(a[1]+b[1])/2);await page.waitForTimeout(160);
    assert.equal(await page.evaluate(()=>sdk.readSelection().primary),id);
    await page.evaluate(()=>sdk.transformSelection([1,0,0,1,43,23]));await page.waitForTimeout(150);
    const original=await read(id),t=original.worldTransform;
    await page.evaluate(()=>sdk.duplicateSelection({offset:[20,20]}));await page.waitForTimeout(120);
    const copy=await page.evaluate(()=>({s:sdk.readSelection(),wire:sdk.readConnector()}));
    assert.notEqual(copy.s.primary,id);assert(copy.wire.start.point && copy.wire.end.point);
    for(const n of [0,-1])assert(near(world(copy.wire.worldTransform,copy.wire.displayRoute.at(n)),world(t,original.displayRoute.at(n)).map(v=>v+20)));
    await page.evaluate(()=>sdk.undo());assert.equal((await read(copy.s.primary)).available,false);
    await page.evaluate(()=>sdk.redo());assert.equal((await read(copy.s.primary)).available,true);
  });
  await test('external snapshot and remote operations',async()=>{
    const check=await page.evaluate(async({id,left})=>{
      const {createRuntime}=await import('./axiom.mjs');
      const mount=()=>{const e=document.createElement('div');e.style.cssText='position:absolute;left:-2000px;width:800px;height:600px';document.body.append(e);return e;};
      const checkpoint={generation:sdk.readState().generation,documentRevision:'1',recoveryFrontier:[8,1,16,1,24,1]};
      const snap=await sdk.captureSnapshot(checkpoint),doc=sdk.readState().documentId;
      const inspect=r=>{const c=r.readConnector(id);return {start:c.start,end:c.end,routing:c.routing,endArrow:c.endArrow,members:r.readSection(left).children};};
      const original=inspect(sdk),m1=mount(),m2=mount();
      const restored=await createRuntime({mount:m1,documentId:doc,permission:'localSingleUser',restore:{bytes:snap.bytes,metadata:{...checkpoint,checkpointDigest:snap.checkpointDigest,resourceBinding:'1'}}});
      const replica=await createRuntime({mount:m2,documentId:doc,permission:'localSingleUser'});
      try{for(const bytes of window.packets)await replica.applyRemoteOperation(bytes);return [original,inspect(restored),inspect(replica)];}
      finally{await restored.close();await replica.close();m1.remove();m2.remove();}
    },{id,left});assert.deepEqual(check[1],check[0]);assert.deepEqual(check[2],check[0]);
  });
  await open();
  await test('visible endpoint gaps and arrow direction in the canvas and exported PNG',async()=>{
    const left=await draw('ellipse',[250,320],[350,420]),right=await draw('ellipse',[640,320],[740,420]);
    await page.mouse.click(1050,640);
    const id=await page.evaluate(async({a,b})=>(await sdk.createConnector({start:{objectId:a,port:2},end:{objectId:b,port:4}})).objectId,{a:left.primary,b:right.primary});
    const ready=await read(id);assert.equal(ready.endArrow,true);assert.equal(ready.startArrow,false);
    const [[ax,ay],[bx,by]]=ready.route;assert.equal(ay,by);
    const [[sx],[ex]]=ready.displayRoute;
    const clip={x:Math.floor(ax)-4,y:Math.floor(ay)-18,width:Math.ceil(bx-ax)+8,height:36};
    const samples=[[ax+4,ay],[bx-4,by],[sx+12,ay],[ex-8,by+2],[sx+8,ay+2]];
    const pixels=async png=>page.evaluate(async({bytes,clip,samples})=>{
      const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
      const g=new OffscreenCanvas(image.width,image.height).getContext('2d');g.drawImage(image,0,0);
      const s=image.width/clip.width;
      const colors=samples.map(([x,y])=>[...g.getImageData(Math.floor((x-clip.x)*s),Math.floor((y-clip.y)*s),1,1).data]);
      image.close();return colors;
    },{bytes:[...png],clip,samples});
    const png=await page.screenshot({clip,path:path.join(out,'connection-gaps.png')});
    const exported=await page.evaluate(async clip=>[...(await sdk.exportImage({format:'png',bounds:[clip.x,clip.y,clip.x+clip.width,clip.y+clip.height],padding:0,scale:1})).bytes],clip);
    const screen=await pixels(png),encoded=await pixels(exported);
    const light=c=>c.slice(0,3).every(v=>v>245),dark=c=>Math.min(...c.slice(0,3))<165;
    for(const p of [screen,encoded]){
      assert(light(p[0])&&light(p[1]),'both ends have visible whitespace');
      assert(dark(p[2])&&dark(p[3])&&light(p[4]),'one end arrow painted');
    }
    await page.mouse.click((ax+bx)/2,ay);await page.locator('#connector-arrows-10').click();
    const changed=await read(id);assert.equal(changed.startArrow,true);assert.equal(changed.endArrow,false);
    await page.mouse.click(1050,630);
    const reversed=await pixels(await page.screenshot({clip}));
    assert(light(reversed[3])&&dark(reversed[4]),'start arrow painted');
    await page.evaluate(()=>sdk.undo());assert.equal((await read(id)).endArrow,true);
    await page.evaluate(()=>sdk.selectShape('arrow'));
    await page.locator('#connector-route-1').click();await page.locator('#connector-arrows-11').click();
    const changedDefaults=await page.evaluate(()=>sdk.readConnectorDefaults());
    assert.deepEqual(changedDefaults,{routing:1,startArrow:true,endArrow:true});
    await page.evaluate(()=>sdk.selectShape('line'));assert.deepEqual(await page.evaluate(()=>sdk.readConnectorDefaults()),{routing:1,startArrow:false,endArrow:false});
    await page.evaluate(()=>sdk.selectShape('arrow'));assert.deepEqual(await page.evaluate(()=>sdk.readConnectorDefaults()),changedDefaults);
  });
  await open();
  await test('quick creation shows its sibling while dragging and commits the pair once',async()=>{
    const s=await draw('rectangle',[200,315],[290,385]),port=[s.bounds[2]+28,(s.bounds[1]+s.bounds[3])/2];
    const clip={x:460,y:367,width:110,height:83};
    await page.mouse.move(1000,650);
    const before=await page.screenshot({clip}),n=await count();
    await drag(port,[469,410],false);
    const during=await page.screenshot({clip,path:path.join(out,'quick-sibling-preview.png')});
    assert.equal(await count(),n);assert.equal(await page.evaluate(()=>sdk.readState().objects),1);
    assert.notDeepEqual(before,during,'sibling is visible past the free end of the line');
    await page.mouse.up();await page.waitForTimeout(150);
    const wire=await page.evaluate(()=>sdk.readConnector());
    assert.equal(wire.start.objectId,s.primary);assert(wire.end.objectId);assert.equal(wire.routing,2);assert.equal(wire.endArrow,true);
    assert.equal(await count(),n+1);assert.equal(await page.evaluate(()=>sdk.readState().objects),3);
    await page.evaluate(()=>sdk.undo());assert.equal(await page.evaluate(()=>sdk.readState().objects),1);
  });
  await open();
  await test('locked targets, invalid endpoint and concurrent document change',async()=>{
    const left=await section([140,245,880,380],'Board');
    const a=await draw('rectangle',[215,300],[315,390]), b=await draw('rectangle',[690,370],[790,460]);
    await page.evaluate(()=>sdk.lockSelection(true));
    const wire=await draw('arrow',side(a,true),[530,335]),id=wire.primary;
    const committed=await read(id);
    const reasons=await page.evaluate(async({id,b,generation})=>{
      const errors=[];
      for(const endpoint of [{objectId:b,port:1},{objectId:b,port:5},{point:[2,2],objectId:b},{point:[4]}]){
        try{await sdk.setConnectorEndpoint({objectId:id,side:'end',endpoint,generation});errors.push('accepted');}catch(e){errors.push(e.code);}
      }
      return errors;
    },{id,b:b.primary,generation:committed.generation});
    assert(reasons.every(x=>x!=='accepted'));assert.equal(reasons[0],'LOCKED');
    await drag(world(wire.transform,committed.displayRoute.at(-1)),side(b,false));
    const free=await read(id);assert(free.end.point);
    await page.evaluate(async({left})=>{
      const {createRuntime}=await import('./axiom.mjs');const mount=document.createElement('div');
      mount.style.cssText='position:absolute;left:-2000px;width:400px;height:300px';document.body.append(mount);
      const sent=[];
      const peer=await createRuntime({mount,documentId:sdk.readState().documentId,permission:'localSingleUser',onOperation:b=>sent.push(new Uint8Array(b))});
      try{for(const bytes of packets)await peer.applyRemoteOperation(bytes);await peer.renameSection(left,'Remote');window.remoteRename=sent.at(-1);}
      finally{await peer.close();mount.remove();}
    },{left});
    const n=await count();await drag(world(wire.transform,free.displayRoute.at(-1)),[542,487],false);
    await page.evaluate(()=>sdk.applyRemoteOperation(remoteRename));await page.waitForTimeout(160);await page.mouse.up();await page.waitForTimeout(100);
    const after=await read(id);assert.deepEqual(after.end,free.end);assert.equal(await count(),n);assert.equal(after.endpointEdit.state,'stale');
  });
  await open();
  await test('moving and duplicating the entire section carries both visible tips and remaps both bindings',async()=>{
    const section=await draw('section',[185,260],[930,605]);
    const source=await draw('rectangle',[310,335],[405,415]);
    const button=[source.bounds[2]+28,(source.bounds[1]+source.bounds[3])/2];
    await page.mouse.click(...button);await page.waitForFunction(()=>sdk.readConnector().available);
    const wire=await page.evaluate(()=>sdk.readConnector()),id=wire.objectId;
    const tips=c=>[c.displayRoute[0],c.displayRoute.at(-1)].map(p=>world(c.worldTransform,p));
    const moved=(before,after,dx,dy)=>tips(before).every(([x,y],i)=>near(tips(after)[i],[x+dx,y+dy]));
    await page.evaluate(id=>sdk.selectSection(id),section.primary);
    const pose=await page.evaluate(()=>sdk.readSelection().transform);pose[4]+=45;pose[5]+=28;
    await page.evaluate(t=>sdk.transformSelection(t),pose);await page.waitForTimeout(180);
    const changed=await read(id);assert(moved(wire,changed,45,28));assert.deepEqual(changed.start,wire.start);assert.deepEqual(changed.end,wire.end);
    const family=await ids(section.primary);assert(family.includes(id)&&family.includes(wire.start.objectId)&&family.includes(wire.end.objectId));
    await page.evaluate(()=>sdk.duplicateSelection({offset:[26,24]}));await page.waitForTimeout(120);
    const copy=await page.evaluate(()=>sdk.readSection().objectId),children=await ids(copy);
    const wires=await page.evaluate(children=>children.map(id=>sdk.readConnector(id)).filter(c=>c.available),children);
    assert.equal(wires.length,1);const twin=wires[0];assert.notEqual(twin.objectId,id);
    assert(children.includes(twin.start.objectId)&&children.includes(twin.end.objectId));
    assert(!family.includes(twin.start.objectId)&&!family.includes(twin.end.objectId));assert(moved(changed,twin,26,24));
  });
  await open();
  await test('fixed port and automatic outline show distinct hover feedback',async()=>{
    await draw('ellipse',[470,330],[590,430]);await page.mouse.click(1040,630);
    await page.evaluate(()=>sdk.selectShape('arrow'));
    const n=await count(),clip={x:451,y:312,width:160,height:138};
    const purple=async name=>{
      await page.waitForTimeout(160);
      const bytes=await page.screenshot({clip,path:path.join(out,name+'.png')});
      return page.evaluate(async data=>{
        const image=await createImageBitmap(new Blob([new Uint8Array(data)],{type:'image/png'}));
        const g=new OffscreenCanvas(image.width,image.height).getContext('2d');g.drawImage(image,0,0);
        const rgba=g.getImageData(0,0,image.width,image.height).data;image.close();let count=0;
        for(let i=0;i<rgba.length;i+=4)if(rgba[i+2]>190 && rgba[i+2]-Math.max(rgba[i],rgba[i+1])>60)++count;
        return count;
      },[...bytes]);
    };
    await page.mouse.move(900,600);const idle=await purple('target-idle');
    await page.mouse.move(470,380);const fixed=await purple('target-port');
    await page.mouse.move(572,415);const auto=await purple('target-perimeter');
    assert(fixed>idle+10,'fixed port ring');assert(auto>fixed+50,'automatic target outline');
    assert.equal(await count(),n);
  });
  for(const scale of [.5,1,2]){
    await open();
    await test('screen-space targets at '+scale+' zoom',async()=>{
      const shape=await draw('rectangle',[475,335],[575,415]);
      await page.evaluate(z=>sdk.zoomAt(520,376,z),scale);
      const camera=await page.evaluate(()=>sdk.readState().camera);
      const view=([x,y])=>[x*camera.scale+camera.x,y*camera.scale+camera.y];
      const p=side(shape,false),left=view(p),y=left[1];
      const first=await draw('arrow',[left[0]-110,y-44],[left[0]-4,y]);
      assert.deepEqual((await read(first.primary)).end,{objectId:shape.primary,port:4});
      const far=await draw('arrow',[left[0]-180,y+80],[left[0]-75,y]);
      assert((await read(far.primary)).end.point);
    });
  }
} finally {
  const result={browser:browser.version(),dpr,cases,errors};
  await fs.writeFile(path.join(out,'connector-editing.json'),JSON.stringify(result,null,2));
  console.log(JSON.stringify(result));
  await browser.close();await new Promise(r=>server.close(r));
}
assert.deepEqual(errors,[]);
