// Frozen post-wheel boundary: setup/preparation observations are never timing.
export function startupMetrics(phase){
 const first=phase.wheels?.[0]?.started;if(!Number.isFinite(first))throw Error('missingfirstwheel');
 const content=phase.colorOps.filter(x=>x.kind==='content'&&x.started>=first);
 const at=content[0]?.started;
 const callbacks=phase.rafCallbacks.filter(x=>x.source==='sdk'&&x.started>=first);
 return{firstContentMs:at===undefined?null:at-first,missedOpportunities:callbacks.filter(x=>at!==undefined&&x.started<at&&!content.some(w=>w.rafId===x.id)).length,maxRAFMs:Math.max(0,...callbacks.map(x=>x.ended-x.started)),maxInputLatenessMs:Math.max(0,...phase.wheels.map(x=>x.started-x.planned)),notPhysicalPhoton:true};
}
export function startupGate(baseline,candidate){
 if(!baseline.length||baseline.length!==candidate.length)return{qualified:false,reason:'missingpair'};
 const median=a=>{const v=[...a].sort((x,y)=>x-y);return(v[Math.floor((v.length-1)/2)]+v[Math.ceil((v.length-1)/2)])/2;};
 if([...baseline,...candidate].some(x=>!Number.isFinite(x.firstContentMs)))return{qualified:false,reason:'missingfeedback'};
 const reducedWait=median(candidate.map(x=>x.missedOpportunities))<=median(baseline.map(x=>x.missedOpportunities))-1||median(candidate.map(x=>x.firstContentMs))<=.8*median(baseline.map(x=>x.firstContentMs));
 return{startupImproved:reducedWait,qualified:reducedWait,requiresIndependentControlGapAndPixelChecks:true};
}
