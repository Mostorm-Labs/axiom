// Verification transport only. No document, stroke or viewport logic.
export const CHUNK_BYTES=65536;
export const MAX_BUFFER_BYTES=128*1024*1024;
export const WINDOW_IDS=Object.freeze(['U2D1-OLD-F','U2D1-A-F','U2D1-B-F','U2D1-COMPARE']);
export function validateBufferName(name){
 if(typeof name!=='string'||!(/^[A-Za-z0-9_-]{1,96}\.(rgba|axsnap)$/.test(name)))throw Error('invalid buffer name');
 return name;
}
export async function sendBinaryChunks(name,bytes,persist){
 validateBufferName(name);
 if(!(bytes instanceof Uint8Array)||!bytes.byteLength||bytes.byteLength>MAX_BUFFER_BYTES)throw Error('invalid typed buffer');
 let chunks=0;
 for(let offset=0;offset<bytes.byteLength;offset+=CHUNK_BYTES){
  const slice=bytes.subarray(offset,Math.min(bytes.byteLength,offset+CHUNK_BYTES));let binary='';
  for(let i=0;i<slice.length;i+=8192)binary+=String.fromCharCode(...slice.subarray(i,i+8192));
  const reply=await persist({name,offset,total:bytes.byteLength,base64:btoa(binary)});
  if(!reply||reply.nextOffset!==offset+slice.byteLength)throw Error('binary persistence acknowledgement mismatch');
  ++chunks;
 }
 return {byteLength:bytes.byteLength,chunks};
}
export function validateChunk(chunk,previous){
 validateBufferName(chunk?.name);const {offset,total,base64}=chunk;
 if(!Number.isSafeInteger(offset)||!Number.isSafeInteger(total)||offset<0||total<=0||total>MAX_BUFFER_BYTES||offset>=total)
  throw Error('invalid chunk range');
 if(previous){if(previous.total!==total||previous.nextOffset!==offset)throw Error('out-of-order or foreign transfer');}
 else if(offset!==0)throw Error('missing initial chunk');
 if(typeof base64!=='string'||base64.length>Math.ceil(CHUNK_BYTES/3)*4||
    !/^(?:[A-Za-z0-9+/]{4})*(?:[A-Za-z0-9+/]{2}==|[A-Za-z0-9+/]{3}=)?$/.test(base64))throw Error('invalid base64 chunk');
 const size=base64.length/4*3-(base64.endsWith('==')?2:base64.endsWith('=')?1:0);
 if(size!==Math.min(CHUNK_BYTES,total-offset))throw Error('short or oversized chunk');
 return {total,nextOffset:offset+size,size,complete:offset+size===total};
}
export function classifyWindow(guard,worker,sourceStable){
 if(!guard||guard.reason!=='completed'||guard.returncode!==0||!guard.cleanup_complete||
    guard.remaining_owned_pids?.length||!sourceStable)return 'STOPPED';
 return worker?.status==='OBSERVED'?'COMPLETED':'STOPPED';
}
export function classifyPerformance(pairs){
 if(pairs.length!==3||pairs.some(p=>!p.correct||!Number.isFinite(p.old)||!Number.isFinite(p.candidate)||p.old<=0||p.candidate<0))
  return {status:'MISSING',reason:'requires all three correct finite pairs'};
 const median=values=>[...values].sort((a,b)=>a-b)[1];
 return {status:'OBSERVED',oldMedian:median(pairs.map(p=>p.old)),candidateMedian:median(pairs.map(p=>p.candidate))};
}
