import {createRuntime,AxiomError,type Color} from '../../sdk/web/axiom.mjs';
const sdk=await createRuntime({mount:document.createElement('div'),permission:'localSingleUser',onOperation:bytes=>{bytes.slice();}});
await sdk.selectTool('objectEraser');
await sdk.selectShape('triangle');
const projection=sdk.multi.read(), property=projection.properties['3'];
await sdk.multi.setProperty(3,{type:'float',value:.5},projection.stamp,property.eligible);
const color:Color=[0,0,1,1];await sdk.text.format({color,bold:true});
await sdk.createConnector({start:{point:[1,2]},end:{objectId:sdk.readState().primary,port:4}});
const checkpoint=await sdk.captureSnapshot({generation:sdk.readState().generation,documentRevision:'9007199254740993',recoveryFrontier:[8,1,16,1,24,1]});
checkpoint.bytes.slice();checkpoint.checkpointDigest.slice();
// @ts-expect-error unsupported tools do not widen to arbitrary strings
await sdk.selectTool('brushX');
// @ts-expect-error revisions must retain exact decimal representation
await sdk.captureSnapshot({generation:1,documentRevision:'1',recoveryFrontier:[]});
// @ts-expect-error supported port identity is explicit
await sdk.createConnector({start:{objectId:'a',port:99},end:{point:[0,0]}});
const e=new AxiomError('STALE_TARGET');e.details;
await sdk.close();
