import fs from 'node:fs/promises';
import path from 'node:path';
import {createHash} from 'node:crypto';
import {gzipSync} from 'node:zlib';
const hash=bytes=>createHash('sha256').update(bytes).digest('hex');
export class EvidenceStore {
 constructor(directory,expected){this.directory=directory;this.expected=[...expected];this.entries=[];this.writing=false;}
 async save(row){
  const key=row.q.lane+'-'+row.q.action;
  if(!this.expected.includes(key))throw Error('unknown row '+key);
  if(this.writing||this.entries.some(value=>value.key===key))throw Error('duplicate or overlapping row '+key);
  this.writing=true;
  try{
   const rowDirectory=path.join(this.directory,key);await fs.mkdir(rowDirectory,{recursive:true});
   const buffer=async(name,base64)=>{
    const bytes=Buffer.from(base64,'base64'),compressed=gzipSync(bytes,{level:5});
    const file=path.join(rowDirectory,name+'.gz'),temporary=file+'.tmp';
    await fs.writeFile(temporary,compressed);await fs.rename(temporary,file);
    return {path:path.relative(this.directory,file),bytes:bytes.length,sha256:hash(bytes),compressed_bytes:compressed.length,compressed_sha256:hash(compressed),encoding:'gzip'};
   };
   for(let i=0;i<(row.pixels??[]).length;++i){
    const pixel=row.pixels[i];
    if(pixel.base64){pixel.buffer=await buffer('pixel-'+i+'-canonical.rgba',pixel.base64);delete pixel.base64;}
    if(pixel.preview?.base64){pixel.preview.buffer=await buffer('pixel-'+i+'-preview.rgba',pixel.preview.base64);delete pixel.preview.base64;}
   }
   for(const [name,value]of Object.entries(row.snapshots??{}))if(value.base64){value.buffer=await buffer('snapshot-'+name+'.axsnap',value.base64);delete value.base64;}
   for(let i=0;i<(row.operations??[]).length;++i){const value=row.operations[i];if(value.base64){value.buffer=await buffer('operation-'+i+'.bin',value.base64);delete value.base64;}}
   // The fixture is already hash-bound in the request; no repeated base64 copy.
   if(row.q.fixture?.base64){row.q.fixture={metadata:row.q.fixture.metadata,bytes:Buffer.byteLength(row.q.fixture.base64,'base64'),sha256:hash(Buffer.from(row.q.fixture.base64,'base64'))};}
   const metadata=JSON.stringify(row,null,2)+'\n';if(Buffer.byteLength(metadata)>8*1024*1024)throw Error('metadata quota');
   const file=path.join(rowDirectory,'metadata.json');await fs.writeFile(file+'.tmp',metadata);await fs.rename(file+'.tmp',file);
   const entry={key,path:path.relative(this.directory,file),sha256:hash(metadata),bytes:Buffer.byteLength(metadata),status:row.status};
   const next=[...this.entries,entry];await this.writeIndex(next);this.entries=next;
   return {saved:true,...entry,count:this.entries.length};
  }finally{this.writing=false;}
 }
 async writeIndex(rows){
  const index={schema:'axiom-pending-navigation-per-row-index-v1',expected:this.expected,rows,
   complete:rows.length===this.expected.length&&this.expected.every(key=>rows.some(value=>value.key===key&&value.status==='OBSERVED'))};
  const text=JSON.stringify(index,null,2)+'\n';if(text.length>65536)throw Error('index quota');
  await fs.mkdir(this.directory,{recursive:true});const file=path.join(this.directory,'index.json');await fs.writeFile(file+'.tmp',text);await fs.rename(file+'.tmp',file);return index;
 }
 async index(){return {schema:'axiom-pending-navigation-per-row-index-v1',expected:this.expected,rows:this.entries,
  complete:this.entries.length===this.expected.length&&this.expected.every(key=>this.entries.some(value=>value.key===key&&value.status==='OBSERVED'))};}
}
