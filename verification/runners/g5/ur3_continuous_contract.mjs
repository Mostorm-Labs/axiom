export const CSS=Object.freeze([1100,760]);
export function physicalRect(point,dpr,width=4,height=4){
 if(!point?.every(Number.isFinite)||![1,2,4].includes(dpr))throw Error('invalid DPR witness');
 const rect={x:Math.round(point[0]*dpr)-Math.floor(width/2),y:Math.round(point[1]*dpr)-Math.floor(height/2),width,height};
 if(rect.x<0||rect.y<0||rect.x+width>CSS[0]*dpr||rect.y+height>CSS[1]*dpr)throw Error('witness outside target');return rect;
}
export function cleanupPoint(camera){
 const view=[32,732],world=[(view[0]-camera.x)/camera.scale,(view[1]-camera.y)/camera.scale];
 // All tested ink is above world680; mixed objects at x350..1040.
 if(!world.every(Number.isFinite)||world[0]>=300||world[1]<=690)throw Error('camera lacks in-canvas cleanup proof');
 return{world,view,proof:'in-canvas view32,732 mapsworldx<300/y>690 belowinkandleftofmixed'};
}
export function afterWheel(camera,ctrl,dx,dy,x=550,y=380){
 if(![camera.scale,camera.x,camera.y,dx,dy,x,y].every(Number.isFinite))throw Error('camera invalid');
 if(!ctrl)return{scale:camera.scale,x:camera.x-dx,y:camera.y-dy};
 const factor=Math.exp(-dy*.01);return{scale:camera.scale*factor,x:x-factor*(x-camera.x),y:y-factor*(y-camera.y)};
}
export function typedPlan(kind,selection,typed,dpr){
 if(!selection?.transform||!typed?.available||typed.objectId!==selection.primary)throw Error('foreign typed witness');
 const width=kind==='table'?typed.columns.reduce((v,x)=>v+x.size,0):kind==='image'?typed.content.width:typed.width;
 const height=kind==='table'?typed.rows.reduce((v,x)=>v+x.size,0):kind==='image'?typed.content.height:typed.height;
 if(!(width>0&&height>0))throw Error('shape-only dimensions invalid');
 const t=selection.transform,point=[t[4]+width+24,t[5]+30+(kind==='table'?typed.rows[0].size:height/2)],color=kind==='sticky'?[255,255,128,255]:kind==='image'?[255,0,0,255]:kind==='table'?typed.grid.color.map(x=>Math.round(x*255)):[51,128,204,255];
 if(point[0]<=selection.bounds[2]+7||point[0]>=t[4]+width+48-7)throw Error('witness not new footprint');
 return{kind,width,height,point,color,rect:physicalRect(point,dpr,4,kind==='table'?1:4),delta:[48,30]};
}
export function positives(bytes,color){let n=0;for(let i=0;i<bytes.length;i+=4)if(color.every((v,c)=>Math.abs(bytes[i+c]-v)<=2))++n;return n;}
export const AFFINE=Object.freeze([.9*Math.cos(.15),.9*Math.sin(.15),-1.1*Math.sin(.15),1.1*Math.cos(.15),70,18]);
export function livePoint(index){return[120+index*.95,650+12*Math.sin(index*.055)];}
