// Verification-only modules shared by every external sampler host.
import fs from 'node:fs/promises';
const names=new Set(['/navigation_metrics.mjs','/browser_metrics.mjs','/editing_actions.mjs']);
export async function readFixtureModule(pathname){
 return names.has(pathname)?fs.readFile(new URL('.'+pathname,import.meta.url)):null;
}
