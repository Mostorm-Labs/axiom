export function correctedCameraWindows(phase){
 const stamps=(phase.presents??[]).filter(s=>s.role!=='preview'),inputs=phase.inputs;
 const matches=(s,c)=>s&&Math.abs(s.affine[0]-c.scale)<1e-6&&Math.abs(s.affine[4]-c.x)<1e-3&&Math.abs(s.affine[5]-c.y)<1e-3;
 const misses=[];
 for(const i of inputs){const future=inputs.filter(j=>j.index>=i.index&&j.actual<=i.ended+33.4);if(!stamps.some(s=>s.at>=i.actual&&s.at<=i.ended+33.4&&future.some(j=>matches(s,j.camera))))misses.push(i.index);}
 return{misses,finalCurrent:matches(phase.finalPresent,phase.endState.camera)};
}
export function pairedStable({DPR,route,B,C}){const median=a=>{const s=[...a].sort((a,b)=>a-b);return(s[Math.floor((s.length-1)/2)]+s[Math.ceil((s.length-1)/2)])/2;};if(B.length!==2||C.length!==2)throw Error('incomplete pair');const absolute={DPR2_wheel:386.8100001096725,DPR2_touch:398.82749989032743,DPR4_wheel:558.2174999177455,DPR4_touch:573.1100001096725}[`DPR${DPR}_${route}`];if(!absolute)throw Error('route');const bm=median(B),cm=median(C),relative=bm*1.15+100;return{Bmedian:bm,Cmedian:cm,absolute,relative,pass:cm<=absolute&&cm<=relative};}
export function admissionAfterBlock(rows,DPR){for(const route of['wheel','touch']){const x=rows.filter(x=>x.DPR===DPR&&x.route===route);const B=x.filter(x=>x.lane==='baseline').map(x=>x.stable),C=x.filter(x=>x.lane==='candidate').map(x=>x.stable);if(B.length===2&&C.length===2&&!pairedStable({DPR,route,B,C}).pass)return false;}return true;}
export function phaseAccounting(row){let sum=0;for(const t of row.times)sum+=Number(t[1]);const elapsed=Number(BigInt(row.end)-BigInt(row.start));if(sum>elapsed+1000)throw Error('nested phase doublecount');return{elapsedNs:elapsed,selfNs:sum,unattributedNs:elapsed-sum};}
