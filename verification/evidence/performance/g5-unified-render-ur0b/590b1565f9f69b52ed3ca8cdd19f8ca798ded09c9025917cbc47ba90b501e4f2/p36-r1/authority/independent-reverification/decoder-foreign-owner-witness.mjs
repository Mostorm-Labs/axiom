// Review-only witness. Run from the controlled repository root.
// Captures current behavior; it is not a passing correctness oracle.
import {pathToFileURL} from 'node:url';
import path from 'node:path';
const {decodeTrace}=await import(pathToFileURL(path.resolve('verification/runners/g5/ur0/decode_trace.mjs')));
let seq=0;
const r=(kind,tag,values,job='10')=>({kind,tag,values:values.map(String),job,basis:job,attempt:job,event:'1',purpose:2,route:1,at_ns:String(++seq*100),seq:String(seq)});
const rows=[
 r(5,1,[11,1,2,0,0,4,7,1]),
 r(6,27,[0,0,1,0,8,9,4,6]),
 r(5,1,[99,1,2,0,0,4,7,1],'99'),
 r(6,27,[0,0,2,0,8,9,4,6],'99'),
 r(7,9,[9,9,44],'10'),
 r(5,26,[1,23,9,44,4,2,3,8,67108864,201326592],'20'),
 r(11,19,[1,23,9,44,2,3,0],'20'),
 r(5,26,[10,11,1,2,0,0,4,6,7,44,4]),
 r(11,29,[10,1,23,9,9,2,3]),
 r(12,29,[1,10,11,1,2,1,23,9,9,2,3])
];
const d=decodeTrace(rows,{critical:0,statistical:0});
console.log(JSON.stringify({errors:d.errors,sceneRuntime:d.scenes[0]?.runtime,qualified:d.qualified_completions.map(x=>({runtime:x.runtime,operation:x.operation})),expected:'No qualified completion from this mixed-owner chain; reject or mark missing.'},null,2));
