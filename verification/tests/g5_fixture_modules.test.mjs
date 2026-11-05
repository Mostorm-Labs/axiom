import test from 'node:test';
import assert from 'node:assert/strict';
import fs from 'node:fs/promises';
import http from 'node:http';
import {readFixtureModule} from '../runners/g5/fixture_modules.mjs';

test('all local sampler imports can be fetched as JavaScript modules',async t=>{
 const html=await fs.readFile(new URL('../runners/g5/navigation_sampling.html',import.meta.url),'utf8');
 const imports=[...html.matchAll(/(?:from\s*|import\()['"](\/[^'"]+\.mjs)['"]/g)].map(m=>m[1]).filter(n=>n!=='/axiom.mjs');
 assert.ok(imports.length>0);
 const server=http.createServer(async(req,res)=>{
  const bytes=await readFixtureModule(req.url);
  res.writeHead(bytes?200:404,{'Content-Type':'text/javascript'});res.end(bytes);
 });
 await new Promise(r=>server.listen(0,'127.0.0.1',r));
 t.after(()=>new Promise(r=>server.close(r)));
 for(const name of imports){
  const response=await fetch('http://127.0.0.1:'+server.address().port+name);
  assert.equal(response.status,200,name);assert.equal(response.headers.get('content-type'),'text/javascript');
  assert.ok((await response.text()).length>0,name);
 }
});
test('fixture routing does not expose the SDK or an arbitrary local path',async()=>{
 for(const path of ['/axiom.mjs','/internal/axiom.mjs','/../AGENTS.md','/missing.mjs'])assert.equal(await readFixtureModule(path),null);
});
