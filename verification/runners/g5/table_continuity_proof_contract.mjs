// Verification-only bounded evidence, saved before caller assertions/cleanup.
export async function probeUntilContent({basis,probe,persist,pause,now,guardMs=15000}){
 const result={basis,status:'PENDING',started:now(),first:null,last:null,recent:[],count:0};
 for(;;){try{const value=await probe(),sample={...value,at:now()};++result.count;result.first??=sample;result.last=sample;result.recent.push(sample);if(result.recent.length>8)result.recent.shift();if(sample.positive){result.status='QUALIFIED';result.firstCorrectAt=sample.at;}else if(sample.at-result.started>=guardMs)result.status='MISSING';}catch(error){result.status='ERROR';result.error=String(error.stack??error);result.errorAt=now();}
  await persist(result);if(result.status!=='PENDING')return result;await pause(16);
 }
}
export function compareExpectedROI(actual,reference,{requiredPixels,maximumDelta=2}){
 if(!actual||actual.length!==reference.length||!requiredPixels?.length)throw Error('invalid expected ROI');
 let matches=0;for(const p of requiredPixels){let good=true;for(let c=0;c<4;++c)good&&=Math.abs(actual[p*4+c]-reference[p*4+c])<=maximumDelta;if(good)++matches;}
 return{positive:matches===requiredPixels.length,matches,required:requiredPixels.length};
}
