// Qualification host only: delay one *real* Worker terminal delivery. The
// kernel, original MessageEvent, callback, receiver, identity and bytes stay real.
export class TerminalDeliveryGate {
 constructor(hash=async bytes=>Array.from(new Uint8Array(await crypto.subtle.digest('SHA-256',bytes)),x=>x.toString(16).padStart(2,'0')).join('')){
  this.hash=hash;this.record=null;this.passthrough=false;this.deliveries=0;this.events=[];
  this.waiting=new Promise(resolve=>this.resolve=resolve);
 }
 deliver(event,callback,receiver){
  if(this.passthrough||event.data?.type!=='terminal'){callback.call(receiver,event);return;}
  if(this.record)throw Error('duplicate held terminal');
  const value=event.data;if(!(value.bytes instanceof ArrayBuffer))throw Error('real terminal has no buffer');
  this.record={event,callback,receiver,key:value.key,status:value.status,bytes:value.bytes,byteLength:value.bytes.byteLength};
  this.digest=this.hash(new Uint8Array(value.bytes)).then(sha256=>{
   this.record.sha256=sha256;const fact={key:value.key,status:value.status,bytes:this.record.byteLength,sha256,at:performance.now(),type:'held-real-terminal'};
   this.events.push(fact);this.resolve(fact);return sha256;
  });
 }
 held(){return this.waiting;}
 async release(){
  if(this.passthrough)return false;if(!this.record)return false;
  const expected=await this.digest,record=this.record;
  const actual=await this.hash(new Uint8Array(record.bytes));
  if(actual!==expected||record.event.data.key!==record.key||record.event.data.status!==record.status||record.bytes.byteLength!==record.byteLength)throw Error('terminal custody changed');
  this.passthrough=true;++this.deliveries;
  this.events.push({type:'released-real-terminal',key:record.key,status:record.status,bytes:record.byteLength,sha256:actual,at:performance.now(),deliveries:this.deliveries});
  record.callback.call(record.receiver,record.event);return true;
 }
}
export function installHeldWorkerGate(){
 const Original=window.Worker,gate=new TerminalDeliveryGate();
 class HeldWorker extends Original {
  set onmessage(callback){this.qualificationCallback=callback;super.onmessage=event=>gate.deliver(event,callback,this);}
  get onmessage(){return this.qualificationCallback;}
 }
 window.Worker=HeldWorker;
 return {gate,restore(){if(window.Worker!==HeldWorker)throw Error('foreign Worker wrapper');window.Worker=Original;return true;}};
}
