import fs from 'node:fs/promises';import {createWriteStream} from 'node:fs';
import path from 'node:path';import {createGzip} from 'node:zlib';import {createHash} from 'node:crypto';import {once} from 'node:events';
import {validateChunk} from './ur2d_selection_up_contract.mjs';
import {assertRecordBatch} from './heavy_scene_attribution_contract.mjs';
export class HeavyStore{
 constructor(directory){this.directory=directory;this.transfers=new Map();this.recordSequence=0;this.recordCount=0;this.recordBytes=0;this.recordSink=null;this.files={};}
 async initialize(){await fs.mkdir(this.directory,{recursive:true});}
 sink(name){const gzip=createGzip({level:5}),file=createWriteStream(path.join(this.directory,name));let failure=null;gzip.on('error',e=>failure=e);file.on('error',e=>{failure=e;gzip.destroy(e);});gzip.pipe(file);return {gzip,file,error:()=>failure};}
 async write(sink,bytes){if(sink.error())throw sink.error();if(!sink.gzip.write(bytes))await once(sink.gzip,'drain');if(sink.error())throw sink.error();}
 async records(value){const next=assertRecordBatch(value,this.recordSequence);if(!this.recordSink)this.recordSink=this.sink('records.jsonl.gz');
  for(const record of value.records){const bytes=Buffer.from(JSON.stringify(record)+'\n');await this.write(this.recordSink,bytes);this.recordBytes+=bytes.length;}
  this.recordSequence=next.nextSequence;this.recordCount+=next.records;return {nextSequence:this.recordSequence};
 }
 async chunk(c){const old=this.transfers.get(c?.name);if(old?.complete)throw Error('duplicate completed buffer');
  const next=validateChunk(c,old);let sink=old?.sink,hash=old?.hash;
  if(!sink){sink=this.sink(c.name+'.gz');hash=createHash('sha256');}
  const bytes=Buffer.from(c.base64,'base64');await this.write(sink,bytes);hash.update(bytes);
  this.transfers.set(c.name,{...next,sink,hash});
  if(next.complete){const done=once(sink.file,'finish');sink.gzip.end();await done;if(sink.error())throw sink.error();this.files[c.name]={path:c.name+'.gz',bytes:next.total,sha256:hash.digest('hex'),compressed_bytes:(await fs.stat(path.join(this.directory,c.name+'.gz'))).size};}
  return {nextOffset:next.nextOffset};
 }
 async metadata(value){const text=JSON.stringify(value,null,2)+'\n';if(Buffer.byteLength(text)>16*1024*1024)throw Error('row metadata bound16MiB');await fs.writeFile(path.join(this.directory,'raw.json'),text);return {saved:true,bytes:Buffer.byteLength(text),sha256:createHash('sha256').update(text).digest('hex')};}
 async finish(required=[]){for(const name of required)if(!this.files[name])throw Error('missing required buffer '+name);
  for(const [name,value]of this.transfers)if(!value.complete)throw Error('unfinished buffer '+name);
  if(this.recordSink){const done=once(this.recordSink.file,'finish');this.recordSink.gzip.end();await done;if(this.recordSink.error())throw this.recordSink.error();}
  const index={files:this.files,records:{path:'records.jsonl.gz',count:this.recordCount,uncompressed_bytes:this.recordBytes,batches:this.recordSequence}};
  await fs.writeFile(path.join(this.directory,'binary-index.json'),JSON.stringify(index,null,2)+'\n');return index;
 }
}
