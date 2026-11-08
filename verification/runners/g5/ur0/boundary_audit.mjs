// Inspect actual binary exports and installed files, not just source declarations.
import fs from 'node:fs/promises';
import path from 'node:path';
import {pathToFileURL} from 'node:url';
import {createHash} from 'node:crypto';
const names=['axiom_g5_trace_control','axiom_g5_trace_bytes','axiom_g5_trace_size'];
export function resolveExports(binaryNames,js){
 const bindings=[...js.matchAll(/Module\["(_[A-Za-z0-9_]+)"\]=wasmExports\["([^"\]]+)"\]/g)].map(m=>({name:m[1].slice(1),binary_name:m[2]}));
 for(const b of bindings)if(!binaryNames.includes(b.binary_name))throw Error('generated JS binding references absent WASM export '+b.binary_name);
 if(!bindings.length)throw Error('no generated JS export bindings resolved');
 return{names:[...new Set(bindings.map(b=>b.name))].sort(),bindings,binary_names:binaryNames};
}
export function inspectBoundary(normal,qualified,installed){
 const errors=[];
 if(normal.some(x=>names.includes(x)))errors.push('normal WASM contains private observation exports');
 const delta=qualified.filter(x=>!normal.includes(x)).sort();
 if(JSON.stringify(delta)!==JSON.stringify([...names].sort())||normal.some(x=>!qualified.includes(x)))errors.push('qualifier export delta differs from the frozen three names');
 for(const file of installed)if(/\.(a|lib)$/.test(file)||/(^|\/)include\/canvas\//.test(file)||/g5_observation/.test(file))errors.push('private/internal install file: '+file);
 return{status:errors.length?'FAILED':'QUALIFIED',errors,normal_exports:normal,qualified_exports:qualified,private_export_delta:delta,installed_files:installed};
}
if(process.argv[1]&&import.meta.url===pathToFileURL(path.resolve(process.argv[1])).href){
 const [normalArg,qualArg,installArg,outArg]=process.argv.slice(2);
 const normal=path.resolve(normalArg),qualified=path.resolve(qualArg),installed=path.resolve(installArg);
 const exports=async root=>resolveExports(WebAssembly.Module.exports(await WebAssembly.compile(await fs.readFile(path.join(root,'internal/axiom.wasm')))).map(x=>x.name),await fs.readFile(path.join(root,'internal/axiom.mjs'),'utf8'));
 const files=(await fs.readdir(installed,{recursive:true})).sort(),manifest=[];
 for(const file of files){const p=path.join(installed,file);if((await fs.stat(p)).isFile())manifest.push({path:file,bytes:(await fs.stat(p)).size,sha256:createHash('sha256').update(await fs.readFile(p)).digest('hex')});}
 const normalSymbols=await exports(normal),qualifiedSymbols=await exports(qualified);
 const value=inspectBoundary(normalSymbols.names,qualifiedSymbols.names,manifest.map(x=>x.path));
 value.binary_symbol_resolution={normal:normalSymbols,qualified:qualifiedSymbols};
 if(qualifiedSymbols.binary_names.length-normalSymbols.binary_names.length!==3)value.errors.push('actual binary export count delta is not three');
 const normalApi=Object.keys(await import(pathToFileURL(path.join(normal,'axiom.mjs')))).sort(),qualApi=Object.keys(await import(pathToFileURL(path.join(qualified,'axiom.mjs')))).sort();
 if(JSON.stringify(normalApi)!==JSON.stringify(qualApi)||normalApi.some(x=>/trace|debug|module|heap/i.test(x)))value.errors.push('root JS exports changed or expose private capability');
 for(const file of ['axiom.mjs','axiom.d.mts','package.json']){
  const same=(await fs.readFile(path.join(normal,file))).equals(await fs.readFile(path.join(qualified,file)))&&(await fs.readFile(path.join(normal,file))).equals(await fs.readFile(path.join(installed,'js',file)));
  if(!same)value.errors.push('normal/qualifier/install public declaration mismatch: '+file);
 }
 const js=await fs.readFile(path.join(normal,'internal/axiom.mjs'),'utf8');
 if(names.some(x=>js.includes(x)))value.errors.push('normal internal JS exposes private observer');
 value.root_exports=normalApi;value.normal_install_manifest=manifest;value.status=value.errors.length?'FAILED':'QUALIFIED';
 await fs.writeFile(outArg,JSON.stringify(value,null,2)+'\n');
 console.log(JSON.stringify({status:value.status,errors:value.errors,private_export_delta:value.private_export_delta,installed_files:manifest.length}));
 if(value.errors.length)process.exitCode=1;
}
