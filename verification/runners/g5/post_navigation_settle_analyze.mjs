import fs from'node:fs/promises';import{routeMetrics}from'./continuous_zoom_quality_contract.mjs';
export function absoluteCamera(phase){
 const all=(phase.presents??[]).filter(x=>x.role!=='preview'&&x.at>=phase.started),stamps=all.filter(x=>x.at<=phase.inputEnded),times=[phase.started,...stamps.map(x=>x.at),phase.inputEnded].sort((a,b)=>a-b);
 const gaps=times.slice(1).map((t,i)=>t-times[i]);
 const current=(s,c)=>Math.abs(s.affine[0]-c.scale)<1e-6&&Math.abs(s.affine[4]-c.x)<1e-3&&Math.abs(s.affine[5]-c.y)<1e-3;
 const lag=[];
 for(const i of phase.inputs){const deadline=i.ended+33.4,eligible=all.filter(s=>s.at>=i.actual&&s.at<=deadline);
  // Latest display must advance to at least one requested input in this
  // opportunity window. Never relabel an old display by reading currentstate.
  const recent=phase.inputs.filter(j=>j.index>=i.index&&j.actual<=deadline);
  if(!eligible.some(s=>recent.some(j=>current(s,j.camera))))lag.push({index:i.index,deadline});
 }
 const boundaries=[60,120,180,240,300].map(index=>{const at=phase.inputs[index-1].actual;let before=phase.started,after=phase.inputEnded;for(const t of times){if(t<=at)before=t;if(t>=at){after=t;break;}}return {index,gap_ms:after-before};});
 const final=phase.finalPresent,finalCurrent=!!final&&current(final,phase.endState.camera);
 return {successfulPresents:stamps.length,max_gap_ms:Math.max(...gaps),boundaries,missed33_4msWindows:lag,finalCurrent,pass:stamps.length>0&&Math.max(...gaps)<=100&&boundaries.every(x=>x.gap_ms<=100)&&lag.length===0&&finalCurrent};
}
if(process.argv[1]===new URL(import.meta.url).pathname){const raw=JSON.parse(await fs.readFile(process.argv[2],'utf8'));const rows=raw.routes.map(r=>({route:r.route,proxy:routeMetrics(r),camera:r.presents?.length?absoluteCamera(r):null}));console.log(JSON.stringify(rows));}
