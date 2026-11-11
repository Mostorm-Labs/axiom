export const WINDOWS=Object.freeze({'G5WA1-BASELINE-WITNESS':240,'G5WA1-BASELINE-TAIL':300,'G5WA1-DIAGNOSTIC':360,'G5WA1-OVERHEAD':180});
export function typedWitness(kind,s,typed,resourceHash){
 if(!s||!typed?.available||typed.objectId!==s.primary||s.transform?.length!==6||s.bounds?.length!==4||
  [...s.transform,...s.bounds].some(x=>!Number.isFinite(x))||s.transform.slice(0,4).some((x,i)=>x!==[1,0,0,1][i]))throw Error('foreign/nonfinite witness basis');
 let width,height,color;
 if(kind==='sticky'){width=typed.width;height=typed.height;color=s.appearance?.fill;}
 else if(kind==='table'){
  if(typed.generation!==s.generation||!typed.rows?.length||!typed.columns?.length)throw Error('stale table');
  width=typed.columns.reduce((v,x)=>v+x.size,0);height=typed.rows.reduce((v,x)=>v+x.size,0);color=typed.grid.color;
 }else if(kind==='image'){
  width=typed.content.width;height=typed.content.height;color=[1,0,0,1];if(!resourceHash||!typed.content.resourceId)throw Error('image resource identity');
 }else throw Error('unsupported typed witness');
 if(!Number.isFinite(width)||!Number.isFinite(height)||width<=0||height<=0||!color||color.length!==4||color.some(x=>!Number.isFinite(x)))throw Error('invalid typed geometry/material');
 const dx=80,dy=30,x=s.transform[4],y=s.transform[5],point=[x+width+40,y+dy+(kind==='table'?typed.rows[0].size:height/2)];
 if(point[0]<=s.bounds[2]+7||point[0]>=x+width+dx-7||point[1]<=y+dy+7||point[1]>=y+dy+height-7)throw Error('witness intersects old footprint or HUD');
 return{kind,primary:s.primary,generation:s.generation,width,height,initialTransform:s.transform,oldBounds:s.bounds,
  delta:[dx,dy],point,color:color.map(x=>Math.round(x*255)),rect:{x:Math.round(point[0]*2)-2,y:Math.round(point[1]*2)-(kind==='table'?0:2),width:4,height:kind==='table'?1:4},
  resourceId:typed.content?.resourceId,resourceHash};
}
export function qualifyWitness(plan,rgba,delta){
 if(!rgba?.length||rgba.length%4||delta?.length!==2||delta.some((x,i)=>!Number.isFinite(x)||Math.abs(x-plan.delta[i])>.1))return{status:'FAILED',positive:0};
 let positive=0;for(let i=0;i<rgba.length;i+=4)if(plan.color.every((v,c)=>Math.abs(rgba[i+c]-v)<=2))++positive;
 return{status:positive?'QUALIFIED':'FAILED',positive};
}
export function reconcilePhases({root,children}){
 if(!Number.isFinite(root)||root<0||children.some(c=>!Number.isFinite(c.inclusive)||!Number.isFinite(c.exclusive)||c.inclusive<0||c.exclusive<0||c.exclusive>c.inclusive))throw Error('invalid phases');
 const sum=children.reduce((v,x)=>v+x.inclusive,0);if(sum>root+1000)throw Error('phase children exceed root');
 // ns callers tolerate only timer quantization. Unit tests use exact values.
 if(root<1000&&sum>root)throw Error('phase children exceed root');
 return{root,childInclusive:sum,residual:Math.max(0,root-sum)};
}
export function traceMeasuredPointer(phase,execute,trace,on=true){
 if(!trace||phase!=='up')return{value:execute(),pages:[]};
 trace.enable(on);try{return{value:execute(),pages:[]};}finally{trace.enable(false);}
}
