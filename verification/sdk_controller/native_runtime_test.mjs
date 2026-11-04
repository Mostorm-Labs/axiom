import assert from 'node:assert/strict';
import {spawn} from 'node:child_process';
import {createInterface} from 'node:readline';
import {createTransportController} from '../../sdk/api/internal/transport.mjs';
const child=spawn(process.argv[2],[],{stdio:['pipe','pipe','inherit']});
const lines=createInterface({input:child.stdout});const waiting=[];
lines.on('line',line=>waiting.shift()?.(JSON.parse(line)));
const raw=(command,args={},bytes)=>new Promise(resolve=>{waiting.push(r=>{if(r.bytes)r.bytes=new Uint8Array(r.bytes);resolve(r);});child.stdin.write(JSON.stringify({request:{command,...args},bytes:bytes?Array.from(bytes):[]})+'\n');});
const request=async(...args)=>{const r=await raw(...args);assert(r.ok,JSON.stringify(r));return r;};
try{
 await request('open',{documentId:'a'.repeat(32),builtinFonts:true});
 const c=createTransportController({owner:'real-session',version:'0.3.0',request:raw,subscribe(){return ()=>{};},close:()=>request('close')});
 assert.equal((await c.readState()).documentId,'a'.repeat(32));
 await c.selectBrush('crayon-wax-v1',3);assert.equal((await c.readBrushConfig()).family,4);
 await c.selectTool('partialEraser');assert.equal((await c.readState()).eraserMode,2);
 await c.selectShape('rectangle');
 for(const [phase,x] of [[0,40],[1,100],[2,120]])await request('pointer',{source:1,pointer:1,timestampNs:(phase+1)*10000000,phase,x,y:80+phase*20,pressure:.5,pressureAvailable:false});
 assert.equal((await c.readState()).objects,1);await c.undo();assert.equal((await c.readState()).objects,0);await c.redo();assert.equal((await c.readState()).objects,1);
 const state=await c.readState();const snapshot=await c.captureSnapshot({generation:state.generation,documentRevision:'9007199254740993',recoveryFrontier:[8,1,16,1,24,1]});assert(snapshot.bytes.byteLength>0);
 const table=await c.table.create({rows:2,columns:2,x:200,y:200});assert((await c.table.read(table.objectId)).available);await c.undo();
 await c.text.create(180,100,200);await c.text.replace('Hello');assert.equal((await c.text.read()).projection.length,5);await c.text.finish();
 const sticky=await c.createSticky({size:80,x:260,y:200});assert.match(sticky.objectId,/^[0-9a-f]{32}$/);
 const connector=await c.createConnector({start:{point:[20,350]},end:{point:[200,380]},routing:1});assert.match(connector.objectId,/^[0-9a-f]{32}$/);assert.notEqual(sticky.objectId,connector.objectId);
 await c.close();console.log('Native transport → real Session/Runtime: tools, brush, pointer, History, snapshot, text PASS');
}finally{child.stdin.end();await new Promise(r=>child.once('exit',r));}
