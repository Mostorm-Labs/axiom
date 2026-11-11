export const SEEDS=Object.freeze([0x13579bdf,0x2468ace1,0x6d2b79f5]);
export function entropyStream(seed){
 let word=seed>>>0;if(!word)throw Error('zero entropy fixture seed');let calls=[],hash=2166136261;
 return{fill(view){if(!ArrayBuffer.isView(view)||view instanceof DataView||view instanceof Float32Array||view instanceof Float64Array)throw Error('integer entropy view required');const bytes=new Uint8Array(view.buffer,view.byteOffset,view.byteLength);calls.push(bytes.length);for(let i=0;i<bytes.length;++i){word^=word<<13;word^=word>>>17;word^=word<<5;const byte=word&255;bytes[i]=byte;hash=Math.imul(hash^byte,16777619)>>>0;}return view;},transcript(){return{seed:seed>>>0,algorithm:'xorshift32-byte-v1-fixture-only',call_lengths:[...calls],byte_count:calls.reduce((a,b)=>a+b,0),stream_fnv1a32:hash>>>0};}};
}
export function installEntropy(seed){
 // Executed only by Playwright addInitScript in this owned localhost test page.
 // No ordinary SDK/user-page modification; lifetime ends with the fresh page.
 const stream=entropyStream(seed),original=crypto.getRandomValues.bind(crypto);
 Object.defineProperty(crypto,'getRandomValues',{configurable:true,value:view=>stream.fill(view)});
 globalThis.exactFixtureEntropy={transcript:()=>stream.transcript(),restore(){Object.defineProperty(crypto,'getRandomValues',{configurable:true,value:original});}};
}
export function emptyViewPoint(camera){
 if(!camera||![camera.scale,camera.x,camera.y].every(Number.isFinite)||camera.scale<=0)throw Error('invalid cleanup camera');
 // The fixture's ink is left of world800, mixed objects start at world850,
 // and their bottom edge is830. A bottom-left in-canvas point stays below
 // all content at both the initial and the frozen zoom camera.
 const view=[24,986],world=[(view[0]-camera.x)/camera.scale,(view[1]-camera.y)/camera.scale];
 if(world[1]<=840||world[0]>=800)throw Error('cleanup point lacks empty fixture proof');
 return{world,view,proof:'inside1431x1010canvas; worldy>840belowmaxobject830; worldx<800outside mixedobjects',content_bottom:830};
}
export function requireCanonicalPair(a,b){
 if(!a||!b||!a.objects_equal||!a.manifest_background_equal||!a.document_digest_equal||!b.objects_equal||!b.manifest_background_equal||!b.document_digest_equal)throw Error('exact canonical pilot mismatch');return true;
}
