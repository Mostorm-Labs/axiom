import {chromium} from '../verification/node_modules/playwright/index.mjs';
import fs from 'node:fs/promises';
const browser=await chromium.launch({executablePath:'/Applications/Google Chrome.app/Contents/MacOS/Google Chrome',headless:true});
const result={sourceRevision:'3fcf642bbfdda126246e29dd1b32ec7710154f62',url:'http://127.0.0.1:4239/example.html',pageErrors:[],scope:'Real installed default root SDK Demo UI smoke; not a latency benchmark'};
try{
 const context=await browser.newContext({viewport:{width:636,height:923},deviceScaleFactor:2});const page=await context.newPage();page.on('pageerror',e=>result.pageErrors.push(String(e)));
 await page.goto(result.url);await page.waitForFunction(()=>/objects/.test(document.getElementById('status')?.textContent??''),null,{timeout:60000});
 await page.locator('[data-tool="tool:0"]').click();
 await page.mouse.move(90,450);await page.mouse.down();for(let i=1;i<=80;++i)await page.mouse.move(90+i*4.5,450+Math.sin(i*.12)*70);await page.mouse.up();
 await page.waitForFunction(()=>parseInt(document.getElementById('status').textContent)>=1);
 await page.getByRole('button',{name:'矩形',exact:true}).click();await page.mouse.move(140,300);await page.mouse.down();await page.mouse.move(350,550);await page.mouse.up();
 await page.waitForFunction(()=>parseInt(document.getElementById('status').textContent)>=2);await page.mouse.move(420,600);await page.mouse.wheel(100,40);await page.waitForTimeout(500);
 result.status=await page.locator('#status').textContent();result.errorFlag=await page.locator('#status').getAttribute('data-error');await page.screenshot({path:'out/g5-retire-demo.png'});
 result.normalDemo=Boolean(result.errorFlag==='false'&&parseInt(result.status)>=2);
 await page.goto('http://127.0.0.1:4239/mixed-navigation.html?dense=1');await page.waitForFunction(()=>document.getElementById('mixed-run')?.disabled===false,null,{timeout:60000});
 result.denseSetup=await page.locator('#mixed-result').textContent();await page.locator('#mixed-run').click();await page.waitForFunction(()=>document.getElementById('mixed-run')?.disabled===false,null,{timeout:60000});
 result.denseMeasurement=JSON.parse(await page.locator('#mixed-result').textContent());await page.screenshot({path:'out/g5-retire-prefilled.png'});
 result.passed=result.normalDemo&&!result.pageErrors.length&&Object.values(result.denseMeasurement.checks).every(Boolean);
 await context.close();
}catch(error){result.error=String(error.stack??error);result.passed=false;}
finally{await browser.close();await fs.writeFile('out/g5-retire-demo-smoke.json',JSON.stringify(result,null,2));console.log(JSON.stringify(result));if(!result.passed)process.exitCode=1;}
