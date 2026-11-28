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
async function frameBoardAt(width,height,scale,focus,screen) {
  await page.setViewportSize({width,height});
  await page.evaluate(async({scale,focus,screen})=>{
    await new Promise(requestAnimationFrame);
    await sdk.zoomAt(...screen,scale);
    const cam=sdk.readState().camera;
    await sdk.panBy(focus[0]*cam.scale+cam.x-screen[0],focus[1]*cam.scale+cam.y-screen[1]);
  },{scale,focus,screen});
  await page.waitForFunction(({focus,screen,scale})=>{
    const cam=sdk.readState().camera;
    return Math.abs(cam.scale-scale)<.001 && Math.abs(focus[0]*cam.scale+cam.x-screen[0])<.05 &&
      Math.abs(focus[1]*cam.scale+cam.y-screen[1])<.05;
  },{focus,screen,scale},{timeout:3000});
}
const countWarningPixels=png=>page.evaluate(async bytes=>{
  const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
  const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');ctx.drawImage(image,0,0);
  const data=ctx.getImageData(0,0,image.width,image.height).data;image.close();let count=0;
  for(let i=0;i<data.length;i+=4)if(data[i]>175 && data[i]-data[i+1]>65 && data[i+1]>data[i+2]+20)++count;
  return count;
},[...png]);
const portInk=(png,areas)=>page.evaluate(async({bytes,areas,dpr})=>{
  const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
  const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');ctx.drawImage(image,0,0);
  const counts=areas.map(([x,y,w,h])=>{
    const pixels=ctx.getImageData(x*dpr,y*dpr,w*dpr,h*dpr).data;let n=0;
    for(let i=0;i<pixels.length;i+=4)if(Math.abs(pixels[i]-pixels[i+1])<13 && pixels[i+2]-pixels[i]>38)++n;
    return n;
  });image.close();return counts;
},{bytes:[...png],areas,dpr});
const redSamples=(png,clip,points)=>page.evaluate(async({bytes,clip,points})=>{
  const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
  const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');ctx.drawImage(image,0,0);
  const scale=image.width/clip.width;
  const samples=points.map(([x,y])=>ctx.getImageData(Math.floor((x-clip.x)*scale),
    Math.floor((y-clip.y)*scale),1,1).data[0]);
  image.close();return samples;
},{bytes:[...png],clip,points});
const horizontalArrowWidth=async(img,clip,tip)=>page.evaluate(async({bytes,tip,clip})=>{
  const image=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
  const ctx=new OffscreenCanvas(image.width,image.height).getContext('2d');ctx.drawImage(image,0,0);
  const scale=image.width/clip.width,x0=Math.round((tip[0]-clip.x)*scale),y0=Math.round((tip[1]-clip.y)*scale);
  const ink=(x,y)=>ctx.getImageData(x,y,1,1).data[0]<190;
  let widest=0;
  for(let x=x0+Math.ceil(6*scale);x<=x0+18*scale;++x){
    if(!ink(x,y0)||ink(x,y0-Math.round(9*scale))||ink(x,y0+Math.round(9*scale)))continue;
    let t=y0,b=y0;while(ink(x,t-1))--t;while(ink(x,b+1))++b;
    widest=Math.max(widest,(b+1-t)/scale);
  }
  image.close();return widest;
},{bytes:[...img],tip,clip});
async function draw(kind,a,b){
  const n=await page.evaluate(()=>sdk.readState().objects);
  await page.evaluate(kind=>sdk.selectShape(kind),kind);await drag(a,b);
  await page.waitForFunction(n=>sdk.readState().objects>n,n);
  return page.evaluate(()=>sdk.readSelection());
}
async function test(name,fn){try{await fn();cases.push({name,passed:true});}catch(e){cases.push({name,passed:false,error:String(e.stack)});throw e;}}

