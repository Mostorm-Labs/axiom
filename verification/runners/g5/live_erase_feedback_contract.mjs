export function mapWorld(camera,[x,y]){if(!camera||![camera.scale,camera.x,camera.y,x,y].every(Number.isFinite)||camera.scale<=0)throw Error('camera');return[x*camera.scale+camera.x,y*camera.scale+camera.y];}
export function erasureWaveforms(){return[0,5,-5].map(offset=>Array.from({length:161},(_,i)=>[420+offset+7*Math.sin(i*.17),160+170*i/160]));}
export function pendingFacts(state){return{generation:state.generation,presentedGeneration:state.lastCommit.presentedGeneration,contacts:state.activeContacts,pending:state.pending?.intents??[],overlapsPending:Boolean(state.pending?.intents?.some(v=>v.kind==='erase'))};}
export function rejected(result){return result?.ok===false;}
