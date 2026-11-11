import {typedWitness as existingWitness,qualifyWitness} from './web_up_attribution_contract.mjs';
export {qualifyWitness};
export const WINDOWS=Object.freeze({'G5QW1-BASELINE-DIAGNOSTIC':240,'G5QW1-FINAL-PAIRS':720,'G5QW1-CONTINUOUS':360,'G5QW1-FINAL-DIAGNOSTIC':240});
export function typedWitness(kind,selection,typed,resourceHash){
 if(kind!=='rectangle')return existingWitness(kind,selection,typed,resourceHash);
 // The same rectangular analytic witness policy, actual public dimensions,
 // explicit canonical material. No shape-only dimensions for sticky/table.
 const result=existingWitness('sticky',selection,typed,resourceHash);
 return {...result,kind:'rectangle'};
}
export function navigationWitness(kind,camera){
 if(!['pan','zoom'].includes(kind)||!camera||camera.scale!==1||camera.x!==0||camera.y!==0)throw Error('unsupported navigation witness basis');
 const expected=kind==='pan'?{scale:1,x:-24,y:-12}:{scale:Math.exp(.24),x:600-Math.exp(.24)*600,y:400-Math.exp(.24)*400};
 // Known analytic preseed: world rectangle850,500..970,580.
 // Pan enters its left strip; zoom moves its right edge past the old footprint.
 const point=kind==='pan'?[835,540]:[1060,570];
 const old=[850,500,970,580],next=[850*expected.scale+expected.x,500*expected.scale+expected.y,970*expected.scale+expected.x,580*expected.scale+expected.y];
 if(point[0]>=old[0]-7&&point[0]<=old[2]+7&&point[1]>=old[1]-7&&point[1]<=old[3]+7)throw Error('navigation point intersects old footprint');
 if(point[0]<=next[0]+7||point[0]>=next[2]-7||point[1]<=next[1]+7||point[1]>=next[3]-7)throw Error('navigation point outside new interior');
 return{kind,expected,point,old,next,color:[51,128,204,255],rect:{x:Math.round(point[0]*2)-2,y:Math.round(point[1]*2)-2,width:4,height:4}};
}
export function plannedCases(windowId,baseline,candidate){
 if(['G5QW1-FINAL-PAIRS','G5QW2-FINAL-PAIRS'].includes(windowId))return[0,1,2].flatMap(pair=>[
  ...['old','candidate'].flatMap(variant=>['long','growth'].map(mode=>({id:`pair${pair}-${variant}-${mode}`,pair,variant,sdk:variant==='old'?baseline:candidate,mode,moves:mode==='long'?1600:160})))
 ]);
 if(['G5QW1-CONTINUOUS','G5QW2-CONTINUOUS'].includes(windowId))return[{id:'stress10',mode:'growth',moves:160,roundCount:10,sdk:candidate},
  ...['ready','erase','pan','zoom','rectangle','sticky','table','image'].map(kind=>({id:'action-'+kind,mode:['rectangle','sticky','table','image'].includes(kind)?'witness':'action',kind,sdk:candidate}))];
 throw Error('diagnostic roster requires explicit scope');
}
