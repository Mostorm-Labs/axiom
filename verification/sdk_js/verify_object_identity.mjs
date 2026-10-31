import fs from 'node:fs/promises';
import path from 'node:path';
import os from 'node:os';
import http from 'node:http';
import {createHash} from 'node:crypto';
import {launchChrome} from '../../tools/release/chrome_probe.mjs';

const root = await fs.mkdtemp(path.join(os.tmpdir(), 'axiom-object-identity-web-'));
const files = path.join(root, 'consumer');
const delay = ms => new Promise(resolve => setTimeout(resolve, ms));
let browser, server, socket, failure;
const observation = {checks: {}, errors: [], exceptions: [], ids: {}, input: 'Chrome CDP mouse, public JS SDK, independent WASM instances'};
try {
  await fs.mkdir(files);
  const hashes = {};
  for (const name of ['axiom.mjs', 'internal', 'axiom.wasm', 'axiom.data', 'pointer_input.mjs', 'text_input.mjs']) {
    await fs.cp(path.resolve(process.argv[2], name), path.join(files, name), {recursive: true});
    if (name !== 'internal') hashes[name] = createHash('sha256').update(await fs.readFile(path.join(files, name))).digest('hex');
  }
  hashes['internal/axiom.mjs'] = createHash('sha256').update(await fs.readFile(path.join(files, 'internal/axiom.mjs'))).digest('hex');
  observation.files = hashes;
  await fs.copyFile(new URL('./object_identity.html', import.meta.url), path.join(files, 'index.html'));
  server = http.createServer(async (request, response) => {
    try {
      const filename = path.resolve(files, '.' + new URL(request.url, 'http://localhost').pathname.replace(/\/$/, '/index.html'));
      if (!filename.startsWith(files + path.sep)) throw Error('invalid path');
      response.setHeader('Content-Type', filename.endsWith('.wasm') ? 'application/wasm'
        : filename.endsWith('.mjs') ? 'text/javascript' : filename.endsWith('.html') ? 'text/html' : 'application/octet-stream');
      response.end(await fs.readFile(filename));
    } catch { response.statusCode = 404; response.end(); }
  });
  await new Promise(resolve => server.listen(0, '127.0.0.1', resolve));
  const started = await launchChrome(process.argv[3], path.join(root, 'profile'));
  browser = started.browser;
  const page = await fetch(`http://127.0.0.1:${started.port}/json/new?about:blank`, {method: 'PUT'}).then(response => response.json());
  socket = new WebSocket(page.webSocketDebuggerUrl);
  await new Promise((resolve, reject) => { socket.addEventListener('open', resolve, {once: true}); socket.addEventListener('error', reject, {once: true}); });
  let serial = 0;
  const pending = new Map();
  socket.addEventListener('message', event => {
    const message = JSON.parse(event.data);
    if (message.id) {
      const request = pending.get(message.id);
      if (!request) return;
      pending.delete(message.id); clearTimeout(request.timer);
      message.error ? request.reject(Error(JSON.stringify(message.error))) : request.resolve(message.result);
    } else if (message.method === 'Runtime.exceptionThrown') observation.exceptions.push(message.params);
  });
  const send = (method, params = {}) => new Promise((resolve, reject) => {
    const id = ++serial;
    const timer = setTimeout(() => { pending.delete(id); reject(Error(`CDP timeout: ${method}`)); }, 90000);
    pending.set(id, {resolve, reject, timer}); socket.send(JSON.stringify({id, method, params}));
  });
  const evaluate = async expression => {
    const result = await send('Runtime.evaluate', {expression, returnByValue: true, awaitPromise: true});
    if (result.exceptionDetails) throw Error(JSON.stringify(result.exceptionDetails));
    return result.result.value;
  };
  observation.browserVersion = await send('Browser.getVersion');
  await send('Runtime.enable'); await send('Page.enable');
  await send('Emulation.setDeviceMetricsOverride', {width: 900, height: 750, deviceScaleFactor: 1, mobile: false});
  await send('Page.navigate', {url: `http://127.0.0.1:${server.address().port}/`});
  for (let attempt = 0; !await evaluate('Boolean(window.probe)'); ++attempt) {
    if (attempt > 300) throw Error('SDK page did not initialize');
    await delay(50);
  }
  await evaluate('probe.ready');
  observation.buildInfo = await evaluate('probe.info()');
  const draw = async (name, x, y, dx = 100, dy = 80) => {
    const origin = await evaluate(`probe.rect(${JSON.stringify(name)})`);
    const start = {x: origin.x + x, y: origin.y + y};
    await send('Input.dispatchMouseEvent', {type: 'mousePressed', ...start, button: 'left', buttons: 1, clickCount: 1});
    await delay(30);
    if (dx || dy) await send('Input.dispatchMouseEvent', {type: 'mouseMoved', x: start.x + dx, y: start.y + dy, button: 'left', buttons: 1});
    await delay(30);
    await send('Input.dispatchMouseEvent', {type: 'mouseReleased', x: start.x + dx, y: start.y + dy, button: 'left', buttons: 0, clickCount: 1});
    await delay(100);
    return evaluate(`probe.settle(${JSON.stringify(name)})`);
  };
  const identity = async (name, x) => {
    await evaluate(`probe.select(${JSON.stringify(name)}, 'select')`);
    await draw(name, x, 80, 0, 0);
    return evaluate(`probe.primary(${JSON.stringify(name)})`);
  };
  await evaluate("probe.select('a', 'rectangle')");
  await evaluate("probe.select('b', 'rectangle')");
  const first = await draw('a', 40, 40);
  const second = await draw('b', 220, 40);
  observation.initial = {first, second, errors: await evaluate('probe.errors'),
    events: await evaluate('probe.events'), focused: await evaluate('document.hasFocus()')};
  if (first.state.objects !== 1 || second.state.objects !== 1 || first.packets !== 1 || second.packets !== 1)
    throw Error('Initial public pointer creation failed');
  observation.ids.a = await identity('a', 80);
  observation.ids.b = await identity('b', 260);
  observation.checks.independentObjectIds = /^[0-9a-f]{32}$/.test(observation.ids.a)
    && /^[0-9a-f]{32}$/.test(observation.ids.b)
    && observation.ids.a !== '0'.repeat(32) && observation.ids.b !== '0'.repeat(32)
    && observation.ids.a !== observation.ids.b;

  const restored = await evaluate('probe.restore()');
  if (restored.objects !== 1) throw Error('Checkpoint restore failed before the regression scenario');
  await evaluate("probe.select('c', 'rectangle')");
  const continued = await draw('c', 220, 40);
  observation.checks.snapshotThenPointerCreation = continued.state.objects === 2 && continued.packets === 1;
  if (observation.checks.snapshotThenPointerCreation) {
    observation.ids.c = await identity('c', 260);
    const undo = await evaluate("probe.history('c', true)");
    const redo = await evaluate("probe.history('c', false)");
    observation.checks.restoredUndoRedo = observation.ids.c !== observation.ids.a
      && undo.objects === 1 && redo.objects === 2;
  } else observation.checks.restoredUndoRedo = false;

  const intoB = await evaluate("probe.merge('b', 'a')");
  const intoA = await evaluate("probe.merge('a', 'b')");
  observation.remote = {intoA, intoB};
  observation.checks.remoteMerge = intoA.outcomes[0].ok && intoB.outcomes[0].ok
    && intoA.state.objects === 2 && intoB.state.objects === 2;
  observation.checks.noRemoteEcho = intoA.packets === 1 && intoB.packets === 1;
  if (observation.checks.remoteMerge) {
    const duplicate = await evaluate("probe.merge('b', 'a')");
    const undo = await evaluate("probe.history('b', true)");
    const remoteUndo = await evaluate("probe.merge('a', 'b', true)");
    const redo = await evaluate("probe.history('b', false)");
    const remoteRedo = await evaluate("probe.merge('a', 'b', true)");
    observation.checks.remoteDeduplication = duplicate.outcomes[0].ok && duplicate.state.objects === 2;
    observation.checks.collaborativeUndoRedo = undo.objects === 1 && remoteUndo.outcomes[0].ok
      && remoteUndo.state.objects === 1 && redo.objects === 2 && remoteRedo.outcomes[0].ok
      && remoteRedo.state.objects === 2;
  } else {
    observation.checks.remoteDeduplication = false;
    observation.checks.collaborativeUndoRedo = false;
  }
  observation.errors = await evaluate('probe.errors');
  try { await evaluate('probe.close()'); observation.checks.cleanClose = true; }
  catch (error) { observation.checks.cleanClose = false; observation.cleanupError = String(error); }
  observation.checks.noRuntimeErrors = observation.errors.length === 0;
  observation.checks.noUncaughtExceptions = observation.exceptions.length === 0;
  observation.status = Object.values(observation.checks).every(Boolean) ? 'PASS' : 'FAIL';
  if (observation.status !== 'PASS') failure = Error('ObjectId regression checks failed');
} catch (error) {
  failure = error; observation.status = 'ERROR'; observation.error = String(error.stack || error);
} finally {
  socket?.close();
  if (browser) {
    const exited = new Promise(resolve => browser.exitCode !== null ? resolve() : browser.once('exit', resolve));
    browser.kill(); await Promise.race([exited, delay(5000)]);
  }
  if (server) await new Promise(resolve => server.close(resolve));
  if (process.argv[4]) {
    const output = path.resolve(process.argv[4]); await fs.mkdir(path.dirname(output), {recursive: true});
    await fs.writeFile(output, JSON.stringify(observation, null, 2) + '\n');
  }
  console.log(JSON.stringify(observation));
  await fs.rm(root, {recursive: true, maxRetries: 6, retryDelay: 100});
}
process.exitCode = failure ? 1 : 0;
