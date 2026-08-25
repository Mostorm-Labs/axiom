import { spawn } from "node:child_process";
import { mkdir, writeFile } from "node:fs/promises";
import { request } from "node:http";
import { setTimeout as delay } from "node:timers/promises";

const args = Object.fromEntries(process.argv.slice(2).reduce((out, value, index, all) => {
  if (value.startsWith("--")) out.push([value.slice(2), all[index + 1]]);
  return out;
}, []));
if (!args.chrome || !args.url || !args.output || !args.rgba || !args.gpu) {
  throw new Error("required: --chrome --url --output --rgba --gpu");
}
const outDir = args.output.replace(/[\\/][^\\/]+$/, "");
await mkdir(outDir, { recursive: true });

function httpJson(path) {
  return new Promise((resolve, reject) => {
    request({ hostname: "127.0.0.1", port: 9222, path, method: "GET" }, (response) => {
      let body = "";
      response.setEncoding("utf8");
      response.on("data", (chunk) => body += chunk);
      response.on("end", () => resolve(JSON.parse(body)));
    }).on("error", reject).end();
  });
}
const profile = `${process.env.TEMP || process.env.TMP || "."}\\gt-g0-17-chrome-${process.pid}`;
const chrome = spawn(args.chrome, [
  "--remote-debugging-port=9222", `--user-data-dir=${profile}`,
  "--no-first-run", "--no-default-browser-check", "--disable-extensions",
  "--enable-precise-memory-info", "--disable-background-timer-throttling",
  "--disable-backgrounding-occluded-windows", "--disable-renderer-backgrounding",
  args.url,
], { windowsHide: true, stdio: "ignore" });
try {
  let target;
  for (let i = 0; i < 100; ++i) {
    try {
      const list = await httpJson("/json/list");
      target = list.find((item) => item.type === "page" && item.url.startsWith(args.url.split("/").slice(0, 3).join("/")));
      if (target) break;
    } catch {}
    await delay(100);
  }
  if (!target) throw new Error("Chrome CDP target did not start");
  const ws = new WebSocket(target.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { ws.onopen = resolve; ws.onerror = reject; });
  let id = 0;
  const pending = new Map();
  ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.id && pending.has(message.id)) {
      const { resolve, reject } = pending.get(message.id);
      pending.delete(message.id);
      if (message.error) reject(new Error(message.error.message)); else resolve(message.result);
    }
  };
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const requestId = ++id;
    pending.set(requestId, { resolve, reject });
    ws.send(JSON.stringify({ id: requestId, method, params }));
  });
  await send("Runtime.enable");
  await send("Page.enable");
  await delay(1000);
  const pageErrors = [];
  ws.onmessage = (event) => {
    const message = JSON.parse(event.data);
    if (message.method === "Runtime.exceptionThrown") pageErrors.push(message.params.exceptionDetails.text || "page exception");
    if (message.id && pending.has(message.id)) {
      const { resolve, reject } = pending.get(message.id);
      pending.delete(message.id);
      if (message.error) reject(new Error(message.error.message)); else resolve(message.result);
    }
  };
  const evaluate = async (expression) => {
    const result = await send("Runtime.evaluate", { expression, awaitPromise: true, returnByValue: true, userGesture: true });
    if (result.exceptionDetails) throw new Error(result.exceptionDetails.text || "Runtime.evaluate failed");
    return result.result?.value;
  };
  await evaluate(`(async () => {
    const wait = async (predicate) => { for (let i = 0; i < 300; ++i) { if (predicate()) return; await new Promise(r => setTimeout(r, 50)); } throw new Error("timeout waiting for UI state"); };
    [...document.querySelectorAll('button')].find(b => b.textContent === 'Load Fixture')?.click();
    await wait(() => document.querySelector('[data-testid=backend]')?.textContent === 'ganesh-webgl2');
    [...document.querySelectorAll('button')].find(b => b.textContent === 'Replay')?.click();
    await wait(() => document.querySelector('[data-testid=digest]')?.textContent === '47826449b895ac4f4a57b4f386379775');
    [...document.querySelectorAll('button')].find(b => b.textContent === 'Render')?.click();
    await wait(() => document.querySelector('[data-testid=metrics]')?.textContent?.includes('pixels within ±2'));
  })()`);
  const result = await evaluate(`(async () => {
    const expectedDigest = '47826449b895ac4f4a57b4f386379775';
    const module = window.__canvasPocModule;
    const check = (s, a) => { if (s !== 0) throw new Error(a + ' status ' + s); };
    const withBytes = (bytes, fn) => { const p = module._malloc(bytes.byteLength || 1); try { module.HEAPU8.set(bytes, p); return fn(p, bytes.byteLength); } finally { module._free(p); } };
    const replay = (text) => { const bytes = new TextEncoder().encode(text); withBytes(bytes, (p, n) => check(module._canvas_poc_web_replay(p, n), 'replay')); };
    const digest = () => { const p = module._malloc(33), r = module._malloc(4); try { check(module._canvas_poc_web_digest(p, 33, r), 'digest'); return module.UTF8ToString(p); } finally { module._free(p); module._free(r); } };
    const surface = () => withBytes(new TextEncoder().encode('#canvas\\0'), (p) => check(module._canvas_poc_web_surface_create(p), 'surface'));
    const [checker, font, fixture] = await Promise.all(['/fixtures/checker.png','/fixtures/Roboto-Regular.ttf','/fixtures/scene.ndjson'].map(u => fetch(u).then(r => u.endsWith('ndjson') ? r.text() : r.arrayBuffer())));
    const load = () => { withBytes(new Uint8Array(checker), (cp, cn) => withBytes(new Uint8Array(font), (fp, fn) => check(module._canvas_poc_web_load_assets(cp, cn, fp, fn), 'load'))); replay(fixture); };
    for (let i = 0; i < 100; ++i) { load(); if (digest() !== expectedDigest) throw new Error('lifecycle digest mismatch'); surface(); check(module._canvas_poc_web_render(), 'lifecycle render'); }
    let generated = ''; let seq = 8; for (let id = 1000; id < 1996; ++id, ++seq) { const i = id - 1000; generated += JSON.stringify({v:1,seq,op:'create',node:{id,type:'rect',order:100+i,x:(i%40)*20,y:Math.floor(i/40)*20,width:12,height:12,color:[64,120,220,96]}}) + '\\n'; }
    replay(generated); for (let i = 0; i < 60; ++i) { await new Promise(requestAnimationFrame); check(module._canvas_poc_web_render(), 'warmup'); }
    const gl = document.querySelector('#canvas').getContext('webgl2'); const dbg = gl?.getExtension('WEBGL_debug_renderer_info'); const frames = []; const memory = []; const wasmBefore = module.HEAPU8.buffer.byteLength; const start = performance.now(); let next = start;
    while (performance.now() - start < 60000) { await new Promise(requestAnimationFrame); const t = performance.now(); check(module._canvas_poc_web_render(), 'smoke render'); frames.push(performance.now() - t); if (performance.memory && performance.now() >= next) { memory.push({elapsed_ms: Math.round(performance.now()-start), bytes: performance.memory.usedJSHeapSize}); next += 5000; } }
    const wasmAfter = module.HEAPU8.buffer.byteLength; frames.sort((a,b)=>a-b); const pct = q => frames[Math.min(frames.length-1, Math.max(0, Math.ceil(frames.length*q)-1))];
    load(); surface(); check(module._canvas_poc_web_render(), 'artifact render');
    const req = module._malloc(4); if (module._canvas_poc_web_readback(0,0,req) !== 6) throw new Error('readback size query failed'); const n = module.HEAPU32[req/4]; const pix = module._malloc(n); check(module._canvas_poc_web_readback(pix,n,req), 'readback'); const raw = module.HEAPU8.slice(pix,pix+n); let binary = ''; for (let offset = 0; offset < raw.length; offset += 0x8000) binary += String.fromCharCode(...raw.subarray(offset, offset + 0x8000)); const rgba = btoa(binary); module._free(pix); module._free(req);
    const debugVendor = dbg ? gl.getParameter(dbg.UNMASKED_VENDOR_WEBGL) : gl.getParameter(gl.VENDOR); const debugRenderer = dbg ? gl.getParameter(dbg.UNMASKED_RENDERER_WEBGL) : gl.getParameter(gl.RENDERER);
    return {platform:'web',backend:'ganesh-webgl2-hardware',digest:expectedDigest,warp:false,lifecycle:100,smoke_seconds:60,smoke_frames:frames.length,p50_ms:pct(.5),p95_ms:pct(.95),p99_ms:pct(.99),max_ms:frames.at(-1),max_frame_ms:frames.at(-1),peak_memory_bytes:memory.length?Math.max(...memory.map(x=>x.bytes)):null,memory_scope:'renderer-js-heap',memory_samples:memory,wasm_heap_before:wasmBefore,wasm_heap_after:wasmAfter,webgl_vendor:debugVendor,webgl_renderer:debugRenderer,user_agent:navigator.userAgent,visual_metrics:document.querySelector('[data-testid=metrics]')?.textContent,page_errors:[] ,rgba_base64:rgba};
  })()`);
  result.page_errors = pageErrors;
  await writeFile(args.output, JSON.stringify(result, null, 2) + "\n");
  await writeFile(args.rgba, Buffer.from(result.rgba_base64, "base64"));
  delete result.rgba_base64;
  console.log(JSON.stringify(result));
  await send("Page.navigate", { url: "chrome://gpu" });
  await delay(2500);
  const gpuText = await evaluate("document.body?.innerText || ''");
  await writeFile(args.gpu, gpuText || "chrome://gpu snapshot unavailable\n");
  ws.close();
} finally {
  chrome.kill();
}