try {
  await open();
  await test('the bare left port has a rounded exit when the opposite top port is constrained',async()=>{
    await page.locator('#shape-tool').click();
    const hub=await draw('rectangle',[330,420],[450,500]), center=await draw('rectangle',[650,420],[770,500]);
    const upper=await draw('rectangle',[650,300],[770,380]), lower=await draw('rectangle',[650,596],[770,676]);
    await page.evaluate(async({hub,center,upper,lower})=>{
      for(const [from,id,to] of [[2,upper,4],[2,center,4],[2,lower,4],[3,center,2]])
        await sdk.createConnector({start:{objectId:hub,port:from},end:{objectId:id,port:to},endArrow:true});
    },{hub:hub.primary,center:center.primary,upper:upper.primary,lower:lower.primary});
    const clip={x:250,y:270,width:580,height:420};
    await page.mouse.click(390,460);await page.waitForFunction(id=>sdk.readSelection().primary===id,hub.primary);
    const n=await count();await drag([hub.bounds[0]-28,460],[710,420],false);
    const held=await page.screenshot({clip,path:path.join(out,'bare-port-drag.png')});
    assert.equal(await count(),n);await page.mouse.up();await page.waitForFunction(n=>packets.length===n+1,n);
    const wire=await read();
    assert.deepEqual(wire.start,{objectId:hub.primary,port:4});assert.deepEqual(wire.end,{objectId:center.primary,port:1});
    assert(!wire.startArrow && wire.endArrow);assert.equal(wire.routeStatus,1);
    await page.screenshot({clip,path:path.join(out,'bare-port-selected.png')});
    await page.mouse.click(990,620);
    const screen=await page.screenshot({clip,path:path.join(out,'bare-port.png')});
    const png=await page.evaluate(async c=>[...(await sdk.exportImage({format:'png',
      bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:4})).bytes],clip);
    await fs.writeFile(path.join(out,'bare-port-export.png'),Buffer.from(png));
    const points=wire.route.map(p=>world(wire.worldTransform,p));
    const samples=[[298,447],[317,447],[298-.5,460+.5],[299.25,458.75]];
    const ink={held:await redSamples(held,clip,samples),screen:await redSamples(screen,clip,samples),
      png:await redSamples(png,clip,samples)};
    await fs.writeFile(path.join(out,'bare-port.json'),JSON.stringify({points,ink,
      display:wire.displayRoute.map(p=>world(wire.worldTransform,p)),warning:await countWarningPixels(held)},null,2));
    assert.equal(points[0][0]-points[1][0],32,'run before the first bend');
    assert.equal(points[0][1],points[1][1]);
    for(const values of Object.values(ink))assert(values[0]<190 && values[1]>240,'visible detour: '+values);
    assert(ink.png[2]>240 && ink.png[3]<190,'rounded outer corner and curved stroke in PNG: '+ink.png);
    assert.equal(await countWarningPixels(held),0);
    await page.evaluate(()=>sdk.undo());assert.equal((await read(wire.objectId)).available,false);
    await page.evaluate(()=>sdk.redo());
    const restored=await read(wire.objectId);assert.deepEqual(restored.route,wire.route);
    assert.deepEqual(restored.displayRoute,wire.displayRoute);
  });
  await open();
  await test('the middle horizontal wire keeps its own full arrow at a shared right port',async()=>{
    await page.locator('#shape-tool').click();
    const hub=await draw('rectangle',[280,320],[400,400]),upper=await draw('rectangle',[662.5,370.5],[782.5,450.5]);
    const lower=await draw('rectangle',[518.5,497],[638.5,577]);
    const outer=await page.evaluate(async({hub,upper,lower})=>{
      await sdk.createConnector({start:{objectId:hub,port:4},end:{objectId:lower,port:3},endArrow:true});
      return (await sdk.createConnector({start:{objectId:hub,port:1},end:{objectId:upper,port:2},endArrow:true})).objectId;
    },{hub:hub.primary,upper:upper.primary,lower:lower.primary});
    const clip={x:211,y:270,width:640,height:365},anchor=[782.5,410.5];
    const tip=c=>world(c.worldTransform,c.displayRoute.at(-1));
    const exportBoard=async name=>{
      const bytes=await page.evaluate(async c=>[...(await sdk.exportImage({format:'png',
        bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
      await fs.writeFile(path.join(out,name+'-export.png'),Buffer.from(bytes));return bytes;
    };
    const other=await read(outer);
    const reference=await horizontalArrowWidth(await exportBoard('shared-outer'),clip,anchor);
    await page.mouse.click(340,360);await page.waitForFunction(id=>sdk.readSelection().primary===id,hub.primary);
    const n=await count();await drag([hub.bounds[2]+28,360],anchor,false);
    const held=await page.screenshot({clip,path:path.join(out,'shared-right-drag.png')});
    assert.equal(await count(),n);await page.mouse.up();await page.waitForFunction(n=>packets.length===n+1,n);
    const c=await read();assert.deepEqual(c.start,{objectId:hub.primary,port:2});assert.deepEqual(c.end,other.end);
    await page.mouse.click(972,645);
    await page.screenshot({clip,path:path.join(out,'shared-right.png')});
    // The outer wire terminates at the same port. Remove just its decoration
    // for one frame so its larger triangle cannot mask a smaller middle head.
    await page.evaluate(outer=>sdk.setConnectorDecoration({objectId:outer,startArrow:false,endArrow:false}),outer);
    const alone=await read(c.objectId);
    const screen=await page.screenshot({clip,path:path.join(out,'shared-right-isolated.png')});
    const widths={png:await horizontalArrowWidth(await exportBoard('shared-right-isolated'),clip,anchor),
      screen:await horizontalArrowWidth(screen,clip,anchor),reference};
    const points=c.route.map(p=>world(c.worldTransform,p));
    await fs.writeFile(path.join(out,'shared-right.json'),JSON.stringify({points,widths,tip:tip(c),otherTip:tip(other)},null,2));
    assert.equal(c.routeStatus,1);assert.equal(await countWarningPixels(held),0);
    assert.deepEqual(alone.route,c.route);assert(near(tip(c),tip(other)),'arrows align at the same visible tip');
    assert(widths.screen>=11 && widths.png>=11.5 && Math.abs(widths.png-reference)<.7,'isolated widths: '+JSON.stringify(widths));
    assert.equal(points.length,4,'the long horizontal span has two bends');
    assert.equal(points[1][1],points[0][1]);assert(points[1][0]>anchor[0]+20);
    await page.evaluate(()=>sdk.undo());assert((await read(outer)).endArrow);
    assert.deepEqual((await read(c.objectId)).route,c.route);
  });
  await open();
  await test('a clear right-side arrow remains full size below a blocking shape',async()=>{
    await page.locator('#shape-tool').click();
    const hub=await draw('rectangle',[280,320],[400,400]);
    const upper=await draw('rectangle',[662.5,370.5],[782.5,450.5]);
    const lower=await draw('rectangle',[518.5,497],[638.5,577]);
    const reference=await page.evaluate(async({hub,upper,lower})=>{
      const pairs=[[1,upper,2],[3,upper,1],[2,upper,4],[4,lower,3],[2,lower,4]],ids=[];
      for(const [from,id,to] of pairs){
        const c=await sdk.createConnector({start:{objectId:hub,port:from},end:{objectId:id,port:to},endArrow:true});
        ids.push(c.objectId);
      }
      return ids[0];
    },{hub:hub.primary,upper:upper.primary,lower:lower.primary});
    const clip={x:211,y:270,width:640,height:365},port=[638.5,537];
    const n=await count();
    await page.mouse.click(340,360);await page.waitForFunction(id=>sdk.readSelection().primary===id,hub.primary);
    await drag([hub.bounds[2]+28,360],port,false);
    const held=await page.screenshot({clip,path:path.join(out,'clear-landing-drag.png')});
    assert.equal(await count(),n);await page.mouse.up();
    await page.waitForFunction(n=>packets.length===n+1,n);
    const wire=await read();
    assert.deepEqual(wire.start,{objectId:hub.primary,port:2});assert.deepEqual(wire.end,{objectId:lower.primary,port:2});
    await page.mouse.click(972,645);
    const inspect=async label=>{
      const c=await read(wire.objectId),other=await read(reference);
      const route=c.route.map(p=>world(c.worldTransform,p)),refTip=world(other.worldTransform,other.route.at(-1));
      const tip=route.at(-1),turn=route.at(-2);
      const screen=await page.screenshot({clip,path:path.join(out,label+'.png')});
      const png=await page.evaluate(async c=>[...(await sdk.exportImage({format:'png',
        bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
      await fs.writeFile(path.join(out,label+'-export.png'),Buffer.from(png));
      const widths={screen:await horizontalArrowWidth(screen,clip,tip),png:await horizontalArrowWidth(png,clip,tip),reference:await horizontalArrowWidth(png,clip,refTip)};
      const data={route,widths,status:c.routeStatus,run:turn[0]-tip[0]};
      await fs.writeFile(path.join(out,label+'.json'),JSON.stringify(data,null,2));return data;
    };
    const original=await inspect('clear-landing');
    assert.equal(await countWarningPixels(held),0);
    assert((await horizontalArrowWidth(held,clip,port))>=11,'full arrow while holding the pointer');
    await page.mouse.click(722,410);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,upper.primary);
    await page.evaluate(async()=>{const t=[...sdk.readSelection().transform];t[4]+=5;await sdk.transformSelection(t);});
    await page.mouse.click(972,645);
    const moved=await inspect('clear-landing-offset');
    for(const scenario of [original,moved]){
      assert.equal(scenario.status,1);assert(scenario.run>=20,'clear landing length: '+scenario.run);
      const {screen,png,reference}=scenario.widths;
      assert(screen>=11 && png>=11.5 && Math.abs(png-reference)<.7,'arrow widths: '+JSON.stringify(scenario.widths));
    }
    await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(wire.objectId)).route,wire.route);
    await page.evaluate(()=>sdk.redo());
    const redone=await read(wire.objectId);assert.deepEqual(redone.route.map(p=>world(redone.worldTransform,p)),moved.route);
    assert.equal(await count(),n+4);
  });
  for(const gap of [28,19]){
    await open();
    await test('both inner side ports in a '+gap+' unit slot accept a full arrow',async()=>{
      await page.locator('#shape-tool').click();
      const source=await draw('rectangle',[310,325],[430,405]);
      const nearBox=await draw('rectangle',[513,423],[633,503]);
      const farBox=await draw('rectangle',[633+gap,423],[753+gap,503]);
      const clip={x:240,y:275,width:600,height:295};
      // Keep the three existing arrows from the screenshot/recording on the
      // board while dragging into each of the two inner ports.
      await page.evaluate(async({hub,near,far,gap})=>{
        const pairs=gap===28?[[3,near,1],[4,near,4],[1,far,1]]:[[3,near,1],[1,far,2],[4,far,3]];
        for(const [from,id,to] of pairs)await sdk.createConnector({
          start:{objectId:hub,port:from},end:{objectId:id,port:to},endArrow:true});
      },{hub:source.primary,near:nearBox.primary,far:farBox.primary,gap});
      for(const [target,side,sourceSide] of [[farBox,4,gap===28?2:4],[nearBox,2,4]]){
        await page.mouse.click(370,365);
        await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
        const button=[sourceSide===2?source.bounds[2]+28:source.bounds[0]-28,365];
        const anchor=[side===4?633+gap:633,463];
        const n=await count(),total=await page.evaluate(()=>sdk.readState().objects);
        const prefix='slot-'+gap+'-port-'+side;
        await drag(button,anchor,false);
        const held=await page.screenshot({clip,path:path.join(out,prefix+'-drag.png')});
        assert.equal(await count(),n);
        await page.mouse.up();
        await page.waitForFunction(n=>sdk.readState().objects===n+1,total,{timeout:3500});
        const c=await read();
        assert.deepEqual(c.start,{objectId:source.primary,port:sourceSide});
        assert.deepEqual(c.end,{objectId:target.primary,port:side});
        await page.mouse.click(970,600);
        const screen=await page.screenshot({clip,path:path.join(out,prefix+'.png')});
        const png=await page.evaluate(async c=>[...(await sdk.exportImage({
          format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
        await fs.writeFile(path.join(out,prefix+'-export.png'),Buffer.from(png));
        const points=c.route.map(p=>world(c.worldTransform,p));
        // Scan the whole intake. A full arrow inks both sides of the shaft at
        // least four units off its centre. A vertical bend alone inks one side.
        const away=side===4?-1:1;
        const flanks=Array.from({length:14},(_,i)=>[anchor[0]+away*(i+4),anchor[1]]);
        const bands=flanks.flatMap(([x,y])=>[[x,y-4],[x,y+4]]);
        const paired=async img=>{
          const reds=await redSamples(img,clip,bands);
          return flanks.filter((_,i)=>reds[2*i]<190 && reds[2*i+1]<190).length;
        };
        const pixels={held:await paired(held),screen:await paired(screen),png:await paired(png)};
        const warning=await countWarningPixels(held);
        await fs.writeFile(path.join(out,prefix+'.json'),JSON.stringify({points,pixels,routeStatus:c.routeStatus,warning}));
        assert.equal(c.routeStatus,1);assert.equal(warning,0,'held preview shows a dashed warning');
        assert(Object.values(pixels).every(v=>v>=3),'full arrow flanks during drag, on canvas and in export: '+JSON.stringify(pixels));
        assert.equal(await count(),n+1);
        await page.evaluate(()=>sdk.undo());
        assert.equal(await page.evaluate(()=>sdk.readState().objects),total);
        await page.evaluate(()=>sdk.redo());
        assert.deepEqual((await read(c.objectId)).route,c.route);
        await page.evaluate(()=>sdk.undo());
      }
    });
  }
  await open();
  await test('a row of port children keeps its shared branch line through an upper child',async()=>{
    await page.locator('#shape-tool').click();
    const hub=await draw('rectangle',[480,305],[600,385]),port=[540,hub.bounds[3]+28];
    const clip={x:267,y:262,width:730,height:375};
    const points=c=>c.route.map(p=>world(c.worldTransform,p));
    const add=async(label)=>{
      if((await page.evaluate(()=>sdk.readSelection().primary))!==hub.primary)await page.mouse.click(540,345);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,hub.primary);
      const total=await page.evaluate(()=>sdk.readState().objects),n=await count();
      await page.mouse.move(...port);await page.waitForTimeout(100);
      const hover=label?await page.screenshot({clip,path:path.join(out,label+'-hover.png')}):null;
      assert.equal(await count(),n);await page.mouse.click(...port);
      await page.waitForFunction(n=>sdk.readState().objects===n+2,total,{timeout:3500});
      const c=await read();assert.equal(await count(),n+1);assert.equal(c.routeStatus,1);
      assert.deepEqual(c.start,{objectId:hub.primary,port:3});assert.equal(c.end.port,1);
      return {wire:c,hover};
    };
    const {wire:upper}=await add();
    const tip=points(upper).at(-1);
    await page.mouse.click(tip[0],tip[1]+39);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,upper.end.objectId);
    await page.evaluate(async()=>{const t=[...sdk.readSelection().transform];t[4]+=194;t[5]-=166;await sdk.transformSelection(t);});
    const center=await add(),right=await add('right'),left=await add(),far=await add('far-right');
    const children=[center,right,left,far].map(i=>i.wire);
    const readRow=async()=>Promise.all(children.map(async c=>points(await read(c.objectId))));
    const clearance=await readRow();
    await page.mouse.click(750,398);await page.waitForFunction(id=>sdk.readSelection().primary===id,upper.end.objectId);
    const prior=await count();
    await drag([754,395],[754,437],false);
    const held=await page.screenshot({clip,path:path.join(out,'fan-drag.png')});assert.equal(await count(),prior);
    await page.mouse.up();await page.waitForTimeout(150);assert.equal(await count(),prior+1);
    const bounds=await page.evaluate(()=>sdk.readSelection().bounds);
    await page.mouse.click(1080,641);
    const crossed=await readRow(),mid=(crossed[0][0][1]+crossed[0].at(-1)[1])/2;
    assert(bounds[1]+6<mid && mid<bounds[3]-6,'the branch really passes through the painted shape');
    const screen=await page.screenshot({clip,path:path.join(out,'fan.png')});
    const png=await page.evaluate(async c=>[...(await sdk.exportImage({
      format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
    await fs.writeFile(path.join(out,'fan-export.png'),Buffer.from(png));
    const samples=[[700,mid],[720,mid+14],[850,mid],[850,tip[1]-13],[720-1.5,mid+1.5]];
    const pixels={held:await redSamples(held,clip,samples),screen:await redSamples(screen,clip,samples),
      png:await redSamples(png,clip,samples)};
    const ghosts=[(await redSamples(right.hover,clip,[[620,mid]]))[0],(await redSamples(far.hover,clip,[[850,mid]]))[0]];
    await fs.writeFile(path.join(out,'fan.json'),JSON.stringify({mid,bounds,clearance,crossed,ghosts,pixels},null,2));
    for(const paths of [clearance,crossed]){
      assert.equal(paths[0].length,2,'the central branch is straight');
      for(const p of paths.slice(1)){
        assert.equal(p.length,4);assert(near(p[1],[p[0][0],mid]) && near(p[2],[p.at(-1)[0],mid]),
          'siblings turn on the shared midline: '+JSON.stringify(p));
      }
    }
    assert(ghosts.every(r=>r<253),'the uncommitted branches already use the common line: '+ghosts);
    for(const [view,v] of Object.entries(pixels)){
      assert(v.slice(0,3).every(r=>r<185) && v[3]>242,view+' route: '+v);
      assert(v[4]<195,view+' keeps the rounded arc inside the intervening shape: '+v[4]);
    }
    await page.evaluate(()=>sdk.undo());assert.deepEqual(await readRow(),clearance);
    await page.evaluate(()=>sdk.redo());assert.deepEqual(await readRow(),crossed);
    for(const c of children){const saved=await read(c.objectId);assert.deepEqual(saved.start,c.start);assert.deepEqual(saved.end,c.end);}
    const before=await count();
    await page.evaluate(async ids=>{for(const objectId of ids)await sdk.setConnectorRouting({objectId,routing:1});},
      [center,left,far].map(x=>x.wire.objectId));
    const waitForFamily=async enabled=>page.waitForFunction(({id,mid,enabled})=>{
      const c=sdk.readConnector(id);
      return c.routeStatus===1 && (c.route.length===4 && Math.abs(c.route[1][1]-mid)<.02)===enabled;
    },{id:right.wire.objectId,mid,enabled},{timeout:3500});
    await waitForFamily(false);assert.equal(await count(),before+3);
    await page.evaluate(()=>sdk.undo());await waitForFamily(true);assert.equal(await count(),before+4);
  });
  await open();
  await test('two outward perpendicular port drags keep distinct upper and lower routes',async()=>{
    await page.locator('#shape-tool').click();
    const a=await draw('rectangle',[320,360],[440,440]), b=await draw('rectangle',[670,422],[790,502]);
    const clip={x:245,y:282,width:620,height:284};
    const routes=[], held=[], wires=[];
    const pairs=[
      {ports:[3,2],button:[380,a.bounds[3]+28],target:[790,462]},
      {ports:[4,1],button:[a.bounds[0]-28,400],target:[730,422]},
    ];
    for (const {ports,button,target} of pairs){
      await page.mouse.click(380,400);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,a.primary);
      const n=await count(),total=await page.evaluate(()=>sdk.readState().objects);
      await drag(button,target,false);
      held.push(await page.screenshot({clip,path:path.join(out,'perpendicular-drag-'+ports[0]+'.png')}));
      assert.equal(await count(),n);
      await page.mouse.up();
      await page.waitForFunction(n=>sdk.readState().objects===n+1,total,{timeout:3500});
      const c=await read();
      assert.equal(c.routeStatus,1);assert(c.endArrow);assert.equal(await count(),n+1);
      assert.deepEqual(c.start,{objectId:a.primary,port:ports[0]});
      assert.deepEqual(c.end,{objectId:b.primary,port:ports[1]});
      wires.push(c.objectId);routes.push(c.route.map(p=>world(c.worldTransform,p)));
    }
    await page.mouse.click(980,615);
    const screen=await page.screenshot({clip,path:path.join(out,'two-connectors.png')});
    const png=await page.evaluate(async c=>[...(await sdk.exportImage({
      format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
    await fs.writeFile(path.join(out,'two-connectors-export.png'),Buffer.from(png));
    const runs=points=>points.slice(1).map(([x,y],i)=>{
      const [px,py]=points[i];return y===py ? ['h',y,Math.min(x,px),Math.max(x,px)]
                                            : ['v',x,Math.min(y,py),Math.max(y,py)];
    });
    const sharedLength=(first,last)=>{
      let length=0;
      for(const [axis,pos,l,r] of runs(first))for(const [other,at,ll,rr] of runs(last))
        if(axis===other && pos===at)length+=Math.max(0,Math.min(r,rr)-Math.max(l,ll));
      return length;
    };
    const [lower,upper]=routes,shared=sharedLength(lower,upper);
    const tips=routes.map(p=>p.at(-1));
    const intake=routes.map(p=>Math.hypot(p.at(-2)[0]-p.at(-1)[0],p.at(-2)[1]-p.at(-1)[1]));
    const flanks=[[tips[0][0]+14,tips[0][1]-3.5],[tips[0][0]+14,tips[0][1]+3.5],
      [tips[1][0]-3.5,tips[1][1]-14],[tips[1][0]+3.5,tips[1][1]-14]];
    const pixels={screen:await redSamples(screen,clip,flanks),export:await redSamples(png,clip,flanks)};
    await fs.writeFile(path.join(out,'two-connectors.json'),JSON.stringify({routes,shared,intake,pixels},null,2));
    assert.equal(shared,0,'length of the overlapping strokes');
    assert.deepEqual(routes.map(p=>p.length),[5,5],'three turns per line');
    assert(lower[1][1]>=502+16 && upper[2][1]<=360-16,'use opposite outer sides');
    assert(intake.every(v=>v>=32),'straight approach before each tip');
    for(const sample of Object.values(pixels))assert(sample.every(v=>v<190),'full arrowhead flanks: '+sample);
    const lanes=[[552,lower[1][1]],[552,upper[2][1]]];
    for(const img of [screen,png])assert((await redSamples(img,clip,lanes)).every(v=>v<190));
    for(const [i,img] of held.entries())assert((await redSamples(img,clip,[lanes[i]]))[0]<245,'held route matches release');
    for(const [i,lane] of lanes.entries()){
      await page.mouse.click(...lane);assert.equal((await read()).objectId,wires[i],'pick the individual line');
    }
    await page.evaluate(()=>sdk.undo());assert.equal((await read(wires[0])).available,true);
    await page.evaluate(()=>sdk.redo());const restored=await read(wires[1]);
    assert.deepEqual(restored.route.map(p=>world(restored.worldTransform,p)),upper);
    // The recording also drags a third wire out of the top port. Its
    // horizontal run and the line passing above that face remain distinct.
    await page.mouse.click(380,400);await page.waitForFunction(id=>sdk.readSelection().primary===id,a.primary);
    const n=await count(),total=await page.evaluate(()=>sdk.readState().objects);
    await drag([380,a.bounds[1]-28],[790,462],false);
    const thirdHeld=await page.screenshot({clip,path:path.join(out,'third-preview.png')});
    assert.equal(await count(),n);
    await page.mouse.up();await page.waitForFunction(n=>sdk.readState().objects===n+1,total);
    const third=await read(),path3=third.route.map(p=>world(third.worldTransform,p));
    assert.deepEqual(third.start,{objectId:a.primary,port:1});assert.deepEqual(third.end,{objectId:b.primary,port:2});
    assert.equal(sharedLength(upper,path3),0);
    assert.equal(sharedLength(lower,path3),intake[0],'only the common approach to the right port overlaps');
    const topLanes=[[552,upper[2][1]],[552,path3[2][1]]];
    assert(topLanes[0][1]-topLanes[1][1]>=12);
    assert((await redSamples(thirdHeld,clip,topLanes)).every(v=>v<245));
    await page.evaluate(()=>sdk.undo());assert.equal(await page.evaluate(()=>sdk.readState().objects),total);
  });
  await open();
  await test('moving a branch frees the original port position for the next hover and click',async()=>{
    await page.locator('#shape-tool').click();
    const source=await draw('rectangle',[480,315],[600,395]), port=[540,source.bounds[3]+28];
    await page.mouse.click(...port);
    await page.waitForFunction(()=>sdk.readConnector().available);
    const first=await read(), original=world(first.worldTransform,first.route.at(-1));
    await page.mouse.click(original[0],original[1]+42);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,first.end.objectId);
    await page.evaluate(async()=>{
      const pose=[...sdk.readSelection().transform];pose[4]+=248;pose[5]-=146;
      await sdk.transformSelection(pose);
    });
    const moved=await read(first.objectId);
    const clip={x:335,y:272,width:552,height:365};
    await page.mouse.click(540,350);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
    const n=await count(),total=await page.evaluate(()=>sdk.readState().objects);
    await page.mouse.move(...port);await page.waitForTimeout(100);
    const hover=await page.screenshot({clip,path:path.join(out,'default-site-hover.png')});
    const ink=await redSamples(hover,clip,[[original[0],original[1]+29]]);
    assert.equal(await count(),n);
    await page.mouse.click(...port);
    await page.waitForFunction(total=>sdk.readState().objects===total+2,total,{timeout:3500});
    const next=await read(),landed=world(next.worldTransform,next.route.at(-1));
    await fs.writeFile(path.join(out,'default-site.json'),JSON.stringify({
      original,moved:world(moved.worldTransform,moved.route.at(-1)),landed,hoverPixel:ink[0]},null,2));
    assert(near(landed,original),'free original site should win: '+landed+' instead of '+original);
    assert(ink[0]<255,'the ghost occupied the original site');
    assert.equal(await count(),n+1);assert.deepEqual(next.start,first.start);
    assert.notEqual(next.end.objectId,first.end.objectId);
    assert.deepEqual((await read(first.objectId)).route,moved.route);
    await page.evaluate(()=>sdk.undo());assert.equal(await page.evaluate(()=>sdk.readState().objects),total);
    await page.evaluate(()=>sdk.redo());assert.deepEqual((await read(next.objectId)).route,next.route);
  });
  await open();
  await test('a port creates its target beyond a closed ring, then reroutes through an opened gate',async()=>{
    await page.locator('#shape-tool').click();
    const source=await draw('rectangle',[500,315],[580,375]);
    const walls=[];
    for(const [a,b] of [[[430,255],[650,275]],[[430,258],[455,447]],
      [[625,258],[650,447]],[[430,423],[650,445]]])walls.push(await draw('rectangle',a,b));
    const other=await draw('rectangle',[522,470],[558,487]);
    await page.mouse.click(540,345);await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary);
    const n=await count(), total=await page.evaluate(()=>sdk.readState().objects);
    const port=[540,source.bounds[3]+28],expected=[540,523];
    const clip={x:395,y:235,width:295,height:394};
    await page.mouse.move(...port);await page.waitForTimeout(120);
    const ghost=await page.screenshot({clip,path:path.join(out,'enclosed-preview.png')});
    assert.equal(await count(),n);
    await page.mouse.click(...port);
    await page.waitForFunction(total=>sdk.readState().objects===total+2,total,{timeout:3500});
    const line=await read(),geom=c=>c.route.map(p=>world(c.worldTransform,p));
    assert.equal(line.routeStatus,1);assert(near(geom(line).at(-1),expected,.1));
    assert.equal(await count(),n+1);assert.equal(line.start.objectId,source.primary);
    assert.equal(line.start.port,3);assert.equal(line.end.port,1);
    assert((await redSamples(ghost,clip,[[540,555]]))[0]<255);
    const crossingPoint=(c,rect)=>{
      const p=geom(c),[l,t,r,b]=rect;
      for(let i=1;i<p.length;i++){
        const [x,y]=p[i],[px,py]=p[i-1],vertical=x===px;
        const lo=vertical?Math.max(t,Math.min(y,py)):Math.max(l,Math.min(x,px));
        const hi=vertical?Math.min(b,Math.max(y,py)):Math.min(r,Math.max(x,px));
        if(lo<hi && (vertical?x>l && x<r:y>t && y<b))
          return vertical?[x,(lo+hi)/2]:[(lo+hi)/2,y];
      }
      return null;
    };
    const pierced=walls.map(w=>crossingPoint(line,w.bounds)).filter(Boolean);
    assert.equal(pierced.length,1,'cross only one enclosing object');
    assert(!crossingPoint(line,other.bounds),'avoid the additional obstacle');
    await page.mouse.click(780,620);
    const shown=await page.screenshot({clip,path:path.join(out,'enclosed-connected.png')});
    assert.equal(await countWarningPixels(shown),0);
    const exported=await page.evaluate(async c=>[...(await sdk.exportImage({
      format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:2})).bytes],clip);
    await fs.writeFile(path.join(out,'enclosed-export.png'),Buffer.from(exported));
    assert.equal(await countWarningPixels(exported),0);
    for(const png of [shown,exported]) assert((await redSamples(png,clip,pierced))[0]<185,'shaft inside the crossed object');
    await page.mouse.click(467,434);await page.waitForFunction(id=>sdk.readSelection().primary===id,walls[3].primary);
    const gate=await page.evaluate(async()=>{
      const t=[...sdk.readSelection().transform];t[4]+=64;
      await sdk.transformSelection(t);return sdk.readSelection().bounds;
    });
    await page.waitForFunction(({id,before})=>{
      const c=sdk.readConnector(id);return c.routeStatus===1 && JSON.stringify(c.route)!==JSON.stringify(before);
    },{id:line.objectId,before:line.route},{timeout:3500});
    const openGate=await read(line.objectId);
    assert.equal(openGate.routeStatus,1);assert.deepEqual(openGate.start,line.start);assert.deepEqual(openGate.end,line.end);
    for(const rect of [...walls.slice(0,3).map(w=>w.bounds),gate,other.bounds])assert(!crossingPoint(openGate,rect));
    assert.equal(await count(),n+2);
    await fs.writeFile(path.join(out,'enclosed-route.json'),JSON.stringify({closed:geom(line),open:geom(openGate)},null,2));
    await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(line.objectId)).route,line.route);
    const rebound=await page.evaluate(async({start,end})=>{
      const {objectId}=await sdk.createConnector({start,end:{point:[750,570]},routing:2,endArrow:true});
      const first=sdk.readConnector(objectId);
      await sdk.setConnectorEndpoint({objectId,side:'end',generation:first.generation,endpoint:end});
      return sdk.readConnector(objectId);
    },{start:line.start,end:line.end});
    assert.equal(rebound.routeStatus,1);assert.deepEqual(geom(rebound),geom(line));
  });
  for (const layout of ['between','outside','corner']) {
    await open();
    await test('bottom-to-top arrow keeps its size in the '+layout+' layout',async()=>{
      await page.locator('#shape-tool').click();
      const a=await draw('rectangle',[240,315],[420,425]);
      const [x,y]={between:[516,425],outside:[420,315],corner:[422,441]}[layout];
      const b=await draw('rectangle',[x,y+35],[x+180,y+145]);
      await page.evaluate(()=>sdk.appearance.setDefaults({strokeEnabled:true,stroke:[.14,.28,.42,1],strokeWidth:2,cornerRadius:4}));
      const id=await page.evaluate(async({a,b})=>(await sdk.createConnector({
        start:{objectId:a,port:3},end:{objectId:b,port:1},endArrow:true})).objectId,{a:a.primary,b:b.primary});
      const initial=await read(id);
      await page.mouse.click(990,650);
      await page.mouse.click(x+90,y+80);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,b.primary);
      const n=await count();
      await drag([x+115,y+85],[x+115,y+50],false);
      const clip={x:185,y:258,width:555,height:310};
      const held=await page.screenshot({clip,path:path.join(out,layout+'-drag.png')});
      assert.equal(await count(),n);
      await page.mouse.up();
      // Alignment guides can snap the painted borders two units apart.
      await page.waitForFunction(({id,y})=>Math.abs(sdk.readConnector(id).route.at(-1)[1]-y)<=3,{id,y},{timeout:3500});
      const wire=await read(id);
      assert.equal(await count(),n+1);assert.equal(wire.routeStatus,1);
      assert.deepEqual(wire.start,initial.start);assert.deepEqual(wire.end,initial.end);
      const points=wire.route.map(p=>world(wire.worldTransform,p));
      if(layout==='corner')await page.screenshot({clip,path:path.join(out,'corner-selected.png')});
      await page.mouse.click(990,650);
      const screen=await page.screenshot({clip,path:path.join(out,layout+'.png')});
      const png=await page.evaluate(async c=>[...(await sdk.exportImage({
        format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
      await fs.writeFile(path.join(out,layout+'-export.png'),Buffer.from(png));
      // Both flanks 10 units behind the displayed tip belong to a full
      // 12-unit head, clear of the shaft, selection outline and shape body.
      const [tipX,tipY]=points.at(-1);
      const flanks=[[-3.5,-14],[3.5,-14]].map(([dx,dy])=>[tipX+dx,tipY+dy]);
      const profiles={held:await redSamples(held,clip,flanks),screen:await redSamples(screen,clip,flanks),
        png:await redSamples(png,clip,flanks)};
      const legs=[points[1][1]-points[0][1],points.at(-1)[1]-points.at(-2)[1]];
      await fs.writeFile(path.join(out,layout+'.json'),JSON.stringify({points,legs,profiles},null,2));
      assert(legs.every(v=>v>=32),'short terminal runs: '+legs);
      for(const [view,samples] of Object.entries(profiles))
        assert(samples.every(v=>v<190),view+' is missing the arrowhead flanks: '+samples);
      if(layout==='corner'){
        assert(points.length<=8 && points.every(([px,py])=>px>=points[0][0] && px<=tipX &&
          py>=tipY-32 && py<=points[0][1]+32),'keep the route between the two nearby corners');
      }else{
        assert.equal(points.length,6);
        if(layout==='outside')assert(points[2][0]<240 || points[2][0]>x+180,'route around both bodies');
        else assert.equal(points[2][0],(420+x)/2,'turn in the middle of the gap');
      }
      await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(id)).route,initial.route);
      await page.evaluate(()=>sdk.redo());assert.deepEqual((await read(id)).route,wire.route);
    });
  }
  await open();
  await test('two bottom arrows keep their size as the rectangles move into alignment',async()=>{
    await page.locator('#shape-tool').click();
    const a=await draw('rectangle',[250,315],[400,415]),b=await draw('rectangle',[590,291],[740,391]);
    await page.evaluate(()=>sdk.appearance.setDefaults({strokeEnabled:true,stroke:[.14,.28,.42,1],strokeWidth:2,cornerRadius:4}));
    const id=await page.evaluate(async({a,b})=>(await sdk.createConnector({
      start:{objectId:a,port:3},end:{objectId:b,port:3},startArrow:true,endArrow:true})).objectId,{a:a.primary,b:b.primary});
    const first=await read(id);
    const clip={x:225,y:280,width:550,height:205};
    const points=c=>c.route.map(p=>world(c.worldTransform,p));
    const legs=c=>{const p=points(c);return [p[1][1]-p[0][1],p.at(-2)[1]-p.at(-1)[1]];};
    assert(legs(first).every(v=>v>=32));assert(first.startArrow && first.endArrow);
    await page.mouse.click(990,635);
    await page.mouse.click(665,341);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,b.primary);
    const n=await count();
    await drag([693,330],[693,354],false);
    const held=await page.screenshot({clip,path:path.join(out,'aligned-while-dragging.png')});
    assert.equal(await count(),n);assert.deepEqual((await read(id)).route,first.route);
    await page.mouse.up();
    await page.waitForFunction(id=>Math.abs(sdk.readConnector(id).route.at(-1)[1]-415)<.05,id,{timeout:3500});
    const aligned=await read(id);assert.equal(await count(),n+1);
    assert.deepEqual(aligned.start,first.start);assert.deepEqual(aligned.end,first.end);
    await page.mouse.click(990,635);
    const screen=await page.screenshot({clip,path:path.join(out,'aligned-arrows.png')});
    const exported=await page.evaluate(async c=>[...(await sdk.exportImage({
      format:'png',bounds:[c.x,c.y,c.x+c.width,c.y+c.height],padding:0,scale:3})).bytes],clip);
    // Sample the outer arrow flanks and the middle of the shared lane.
    const profile=bytes=>redSamples(bytes,clip,[[321.5,429],[668.5,429],[495,447]]);
    const profiles={held:await profile(held),screen:await profile(screen),png:await profile(exported)};
    const results={before:points(first),aligned:points(aligned),legs:legs(aligned),profiles};
    await fs.writeFile(path.join(out,'aligned-arrows.json'),JSON.stringify(results,null,2));
    assert(legs(aligned).every(v=>v>=32),'an aligned U must keep its terminal runs: '+legs(aligned));
    for(const [frame,values] of Object.entries(profiles)) assert(values.every(red=>red<190),frame+' lost a full arrow or its outer lane: '+values);
    await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(id)).route,first.route);
    await page.evaluate(()=>sdk.redo());assert.deepEqual((await read(id)).route,aligned.route);
    await page.evaluate(async({id,a,b})=>{
      let c=sdk.readConnector(id);
      await sdk.setConnectorEndpoint({objectId:id,side:'start',generation:c.generation,endpoint:{objectId:a,port:2}});
      c=sdk.readConnector(id);
      await sdk.setConnectorEndpoint({objectId:id,side:'end',generation:c.generation,endpoint:{objectId:b,port:4}});
    },{id,a:a.primary,b:b.primary});
    const straight=await read(id);
    assert.deepEqual(points(straight),[[400,365],[590,365]],'aligned facing ports connect directly');
  });
  await open();
  await test('a dragged end follows the near bottom edge of two touching rectangles',async()=>{
    await page.locator('#shape-tool').click();
    const a=await draw('rectangle',[250,305],[400,405]),b=await draw('rectangle',[400,305],[550,405]);
    const lower=await draw('rectangle',[250,530],[400,630]);
    const selection=await draw('arrow',[325,405],[325,530]),id=selection.primary,first=await read(id);
    assert.deepEqual(first.start,{objectId:a.primary,port:3});assert.deepEqual(first.end,{objectId:lower.primary,port:1});
    const n=await count(),end=world(first.worldTransform,first.displayRoute.at(-1));
    await drag(end,[518,427],false);
    const guideClip={x:384,y:290,width:182,height:132};
    const guides=await page.screenshot({clip:guideClip,path:path.join(out,'nearby-ports.png')});
    const markers=await portInk(guides,[[86,10,10,10],[161,60,10,10],[86,110,10,10],[11,60,10,10]]);
    assert(markers.every(n=>n>=4*dpr*dpr),'show four ports before capture: '+markers);
    await page.mouse.move(518,410);await page.waitForTimeout(100);
    const held=await page.screenshot({clip:{x:220,y:281,width:360,height:380},path:path.join(out,'touching-edge-preview.png')});
    const orange=await countWarningPixels(held);
    const [landingRing,duplicateGrip]=await portInk(held,[[292,118,12,12],[296,132,4,4]]);
    assert(landingRing>=12*dpr*dpr && duplicateGrip===0,'one active ring at the snapped rim: '+[landingRing,duplicateGrip]);
    assert.equal(await count(),n);assert.deepEqual((await read(id)).end,first.end);
    // Stay inside the release radius as the pointer slides along the same rim.
    await page.mouse.move(508,417);await page.mouse.up();
    await page.waitForTimeout(120);
    const c=await read(id),tip=world(c.worldTransform,c.route.at(-1));
    await fs.writeFile(path.join(out,'touching-drop.json'),JSON.stringify({end:c.end,tip,route:c.route,status:c.routeStatus,receipt:c.endpointEdit,orange},null,2));
    assert.equal(c.end.objectId,b.primary,'the exposed bottom face accepts the drop');
    assert(c.end.hint && !c.end.port);assert(near(tip,[508,405],.35),'actual rim landing: '+tip);
    assert.equal(orange,0,'the available route stays out of warning state');
    assert.equal(c.routeStatus,1);assert.equal(c.endpointEdit.state,'applied');assert.equal(await count(),n+1);
    assert.deepEqual(c.start,first.start);await page.evaluate(()=>sdk.undo());assert.deepEqual((await read(id)).end,first.end);
    await page.evaluate(()=>sdk.redo());assert.deepEqual((await read(id)).end,c.end);
    const restored=await read(id),prior=await count();
    await drag(world(restored.worldTransform,restored.displayRoute.at(-1)),[400,355],false);
    const blocked=await page.screenshot({clip:{x:245,y:290,width:325,height:158},path:path.join(out,'occluded-port.png')});
    assert((await countWarningPixels(blocked))>8,'an enclosed port still warns');
    await page.mouse.up();await page.waitForTimeout(100);
    const rejected=await read(id);assert.deepEqual(rejected.end,c.end);assert.equal(rejected.endpointEdit.state,'rejected');
    assert.equal(await count(),prior);
  });
  await open();
  await test('the arrow follows the pointer along the far side of its original target',async()=>{
    const top=await draw('rectangle',[250,305],[400,405]),bottom=await draw('rectangle',[250,530],[400,630]);
    const id=(await draw('arrow',[325,405],[325,530])).primary,first=await read(id);
    const end=world(first.worldTransform,first.displayRoute.at(-1));
    await drag(end,[406,610],false);
    const clip={x:236,y:511,width:192,height:132};
    await page.screenshot({clip,path:path.join(out,'slide-right-preview.png')});
    await page.mouse.move(405,605);await page.mouse.up();await page.waitForTimeout(100);
    const wire=await read(id),tip=world(wire.worldTransform,wire.route.at(-1));
    assert.deepEqual(wire.start,{objectId:top.primary,port:3});assert.equal(wire.end.objectId,bottom.primary);
    assert(near(tip,[400,605],.35),'the right edge under the pointer is used, landed '+tip);
    assert(wire.end.hint && !wire.end.port);
    await frameBoardAt(1220,920,2,[400,605],[600,450]);
    const again=world(wire.worldTransform,wire.displayRoute.at(-1));
    const cam=await page.evaluate(()=>sdk.readState().camera),view=p=>[p[0]*cam.scale+cam.x,p[1]*cam.scale+cam.y];
    await drag(view(again),view([403,555]));const moved=await read(id);
    assert(near(world(moved.worldTransform,moved.route.at(-1)),[400,555],.4));
    await page.mouse.click(1080,600);await page.screenshot({clip:{x:275,y:238,width:365,height:290},path:path.join(out,'slide-right-committed.png')});
  });
  await open();
  await test('a nearby source plus cannot take a dragged end from the other shape',async()=>{
    const a=await draw('rectangle',[300,330],[420,430]),b=await draw('rectangle',[449,315],[579,409]);
    await page.mouse.click(360,380);await page.waitForFunction(id=>sdk.readSelection().primary===id,a.primary);
    const plus=[a.bounds[2]+28,380],total=await page.evaluate(()=>sdk.readState().objects);
    await drag(plus,[451,390]);
    await page.waitForFunction(n=>sdk.readState().objects===n+1,total,{timeout:2500});
    const c=await read();
    assert.deepEqual(c.start,{objectId:a.primary,port:2});assert.equal(c.end.objectId,b.primary);assert(c.end.hint);
    assert(near(world(c.worldTransform,c.route.at(-1)),[449,390],.35));
  });
  await open();
  await test('drag construction stores both picked sides and replays a transformed ellipse attachment',async()=>{
    const rectangle=await draw('rectangle',[270,340],[400,450]),ellipse=await draw('ellipse',[615,340],[775,450]);
    const pose=[.9,.35,-.25,1.1,672,338];
    await page.evaluate(t=>sdk.transformSelection(t),pose);
    await frameBoardAt(1220,920,1.5,[510,415],[570,425]);
    const cam=await page.evaluate(()=>sdk.readState().camera),view=([x,y])=>[cam.x+x*cam.scale,cam.y+y*cam.scale];
    const theta=.68,local=[80+80*Math.cos(theta),55+55*Math.sin(theta)];
    const edge=world(pose,local),tx=-80*Math.sin(theta),ty=55*Math.cos(theta);
    const vx=pose[0]*tx+pose[2]*ty,vy=pose[1]*tx+pose[3]*ty;
    const at=view(edge),pointer=[at[0]+5*vy/Math.hypot(vx,vy),at[1]-5*vx/Math.hypot(vx,vy)];
    const id=(await draw('arrow',view([267,423]),pointer)).primary;
    const c=await read(id);
    assert.equal(c.start.objectId,rectangle.primary);assert(c.start.hint);
    assert.equal(c.end.objectId,ellipse.primary);assert(c.end.hint);
    assert(near(world(c.worldTransform,c.route[0]),[270,423],.9));
    assert(near(world(c.worldTransform,c.route.at(-1)),edge,.9));
    const replay=await page.evaluate(async id=>{
      const {createRuntime}=await import('./axiom.mjs');
      const mount=()=>{const e=document.createElement('div');e.style.cssText='position:absolute;left:-2200px;width:800px;height:600px';document.body.append(e);return e;};
      const checkpoint={generation:sdk.readState().generation,documentRevision:'1',recoveryFrontier:[8,1,16,1,24,1]};
      const snap=await sdk.captureSnapshot(checkpoint),documentId=sdk.readState().documentId;
      const m1=mount(),m2=mount();
      const a=await createRuntime({mount:m1,documentId,permission:'localSingleUser',restore:{bytes:snap.bytes,metadata:{...checkpoint,checkpointDigest:snap.checkpointDigest,resourceBinding:'1'}}});
      const b=await createRuntime({mount:m2,documentId,permission:'localSingleUser'});
      try{for(const packet of packets)await b.applyRemoteOperation(packet);return [a.readConnector(id),b.readConnector(id)];}
      finally{await a.close();await b.close();m1.remove();m2.remove();}
    },id);
    for(const r of replay){assert.deepEqual(r.start,c.start);assert.deepEqual(r.end,c.end);assert.deepEqual(r.route,c.route);}
    await page.evaluate(({id,objectId,generation})=>sdk.setConnectorEndpoint({objectId:id,side:'end',endpoint:{objectId},generation}),
      {id,objectId:ellipse.primary,generation:c.generation});
    const facing=await read(id);assert.deepEqual(facing.end,{objectId:ellipse.primary});
    assert(world(facing.worldTransform,facing.route.at(-1))[0]<edge[0]-70,'an explicit hintless endpoint still faces the other end');
  });
  await open();
  await test('a touch drag captures, slides and releases at the touch radius',async()=>{
    const target=await draw('rectangle',[500,340],[660,450]);
    const id=(await draw('arrow',[268,528],[406,526])).primary,original=await read(id);
    const touch=await page.context().newCDPSession(page);
    const send=(type,x,y)=>touch.send('Input.dispatchTouchEvent',{type,touchPoints:
      type==='touchStart' || type==='touchMove' ? [{id:7,x,y,radiusX:2,radiusY:2,force:.7}] : []});
    await touch.send('Emulation.setTouchEmulationEnabled',{enabled:true,maxTouchPoints:5});
    try{
      const n=await count();
      await send('touchStart',...world(original.worldTransform,original.displayRoute.at(-1)));
      await send('touchMove',625,465);await page.waitForTimeout(100);
      await send('touchMove',634,470);await send('touchEnd');await page.waitForTimeout(120);
      const attached=await read(id);
      assert.equal(attached.end.objectId,target.primary);assert(attached.end.hint);
      assert(near(world(attached.worldTransform,attached.route.at(-1)),[634,450],.75));assert.equal(await count(),n+1);
      const grip=world(attached.worldTransform,attached.displayRoute.at(-1));
      await send('touchStart',...grip);await send('touchMove',633,480);await send('touchCancel');await page.waitForTimeout(90);
      assert.deepEqual((await read(id)).end,attached.end);assert.equal(await count(),n+1);
      await send('touchStart',...grip);await send('touchMove',633,480);await send('touchEnd');await page.waitForTimeout(100);
      assert.deepEqual((await read(id)).end,{point:[633,480]});assert.equal(await count(),n+2);
    }finally{await touch.send('Emulation.setTouchEmulationEnabled',{enabled:false});await touch.detach();}
  });
  await open();
  await test('a bottom port continues the established board row after pan, zoom and window resize',async()=>{
    const source=await draw('rectangle',[480,320],[600,395]);
    const children=[], wires=[];
    // The board already has two nodes on one row and a third at another depth.
    for(const [x,y] of [[208,573],[756,573],[540,503]]){
      const child=await draw('rectangle',[x-60,y],[x+60,y+75]);
      const id=await page.evaluate(async({a,b})=>(await sdk.createConnector({
        start:{objectId:a,port:3},end:{objectId:b,port:1},endArrow:true})).objectId,{a:source.primary,b:child.primary});
      children.push(child.primary);wires.push(id);
    }
    const original=await Promise.all(wires.map(read)), landed=[];
    await frameBoardAt(875,825,.7,[540,350],[360,370]);
    for(let i=0;i<3;++i){
      if(i===1) {
        await frameBoardAt(1390,965,2.5,[540,350],[700,360]);
      }
      const camera=await page.evaluate(()=>sdk.readState().camera);
      const at=([x,y])=>[x*camera.scale+camera.x,y*camera.scale+camera.y];
      await page.mouse.click(...at([540,350]));
      await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary,{timeout:3000});
      const port=at([540,source.bounds[3]]);port[1]+=28;
      const n=await count(),total=await page.evaluate(()=>sdk.readState().objects);
      await page.mouse.move(...port);await page.waitForTimeout(80);
      const ghost=await page.screenshot();
      assert.equal(await count(),n);
      await page.mouse.click(...port);
      await page.waitForFunction(n=>sdk.readState().objects===n+2,total,{timeout:3500});
      const c=await read(),tip=world(c.worldTransform,c.route.at(-1));
      assert.deepEqual(c.start,{objectId:source.primary,port:3});
      assert.equal(c.end.port,1);assert.equal(await count(),n+1);
      assert(Math.abs(tip[1]-573)<.01,'new sibling should continue y=573, landed at '+tip);
      const pixel=at([tip[0],tip[1]+23]);
      const viewport=page.viewportSize();
      if(pixel[0]>0 && pixel[0]<viewport.width && pixel[1]>130 && pixel[1]<viewport.height-190){
        const color=await page.evaluate(async({bytes,px,py})=>{
          const img=await createImageBitmap(new Blob([new Uint8Array(bytes)],{type:'image/png'}));
          const ctx=new OffscreenCanvas(img.width,img.height).getContext('2d');ctx.drawImage(img,0,0);
          const rgba=Array.from(ctx.getImageData(px,py,1,1).data);img.close();return rgba;
        },{bytes:[...ghost],px:Math.floor(pixel[0]*dpr),py:Math.floor(pixel[1]*dpr)});
        assert(color[2]>color[0],'hover at the future board position');
      }
      landed.push(tip);
    }
    for(const [i,id] of wires.entries()){
      const c=await read(id);assert.deepEqual(c.end,{objectId:children[i],port:1});
      assert.deepEqual(c.route.at(-1),original[i].route.at(-1),'existing nodes keep their positions');
    }
    assert.equal(new Set(landed.map(p=>p[0])).size,3);
    await fs.writeFile(path.join(out,'existing-row.json'),JSON.stringify({
      existing:original.map(c=>world(c.worldTransform,c.route.at(-1))),new:landed},null,2));
    await page.evaluate(()=>sdk.fitContent());
    await page.waitForTimeout(80);
    await page.screenshot({path:path.join(out,'port-board-row.png')});
    await page.setViewportSize({width:1220,height:920});
  });
  await open();
  await test('the first landing follows the shape perimeter when its painted outline changes',async()=>{
    const source=await draw('rectangle',[485,330],[605,415]);
    const ends=[];
    for(const width of [2,20]){
      if((await page.evaluate(()=>sdk.readSelection().primary))!==source.primary)await page.mouse.click(545,372);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary,{timeout:3000});
      await page.evaluate(w=>sdk.setAppearance({selected:true,strokeEnabled:true,stroke:[.2,.4,.6,1],strokeWidth:w}),width);
      const shape=await page.evaluate(()=>sdk.readSelection());
      await page.mouse.click(545,shape.bounds[3]+28);
      await page.waitForFunction(()=>sdk.readState().objects===3,null,{timeout:3000});
      const wire=await read();ends.push(world(wire.worldTransform,wire.route.at(-1)));
      await page.evaluate(()=>sdk.undo());
      await page.waitForFunction(()=>sdk.readState().objects===1,null,{timeout:3000});
    }
    assert.deepEqual(ends[0],ends[1],'paint and the position of the + do not shift the first child');
  });
  await open();
  await test('clicking a port remains valid when its screen button covers the board landing',async()=>{
    const source=await draw('rectangle',[315,315],[785,525]),cx=550,cy=420;
    await page.mouse.click(cx,source.bounds[3]+28);
    await page.waitForFunction(()=>sdk.readConnector().available,null,{timeout:3500});
    const first=await read(),tip=world(first.worldTransform,first.route.at(-1));
    await frameBoardAt(1220,920,.25,tip,[550,465]);
    const cam=await page.evaluate(()=>sdk.readState().camera);
    const at=([x,y])=>[x*cam.scale+cam.x,y*cam.scale+cam.y];
    await page.mouse.click(...at([cx,cy]));
    await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary,{timeout:3000});
    const p=at(tip),handle=at([cx,source.bounds[3]]);handle[1]+=28;
    assert(Math.hypot(p[0]-handle[0],p[1]-handle[1])<12);
    const n=await count();await page.mouse.click(...p);
    await page.waitForFunction(()=>sdk.readState().objects===5,null,{timeout:3500});
    const second=await read(),next=world(second.worldTransform,second.route.at(-1));
    assert.equal(await count(),n+1);assert.equal(second.start.objectId,source.primary);
    assert.equal(next[1],tip[1]);assert.notEqual(next[0],tip[0]);
  });
  await open();
  await test('close targets keep their ports, a blocked port warns, and separating them recovers',async()=>{
    await page.locator('#shape-tool').click();
    const a=await draw('rectangle',[265,345],[415,455]),b=await draw('rectangle',[615,345],[765,455]);
    const selection=await draw('arrow',[415,400],[690,345]);
    const id=selection.primary,original=await read(id),results=[];
    const clip={x:240,y:292,width:430,height:186};
    let currentCenter=690;
    for(const gap of [8,0,-4,8]){
      await page.mouse.click(currentCenter,400);
      await page.waitForFunction(id=>sdk.readSelection().primary===id,b.primary,{timeout:3000});
      const transform=[...b.transform];transform[4]+=415+gap-615;
      await page.evaluate(pose=>sdk.transformSelection(pose),transform);
      currentCenter=415+gap+75;
      await page.waitForFunction(({id,x})=>{
        const w=sdk.readConnector(id);return w.routeStatus!==0 && Math.abs(w.route.at(-1)[0]-x)<.05;
      },{id,x:currentCenter});
      await page.mouse.click(1030,605);
      const w=await read(id),png=await page.screenshot({clip,path:path.join(out,'near-gap-'+gap+'.png')});
      const warningPixels=await countWarningPixels(png);
      assert.deepEqual(w.start,original.start);assert.deepEqual(w.end,original.end);assert.equal(w.objectId,id);
      results.push({gap,status:w.routeStatus,route:w.route,warningPixels});
      if(w.routeStatus===2){
        const start=world(w.worldTransform,w.route[0]),end=world(w.worldTransform,w.route.at(-1));
        await page.mouse.click((start[0]+end[0])/2,(start[1]+end[1])/2);
        await page.waitForFunction(id=>sdk.readSelection().primary===id,id,{timeout:3000});
        await page.mouse.click(1030,605);
      }
    }
    await fs.writeFile(path.join(out,'close-ports.json'),JSON.stringify(results,null,2));
    assert.deepEqual(results.map(x=>x.status),[1,2,2,1]);
    assert(results.every(x=>x.gap>0 ? x.warningPixels===0 : x.warningPixels>8),'only blocked ports show the warning');
    assert(results[0].route.some(([x,y])=>x>415 && x<423 && y<345),'the normal line uses the open channel');
    assert.deepEqual(results[0].route,results[3].route);
    assert.deepEqual(original.start,{objectId:a.primary,port:2});assert.deepEqual(original.end,{objectId:b.primary,port:1});
    // Close the slot and drag it open with the real pointer. The held frame is transient.
    await page.mouse.click(currentCenter,400);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,b.primary,{timeout:3000});
    const touching=[...b.transform];touching[4]-=200;
    await page.evaluate(t=>sdk.transformSelection(t),touching);
    await page.waitForFunction(id=>sdk.readConnector(id).routeStatus===2,id,{timeout:3000});
    await page.mouse.click(1030,605);
    await page.mouse.click(490,400);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,b.primary,{timeout:3000});
    const before=await count();
    await drag([490,424],[545,424],false);
    const held=await page.screenshot({clip,path:path.join(out,'drag-open.png')});
    assert.equal(await count(),before,'drag preview preserves the document');
    assert.equal(await countWarningPixels(held),0,'the warning clears during the drag');
    await page.mouse.up();
    await page.waitForFunction(id=>sdk.readConnector(id).routeStatus===1,id,{timeout:3000});
    assert.equal(await count(),before+1);
    assert.deepEqual((await read(id)).end,original.end);
  });
  await open();
  await test('external ports route around touching rectangles while keeping their original bindings',async()=>{
    const a=await draw('rectangle',[265,345],[415,455]),b=await draw('rectangle',[615,345],[765,455]);
    const selection=await draw('arrow',[340,455],[690,345]);
    await page.mouse.click(690,400);
    await page.waitForFunction(id=>sdk.readSelection().primary===id,b.primary,{timeout:3000});
    const transform=[...b.transform];transform[4]-=200;
    await page.evaluate(t=>sdk.transformSelection(t),transform);
    const id=selection.primary;
    await page.waitForFunction(id=>{
      const w=sdk.readConnector(id);return w.routeStatus===1 && Math.abs(w.route.at(-1)[0]-490)<.1;
    },id,{timeout:3000});
    await page.mouse.click(1030,605);
    const wire=await read(id),points=wire.route.map(p=>world(wire.worldTransform,p));
    assert.deepEqual(wire.start,{objectId:a.primary,port:3});assert.deepEqual(wire.end,{objectId:b.primary,port:1});
    assert(points.some(([x])=>x<265 || x>565),'the interior corridor is closed');
    assert(points.some(([,y])=>y>455) && points.some(([,y])=>y<345));
    await page.screenshot({clip:{x:200,y:285,width:445,height:242},path:path.join(out,'around-touch.png')});
  });
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
  await test('nine children share a board row and branch line across viewport changes',async()=>{
    await page.locator('#shape-tool').click();
    const source=await draw('rectangle',[420,300],[500,360]),focus=[460,330],samples=[],wires=[];
    const views=[
      [1220,920,1,610,340],[830,765,.55,290,305],[1430,915,2.5,415,310],
      [930,790,1.3,580,390],[1120,900,.25,530,330],[1470,990,3,955,410],
      [910,815,.1,350,310],[1040,880,1.6,610,355],[1220,920,1,610,340]
    ];
    for(const [width,height,scale,x,y] of views){
      await frameBoardAt(width,height,scale,focus,[x,y]);
      if((await page.evaluate(()=>sdk.readSelection().primary))!==source.primary){
        // At low zoom the selected wire's endpoint handle can cover the small source.
        await page.mouse.click(x-80,y-115);
        await page.mouse.click(x,y);
      }
      await page.waitForFunction(id=>sdk.readSelection().primary===id,source.primary,{timeout:3000});
      const operations=await count(),total=await page.evaluate(()=>sdk.readState().objects);
      await page.mouse.click(x+2,y+(source.bounds[3]-focus[1])*scale+28);
      await page.waitForFunction(n=>sdk.readState().objects===n+2,total,{timeout:3500});
      const wire=await read(),points=wire.route.map(p=>world(wire.worldTransform,p)),tip=points.at(-1);
      assert.deepEqual(wire.start,{objectId:source.primary,port:3});assert.equal(wire.end.port,1);
      assert.equal(await count(),operations+1);assert.deepEqual(points[0],[460,360]);
      if(samples.length)assert(near([tip[1]],[samples[0].tip[1]]),'the board row survives camera changes: '+tip);
      if(tip[0]!==points[0][0])assert(points.some(([px,py],i)=>i && px!==points[i-1][0] &&
        py===points[i-1][1] && Math.abs(py-(points[0][1]+tip[1])/2)<.02),'aligned horizontal branches share the same midline');
      samples.push({width,height,scale,tip});wires.push(wire.objectId);
    }
    assert.equal(new Set(samples.map(s=>s.tip[0])).size,views.length);
    for(const [i,id] of wires.entries()){
      const wire=await read(id);assert.deepEqual(world(wire.worldTransform,wire.route.at(-1)),samples[i].tip);
    }
    await page.mouse.click(1150,675);
    await page.screenshot({clip:{x:70,y:268,width:1080,height:390},path:path.join(out,'nine-aligned-children.png')});
    await fs.writeFile(path.join(out,'port-axis.json'),JSON.stringify(samples,null,2));
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
    assert.equal(radius,4,'new elbow connector has a four-unit bend');
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
      const values=[[.5,.5],[-1.25,1.25]].map(([dx,dy])=>
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
    await page.evaluate(()=>sdk.selectShape('arrow'));assert.equal(await defaults(),4);
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
    assert(gaps.every(g=>Math.abs(g-4)<.01));
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
    const auto=await read(id);assert.equal(auto.end.objectId,c.primary);assert(auto.end.hint && !auto.end.port);
    assert(near(world(auto.worldTransform,auto.route.at(-1)),edge,1.8),'the ellipse binds at the picked outline');
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
    assert.equal(sx-ax,4);assert.equal(bx-ex,4);
    const samples=[[ax+2,ay],[bx-2,by],[sx+12,ay],[ex-8,by+2],[sx+8,ay+2]];
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
