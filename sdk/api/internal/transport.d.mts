import type {Controller} from '../axiom.mjs';
export interface TransportReply {ok:boolean;code?:string;message?:string;value?:unknown;bytes?:Uint8Array;[key:string]:unknown;}
export interface ControllerTransport {
 readonly owner:string; readonly version:'0.3.12';
 request(command:string,args?:unknown,bytes?:Uint8Array):Promise<TransportReply>;
 subscribe(kind:'state'|'operation',listener:(event:never)=>void):()=>void;
 dispatchEvents?():Promise<void>;
 close():Promise<void>;
}
export function createTransportController(transport:ControllerTransport):Controller;
