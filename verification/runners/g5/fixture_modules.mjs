// Verification-only modules shared by every external sampler host.
import fs from 'node:fs/promises';
const names=new Set(['/navigation_metrics.mjs','/browser_metrics.mjs','/editing_actions.mjs','/navigation_observer.mjs','/feedback_metrics.mjs']);
export async function readFixtureModule(pathname){
 return names.has(pathname)?fs.readFile(new URL('.'+pathname,import.meta.url)):null;
}

// Dynamic import may outlive the document load event. Never start a fixture
// until its module has published the complete probe. This is setup,not input.
export async function waitForNavigationProbe(page){
 await page.waitForFunction(()=>typeof window.probe?.setup==='function'&&typeof window.probe?.measure==='function');
}
