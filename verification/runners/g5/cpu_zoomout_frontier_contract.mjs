// Verification-only fixed input. No product geometry or state owner.
export const rowIds=Object.freeze(['append','partial-erase'].flatMap(edit=>['zoom-in','zoom-out','pan'].flatMap(direction=>['immediate','settled'].map(timing=>`${edit}-${direction}-${timing}`))));
export function rowSpec(id){
  if(!rowIds.includes(id))throw Error('unfrozen edit/navigation row');
  const edit=id.startsWith('partial-erase')?'partial-erase':'append';
  const timing=id.endsWith('immediate')?'immediate':'settled';
  const direction=id.includes('zoom-in')?'zoom-in':id.includes('zoom-out')?'zoom-out':'pan';
  return {id,edit,direction,timing};
}
export function confirmedTrace(edit){
  const count=edit==='append'?160:24;
  return Array.from({length:count+2},(_,at)=>{
    const i=Math.min(at,count);
    return {phase:at===0?'down':at===count+1?'up':'move',x:at===count+1?999:Math.fround(420+7*Math.sin(i*.17)),y:at===count+1?999:Math.fround(160+170*i/count),timestampNs:90_000_000_000+(at+1)*8_000_000};
  });
}
export function navigationEvents(direction){
  const delta=direction==='pan'?{ctrlKey:false,deltaX:-10,deltaY:-5}:{ctrlKey:true,deltaX:0,deltaY:-Math.log(direction==='zoom-in'?1.15:.85)/.01/6};
  return Array.from({length:6},(_,index)=>({index,plannedOffsetMs:index*16.7,x:715,y:505,...delta}));
}
export function expectedCamera(camera,direction,anchor={x:715,y:505}){
  if(direction==='pan')return {scale:camera.scale,x:camera.x+60,y:camera.y+30};
  const ratio=direction==='zoom-in'?1.15:.85;
  return {scale:camera.scale*ratio,x:anchor.x+(camera.x-anchor.x)*ratio,y:anchor.y+(camera.y-anchor.y)*ratio};
}
