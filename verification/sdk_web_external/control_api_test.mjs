import assert from 'node:assert/strict';
import { createCanvasControls } from './consumer/control_api.mjs';

const calls = [];
let reject = false;
const module = {
  _sdk_control: (action, value) => { calls.push(['control', action, value]); return reject ? 0 : 1; },
  _sdk_section: action => { calls.push(['section', action]); return reject ? 0 : 1; },
  _sdk_schedule: () => calls.push(['schedule']),
};
const controls = createCanvasControls({ module, beforeCommand: async () => calls.push(['finish']) });
await controls.selectTool('ink');
await controls.selectShape('rectangle');
await controls.selectTool('objectEraser');
await controls.setSelectionMode('lasso');
await controls.group();
assert.deepEqual(calls, [
  ['finish'], ['control', 0, 0], ['schedule'],
  ['finish'], ['control', 1, 0], ['schedule'],
  ['finish'], ['control', 6, 1], ['schedule'],
  ['finish'], ['control', 2, 2], ['schedule'],
  ['finish'], ['section', 1], ['schedule'],
]);
await assert.rejects(controls.selectTool('unknown'), /Unknown tool/);
reject = true;
await assert.rejects(controls.undo(), /rejected/);
assert.deepEqual(calls.slice(-2), [['finish'], ['control', 3, 0]]);
reject = false;
await controls.redo();
assert.deepEqual(calls.slice(-3), [['finish'], ['control', 4, 0], ['schedule']]);
let release;
const order = [];
const delayed = createCanvasControls({ module: {
  _sdk_control: (action) => { order.push(action); return 1; },
  _sdk_section: () => 1,
  _sdk_schedule: () => {},
}, beforeCommand: () => order.length ? Promise.resolve() : new Promise(resolve => { release = resolve; }) });
const first = delayed.undo();
const second = delayed.redo();
await Promise.resolve();
assert.deepEqual(order, []);
release();
await Promise.all([first, second]);
assert.deepEqual(order, [3, 4]);
const failing = createCanvasControls({ module, beforeCommand: async () => { throw new Error('IME pending'); } });
const count = calls.length;
await assert.rejects(failing.group(), /IME pending/);
assert.equal(calls.length, count);
console.log('canvas control API tests PASS');
