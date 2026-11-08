// Cold correctness helpers. Browser-safe exports; Node hashing is imported only
// by the host copy operation. No SDK or primary sampling dependency.
const UNIT=32768,UNITS=8,BATCH=UNIT*UNITS;
const integer=x=>Number.isSafeInteger(x)&&x>=0;
const hash=x=>typeof x==='string'&&/^[0-9a-f]{64}$/.test(x);
const validBytes=(part,offset,n)=>part?.offset===offset&&Array.isArray(part.bytes)&&part.bytes.length===n&&part.bytes.every(x=>Number.isInteger(x)&&x>=0&&x<=255);
export function encodeChunkBatch({length,offset,pull}){
 if(!integer(length)||!integer(offset)||offset>=length)throw Error('invalid or exhausted batch offset');
 const start=offset,units=[];
 while(offset<length&&units.length<UNITS){
  const n=Math.min(UNIT,length-offset),part=pull(offset,n);if(!validBytes(part,offset,n))throw Error('invalid source chunk');
  const base64=btoa(String.fromCharCode(...part.bytes));units.push({offset,byte_length:n,base64});offset+=n;
 }
 return{offset:start,next_offset:offset,decoded_bytes:offset-start,units};
}
export async function copyCorrectnessBytes({length,source_sha256,pullBatch,write,finish,cancel}){
 let done=false;
 try{
  if(!integer(length)||!hash(source_sha256))throw Error('invalid length/source SHA256');
  const {createHash}=await import('node:crypto'),digest=createHash('sha256');let offset=0;
  while(offset<length){
   const batch=await pullBatch(offset);
   if(batch?.offset!==offset||!integer(batch.decoded_bytes)||batch.decoded_bytes<1||batch.decoded_bytes>BATCH||!Array.isArray(batch.units)||!batch.units.length||batch.units.length>UNITS)throw Error('invalid bounded batch');
   const start=offset;
   for(const part of batch.units){
    const n=Math.min(UNIT,length-offset);
    if(n<1||part?.offset!==offset||part.byte_length!==n||typeof part.base64!=='string'||part.base64.length!==4*Math.ceil(n/3)||part.base64.length>43692||!/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(part.base64))throw Error('invalid unit offset/length/encoding');
    const binary=atob(part.base64);if(binary.length!==n||btoa(binary)!==part.base64)throw Error('noncanonical or incomplete encoding');
    const bytes=Uint8Array.from(binary,c=>c.charCodeAt(0));await write(bytes);digest.update(bytes);offset+=n;
   }
   if(offset-start!==batch.decoded_bytes||batch.next_offset!==offset)throw Error('batch byte count/offset differs');
  }
  const received_sha256=digest.digest('hex');if(received_sha256!==source_sha256)throw Error('source/received SHA256 differs');
  await finish();done=true;return{byte_length:offset,source_sha256,received_sha256,unit_bytes_max:UNIT,batch_units_max:UNITS,batch_decoded_bytes_max:BATCH};
 }catch(error){
  if(!done){try{await cancel();}catch(cleanup){throw new AggregateError([error,cleanup],'transfer and cancellation failed');}}
  throw error;
 }
}
export function createStageRecorder({emit,clock,domain}){
 let previous=-1n,sequence=0,lastStarted=null;
 const record=(stage,event,extra={})=>{const monotonic_ns=clock();if(!/^(0|[1-9][0-9]*)$/.test(monotonic_ns)||BigInt(monotonic_ns)<previous)throw Error('stage clock is not monotonic');previous=BigInt(monotonic_ns);emit({stage,event,domain,sequence:++sequence,monotonic_ns,...extra});};
 return{get lastStarted(){return lastStarted;},async run(name,fn){lastStarted=name;record(name,'begin');try{const value=await fn();record(name,'end',{status:'completed'});return value;}catch(error){record(name,'end',{status:'failed',error:String(error)});throw error;}}};
}
