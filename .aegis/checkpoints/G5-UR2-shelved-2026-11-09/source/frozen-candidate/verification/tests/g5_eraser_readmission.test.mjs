import test from 'node:test';import assert from 'node:assert/strict';
import {CASES,consumer} from '../runners/g5/eraser_readmission.mjs';
test('independent post-Up branches keep real wheel, preselected drag and cold oracle separate',()=>{
  assert.deepEqual(CASES,['reerase-cancel-navigation','pan','pinch','preselected-drag']);
  const src=consumer.toString();
  assert.match(src,/new WheelEvent\('wheel'/);assert.match(src,/ctrlKey:pinch/);
  assert.match(src,/p\.afterUp\.at\+4/);assert.match(src,/cold point selection/);
  assert.match(src,/point selection mutated document/);assert.match(src,/next Down rejected/);
  const hot=src.slice(src.indexOf('const run='),src.indexOf('const pixels='));
  assert.doesNotMatch(hot,/readPixels|captureSnapshot|settled\(\).*nextDown/);
  assert.match(src,/check\(activePhase===null/);assert.match(src,/checkpoint_mapping/);
  assert.doesNotMatch(src,/HEAP|_sdk_|performance\.memory/);
});
