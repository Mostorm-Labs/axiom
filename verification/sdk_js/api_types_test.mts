import {createRuntime,AxiomError,type Color} from '../../sdk/web/axiom.mjs';
const sdk=await createRuntime({mount:document.createElement('div'),permission:'localSingleUser',onOperation:bytes=>{bytes.slice();}});
const imageExportVersion:1=sdk.readBuildInfo().features.imageExport;
void imageExportVersion;
await sdk.selectTool('objectEraser');
await sdk.selectShape('triangle');
const projection=sdk.multi.read(), property=projection.properties['3'];
await sdk.multi.setProperty(3,{type:'float',value:.5},projection.stamp,property.eligible);
const color:Color=[0,0,1,1];await sdk.text.format({color,bold:true});
await sdk.setBackground({mode:'dots',color:[1,1,1,1],patternColor:[0,0,0,.5]});
const exported=await sdk.exportImage({format:'jpg',scale:2,background:[1,1,1,1],generation:sdk.readState().generation});
exported.bytes.slice();exported.mimeType;exported.bounds[3];exported.generation;
// @ts-expect-error formats are closed
await sdk.exportImage({format:'webp'});
// @ts-expect-error generations retain exact decimal representation
await sdk.exportImage({generation:123});
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

const tableCreated=await sdk.table.create({rows:3,columns:3});
const table=sdk.table.read(tableCreated.objectId);
if(table.available){
 const cell={objectId:table.objectId,rowId:table.rows[0].id,columnId:table.columns[0].id,generation:table.generation};
 await sdk.table.edit(cell);await sdk.text.replace('cell');await sdk.text.finish();
 await sdk.table.setCellStyle({...cell,fill:[1,1,1,1],format:{bold:true,alignment:2}});
 await sdk.table.setColumnWidth({objectId:table.objectId,columnId:cell.columnId,size:160});
}
await sdk.table.navigate(1);
// @ts-expect-error Runtime owns row and column identity allocation
await sdk.table.create({rowIds:['internal']});
// @ts-expect-error no internal canonical TableContent write API
await sdk.table.replaceContent({});
// @ts-expect-error hand-drawn grid styles require a later versioned capability
await sdk.table.setGridStyle({objectId:'id',roughness:1});
