// Private infrastructure binding. Root business DTOs expose no native handle.
const owners=new WeakMap();
export function bindCpuWorkerOwner(runtime,owner){owners.set(runtime,{...owner,generation:0n,executor:null,begin(){if(this.executor)throw Error('WORKER_ALREADY_ACTIVE');this.generation+=1n;if(this.generation>0xffffffffffffffffn)throw Error('EXECUTOR_EXHAUSTED');this.executor=this.generation.toString();return this.executor;},end(id){if(this.executor===id)this.executor=null;}});}
export function cpuWorkerOwner(runtime){const owner=owners.get(runtime);if(!owner)throw Error('FOREIGN_RUNTIME');return owner;}
export function unbindCpuWorkerOwner(runtime){owners.delete(runtime);}
