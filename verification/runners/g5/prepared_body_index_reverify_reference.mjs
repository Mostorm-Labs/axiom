import fs from 'node:fs/promises';import {createHash} from 'node:crypto';import {isDeepStrictEqual} from 'node:util';
import {comparePixels,publicHudBand,compareSnapshotPixels} from './ur0/pixel_oracle.mjs';
const sha=b=>createHash('sha256').update(b).digest('hex');
export async function createReferenceQualifier(catalog,oracle,onMiss=compareSnapshotPixels){
  if(sha(await fs.readFile(oracle))!==catalog.oracle.sha256)throw Error('oracle identity mismatch');
  for(const resource of catalog.resources)if(sha(await fs.readFile(resource.path))!==resource.sha256)throw Error('resource identity mismatch: '+resource.path);
  return async args=>{
    const snapshot=await fs.readFile(args.snapshot),digest=sha(snapshot);
    const entry=catalog.entries.find(e=>e.snapshot.sha256===digest&&isDeepStrictEqual(e.request,args.request));
    if(!entry)return {...await onMiss(args),reference_origin:'NEW_ORACLE_ONCE'};
    if(entry.reference.status!=='REFERENCE_RENDERED')throw Error('saved reference incomplete');
    const savedSnapshot=await fs.readFile(entry.snapshot.path),rgba=await fs.readFile(entry.rgba.path),edge=await fs.readFile(entry.edge.path);
    if(!snapshot.equals(savedSnapshot)||sha(rgba)!==entry.rgba.sha256||sha(edge)!==entry.edge.sha256)throw Error('saved reference identity mismatch');
    await fs.writeFile(args.prefix+'.request.json',JSON.stringify(args.request));
    await fs.writeFile(args.prefix+'.rgba',rgba);await fs.writeFile(args.prefix+'.edge',edge);
    return {...comparePixels(args.actual,rgba,edge,publicHudBand(args.request)),reference:entry.reference,
      reference_snapshot:args.snapshot,reference_origin:'SAVED_EXACT_INPUT',reference_inputs:{snapshot:entry.snapshot,rgba:entry.rgba,edge:entry.edge,request:entry.request,oracle:catalog.oracle,resources:catalog.resources},
      candidate_geometry_source:'fresh public root SDK WebGL2 framebuffer',readback:'correctness-only cold capture; original independent CPU AA oracle'};
  };
}
