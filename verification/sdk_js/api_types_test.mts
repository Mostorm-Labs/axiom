import {createRuntime,AxiomError,type Color} from '../../sdk/web/axiom.mjs';
const sdk=await createRuntime({mount:document.createElement('div'),permission:'localSingleUser',onOperation:bytes=>{bytes.slice();}});
await sdk.selectTool('objectEraser');
await sdk.selectShape('triangle');
const projection=sdk.multi.read(), property=projection.properties['3'];
await sdk.multi.setProperty(3,{type:'float',value:.5},projection.stamp,property.eligible);
const color:Color=[0,0,1,1];await sdk.text.format({color,bold:true});
await sdk.setBackground({mode:'dots',color:[1,1,1,1],patternColor:[0,0,0,.5]});
const background=sdk.readBackground();background.mode;background.patternColor[3];
// @ts-expect-error background modes are a closed vocabulary
await sdk.setBackground({mode:'checkerboard',color,patternColor:color});
await sdk.createConnector({start:{point:[1,2]},end:{objectId:sdk.readState().primary,port:4}});
const checkpoint=await sdk.captureSnapshot({generation:sdk.readState().generation,documentRevision:'9007199254740993',recoveryFrontier:[8,1,16,1,24,1]});
checkpoint.bytes.slice();checkpoint.checkpointDigest.slice();
checkpoint.throughCommit.ordinal;
sdk.data.attach({capacity:1024,byteCapacity:32*1024*1024});
const stop=sdk.subscribeOperations(packet=>{packet.bytes.slice();packet.commit.ordinal;});
sdk.subscribeState(state=>state.canUndo);
await sdk.dispatchEvents();stop();
const packet=sdk.data.readNext();if(packet){sdk.data.reportStatus(packet,3,1);sdk.data.release(packet);}
await sdk.applyRemoteOperations(packet?[packet]:[]);
await sdk.applyReplayOperations(packet?[packet]:[]);
// @ts-expect-error unsupported tools do not widen to arbitrary strings
await sdk.selectTool('brushX');
// @ts-expect-error revisions must retain exact decimal representation
await sdk.captureSnapshot({generation:1,documentRevision:'1',recoveryFrontier:[]});
// @ts-expect-error supported port identity is explicit
await sdk.createConnector({start:{objectId:'a',port:99},end:{point:[0,0]}});
const e=new AxiomError('STALE_TARGET');e.details;
await sdk.close();
