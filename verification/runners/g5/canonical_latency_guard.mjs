// A known-baseline regression guard, never an image correctness exemption.
import crypto from'node:crypto';
const sha=b=>crypto.createHash('sha256').update(b).digest('hex');
export function freezeKnownResidual(baseline,expected,edge,{count=302,basis={}}={}){
 if(baseline.length!==expected.length||edge.length*4!==expected.length)throw Error('shape');
 const pixels=[];for(let i=0;i<edge.length;++i){if(edge[i])continue;let d=0;for(let c=0;c<4;++c)d=Math.max(d,Math.abs(baseline[i*4+c]-expected[i*4+c]));if(d>2)pixels.push({pixel:i,value:Array.from(baseline.subarray(i*4,i*4+4))});}
 if(pixels.length!==count)throw Error('unexpected old residual set');return{basis,pixels,expected_sha256:sha(expected),edge_sha256:sha(edge),baseline_sha256:sha(baseline),scope:'knownoldresidualregressiononly;imagequalificationopen'};
}
export function regressionGuard(actual,baseline,expected,edge,known,basis){
 if(actual.length!==baseline.length||actual.length!==expected.length||edge.length*4!==actual.length)throw Error('shape');
 if(JSON.stringify(basis)!==JSON.stringify(known.basis)||sha(expected)!==known.expected_sha256||sha(edge)!==known.edge_sha256||sha(baseline)!==known.baseline_sha256)throw Error('foreign basis');
 const old=new Map(known.pixels.map(v=>[v.pixel,v.value]));let newResidual=0,knownValue=0,baselineRegression=0,strict=0;const witnesses=[];
 for(let i=0;i<edge.length;++i){if(edge[i])continue;++strict;let cpu=0,gpu=0;for(let c=0;c<4;++c){cpu=Math.max(cpu,Math.abs(actual[i*4+c]-expected[i*4+c]));gpu=Math.max(gpu,Math.abs(actual[i*4+c]-baseline[i*4+c]));}
  if(old.has(i)){if(gpu>2)++knownValue;}else if(cpu>2)++newResidual;
  if(gpu>2)++baselineRegression;
  if((old.has(i)?gpu>2:cpu>2)&&witnesses.length<8)witnesses.push({pixel:i,actual:Array.from(actual.subarray(i*4,i*4+4)),baseline:Array.from(baseline.subarray(i*4,i*4+4)),CPU:Array.from(expected.subarray(i*4,i*4+4)),oldKnown:old.has(i)});
 }
 return{status:!newResidual&&!knownValue&&!baselineRegression?'SCOPED_NO_NEW_REGRESSION':'FAILED',new_residual_pixels:newResidual,changed_known_pixels:knownValue,baseline_exterior_regression_pixels:baselineRegression,strict_compared_pixels:strict,known_image_pixels:old.size,image_correctness:'OPEN;oldCPUfailureunchanged',witnesses};
}
