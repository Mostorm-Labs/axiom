// Delayed public-module import regression;no Runtime performance claim.
import {chromium} from '../../node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';import http from 'node:http';
import * as fixtures from './fixture_modules.mjs';
const result={scope:'Sampling fixture readiness under delayed SDK module import',errors:[]};let browser;
const server=http.createServer(async(req,res)=>{try{
 const name=new URL(req.url,'http://local').pathname;res.setHeader('Content-Type',name.endsWith('.html')?'text/html':'text/javascript');
 if(name==='/axiom.mjs'){await new Promise(r=>setTimeout(r,300));res.end('export const createRuntime=()=>{};');return;}
 const bytes=await fixtures.readFixtureModule(name);res.end(bytes??await fs.readFile(new URL('./navigation_sampling.html',import.meta.url)));
}catch(error){res.statusCode=500;res.end(String(error));}});
try{
 await new Promise(r=>server.listen(0,'127.0.0.1',r));browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
 const page=await browser.newPage();page.on('pageerror',e=>result.errors.push(String(e)));await page.goto('http://127.0.0.1:'+server.address().port+'/sampling.html');
 result.beforeReady=await page.evaluate(()=>typeof window.probe==='undefined');
 await fixtures.waitForNavigationProbe(page);
 result.ready=await page.evaluate(()=>typeof probe.setup==='function'&&typeof probe.measure==='function');
 result.status=result.beforeReady&&result.ready&&!result.errors.length?'PASS':'FAIL';
}catch(error){result.status='FAIL';result.error=String(error);}finally{
 if(browser)await browser.close();await new Promise(r=>server.close(r));await fs.writeFile(process.argv[2],JSON.stringify(result,null,2));console.log(JSON.stringify(result));if(result.status!=='PASS')process.exitCode=1;
}
