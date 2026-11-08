// Public SDK consumer only. All brush/mask geometry is produced by Runtime.
import {SnapshotTransfer} from './stream_transfer.mjs';
export const correctnessCases=['lower_z_translucent_drag','permanent_live_mask','transparent_color_bg','warm_miss_pressure','dpr_switch','feedback_canonical'];
export function correctnessDriver({sdk,pointer,raf,settled,diag}){
 const transfer=new SnapshotTransfer();let kind,step=0;
 const wait=async()=>{await settled(performance.now()+10000);};
 const erase=async(points,finish=true)=>{await sdk.setEraser({mode:2,diameter:32});await sdk.selectTool('partialEraser');pointer('down',...points[0]);for(const p of points.slice(1)){pointer('move',...p);await raf();}if(finish){pointer('up',...points.at(-1));await wait();}};
 const rawPixels=(hud_bounds=[],live_erase=null)=>{
  const gl=sdk.canvas.getContext('webgl2'),width=gl.drawingBufferWidth,height=gl.drawingBufferHeight,raw=new Uint8Array(width*height*4),top=new Uint8Array(raw.length);
  gl.readPixels(0,0,width,height,gl.RGBA,gl.UNSIGNED_BYTE,raw);
  if(gl.getError()!==gl.NO_ERROR)throw Error('actual framebuffer readback failed');
  for(let y=0;y<height;++y)top.set(raw.subarray(y*width*4,(y+1)*width*4),(height-1-y)*width*4);
  transfer.begin(top);const r=sdk.canvas.getBoundingClientRect();
  return{byte_length:top.length,request:{width,height,logical_width:r.width,logical_height:r.height,camera:sdk.readState().camera,hud_bounds,...(live_erase?{live_erase}:{})}};
 };
 return{
  async setup(value,on){
   if(!correctnessCases.includes(value))throw Error('unknown frozen correctness case');kind=value;step=0;if(diag())diag().control(on?1:0,0);
   await sdk.selectBrush('round-handdrawn',4);await sdk.setInk({size:24,opacity:.55,color:[.05,.1,.2,1]});await sdk.selectTool('ink');
   for(let n=0;n<=256;++n){pointer(n===0?'down':n===256?'up':'move',100+600*n/256,100+270*Math.sin(Math.PI*n/256)+35*Math.sin(n/9));if(n%16===0)await raf();}
   await wait();
   if(kind==='lower_z_translucent_drag'){
    await sdk.setAppearance({selected:false,fill:[.85,.2,.1,1],fillEnabled:true,strokeEnabled:false});await sdk.selectShape('rectangle');
    pointer('down',280,150);pointer('move',540,370);pointer('up',540,370);await wait();
    await sdk.selectTool('select');pointer('down',100,100);pointer('up',100,100);await wait();if(sdk.readSelection().count!==1)throw Error('fixed lower-z ink target not selected');
   }else if(['permanent_live_mask','feedback_canonical'].includes(kind)){await erase([[350,180],[380,250],[410,330]]);}
   else{await sdk.selectTool('hand');await wait();}
   return{state:sdk.readState(),selection:sdk.readSelection(),config:sdk.readBrushConfig(),case_id:kind,steps:2};
  },
  async next(){
   if(step>=2)throw Error('correctness steps exhausted');const n=step++;let reference='after',live_erase=null,hud=[];
   if(kind==='lower_z_translucent_drag'){
    if(n===0){const b=sdk.readSelection().bounds;if(!b)throw Error('public selection bounds missing');pointer('down',100,100);pointer('move',124,112);for(let f=0;f<8;++f)await raf();
     const c=sdk.readState().camera;const convert=b=>[b[0]*c.scale+c.x,b[1]*c.scale+c.y,b[2]*c.scale+c.x,b[3]*c.scale+c.y];const original=convert(b);hud=[original,original.map((v,i)=>v+(i%2?12:24))];
     const pixels=rawPixels(hud);pointer('up',124,112);await wait();return{...pixels,reference,stage:'live-lower-z-drag',state:sdk.readState()};
    }await sdk.selectTool('hand');await wait();
   }else if(['permanent_live_mask','feedback_canonical'].includes(kind)){
    if(n===0){const points=[[260,240],[310,300],[360,340]];await erase(points,false);for(let f=0;f<8;++f)await raf();live_erase={view_points:points,diameter:32};reference='before';
     const pixels=rawPixels([],live_erase);pointer('up',...points.at(-1));await wait();return{...pixels,reference,stage:'permanent-plus-live-mask',state:sdk.readState()};
    }await wait();
   }else if(kind==='transparent_color_bg'){
    await sdk.setBackground({mode:'plain',color:n?[.2,.4,.7,1]:[0,0,0,0],patternColor:[0,0,0,0]});await wait();
   }else if(kind==='warm_miss_pressure'){
    for(let f=0;f<(n?16:1);++f){await sdk.zoomAt(400,250,n?(f%2?.5:2):1.1);await sdk.panBy(n?(f%2?-520:520):12,0);for(let k=0;k<2;++k)await raf();}await wait();
   }else if(kind==='dpr_switch'){
    // The browser worker changes real deviceScaleFactor before this step.
    await sdk.panBy(n?-1:1,0);await wait();
   }
   return{...rawPixels(hud,live_erase),reference,stage:kind+'-'+n,state:sdk.readState()};
  },pull:(offset,count)=>transfer.pull(offset,count),release:cancel=>transfer.end(cancel)
 };
}
