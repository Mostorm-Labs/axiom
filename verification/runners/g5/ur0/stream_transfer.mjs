// Verification-only cold transport. The original public snapshot is retained
// once; CDP never receives a document-sized numeric JS array.
export class SnapshotTransfer {
 #bytes=null;#offset=0;
 begin(bytes){if(this.#bytes)throw Error('snapshot already active');if(!(bytes instanceof Uint8Array))throw Error('snapshot bytes required');this.#bytes=bytes;this.#offset=0;}
 pull(offset,count){
  if(!this.#bytes)throw Error('no active snapshot');
  if(!Number.isInteger(count)||count<1||count>32768)throw Error('snapshot chunk must be bounded');
  if(offset!==this.#offset)throw Error('snapshot offset is not sequential');
  const end=Math.min(offset+count,this.#bytes.length),bytes=Array.from(this.#bytes.subarray(offset,end));this.#offset=end;
  return{offset,bytes};
 }
 end(cancel=false){if(!this.#bytes)throw Error('no active snapshot');if(!cancel&&this.#offset!==this.#bytes.length)throw Error('snapshot transfer incomplete');this.#bytes=null;this.#offset=0;}
}
export async function copySnapshot({length,pull,write}){
 if(!Number.isSafeInteger(length)||length<0)throw Error('invalid snapshot length');
 for(let offset=0;offset<length;){const n=Math.min(32768,length-offset),part=await pull(offset,n);
  if(part.offset!==offset||!Array.isArray(part.bytes)||part.bytes.length!==n||part.bytes.some(x=>!Number.isInteger(x)||x<0||x>255))throw Error('invalid or incomplete snapshot chunk');
  await write(Uint8Array.from(part.bytes));offset+=n;
 }
}
