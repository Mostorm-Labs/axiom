import assert from 'node:assert/strict';
import {installPointerInput} from './consumer/pointer_input.mjs';

class Target extends EventTarget {
  constructor() { super(); this.captured = new Set(); }
  getBoundingClientRect() { return {left: 10, top: 20, width: 400, height: 200}; }
  setPointerCapture(id) { this.captured.add(id); }
  hasPointerCapture(id) { return this.captured.has(id); }
  releasePointerCapture(id) { this.captured.delete(id); }
}
const target = new Target(), lifecycle = new EventTarget(), facts = [];
const stop = installPointerInput({canvas: target, logicalSize: () => [200, 100],
  lifecycle, submit: fact => { facts.push(fact); return true; }});
function send(type, values) {
  const e = new Event(type, {cancelable: true});
  for (const [key, value] of Object.entries(values)) Object.defineProperty(e, key, {value});
  target.dispatchEvent(e);
  return e;
}
const base = {pointerId: 4, pointerType: 'touch', clientX: 110, clientY: 70,
  timeStamp: 10, pressure: 0.7, width: 8, height: 4, shiftKey: false};
send('pointerdown', base);
assert.equal(facts[0].source, 3);
assert.equal(facts[0].x, 50);
assert.equal(facts[0].y, 25);
assert.equal(facts[0].pressureAvailable, false);
assert.equal(facts[0].geometryAvailable, true);
assert.equal(target.hasPointerCapture(4), true);
send('pointercancel', {...base, timeStamp: 20});
assert.equal(facts.at(-1).phase, 3);
assert.equal(target.hasPointerCapture(4), false);
send('pointerdown', {...base, pointerId: 9, pointerType: 'pen', timeStamp: 30});
assert.equal(facts.at(-1).pressureAvailable, true);
send('pointerdown', {...base, pointerId: 10, timeStamp: 31});
const history = {...base, pointerId: 9, pointerType: 'pen', clientX: 130, timeStamp: 32};
send('pointermove', {...history, clientX: 140, timeStamp: 33, getCoalescedEvents: () => [history]});
assert.equal(facts.at(-2).historical, true);
assert.equal(facts.at(-1).historical, false);
assert.equal(facts.at(-1).pointer, 9);
const count = facts.length;
send('pointerdown', {...base, pointerId: 11, pointerType: 'unrecognized', timeStamp: 40});
assert.equal(facts.length, count, 'unknown input sources must not become mouse');
assert.equal(target.hasPointerCapture(11), false);
lifecycle.dispatchEvent(new Event('blur'));
assert.equal(facts.at(-1).phase, 3, 'blur cancels active contacts');
assert.equal(target.hasPointerCapture(9), false);
assert.equal(target.hasPointerCapture(10), false);
send('pointerdown', {...base, pointerId: 12, timeStamp: 50});
stop();
assert.equal(facts.at(-1).phase, 3, 'dispose cancels active pointer');
assert.equal(target.hasPointerCapture(12), false);
console.log('pointer input tests PASS');
