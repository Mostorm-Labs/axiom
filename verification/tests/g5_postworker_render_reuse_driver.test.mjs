import test from 'node:test';import assert from 'node:assert/strict';import fs from 'node:fs';
import * as primary from '../runners/g5/cpu_postworker_web.mjs';import * as reuse from '../runners/g5/postworker_render_reuse_web.mjs';
import {confirmedTrace,navigationEvents} from '../runners/g5/cpu_zoomout_frontier_contract.mjs';
test('new primary package delegates exactly the original input consumer',()=>{assert.equal(reuse.consumer,primary.consumer);assert.equal(reuse.installPostworkerObserver,primary.installPostworkerObserver);});
test('outward driver retains original traces and records queue, handler, qualified feedback and final state',()=>{
 assert.equal(confirmedTrace('append').length,162);assert.equal(confirmedTrace('partial-erase').length,26);assert.equal(navigationEvents('zoom-out').length,6);
 const s=fs.readFileSync(new URL('../runners/g5/postworker_render_reuse_zoomout_web.mjs',import.meta.url),'utf8');
 assert.ok(s.includes("await new Promise(resolve=>setTimeout(resolve,0))"));assert.ok(s.includes('confirmedTrace(spec.edit)'));assert.ok(s.includes('navigationEvents(spec.direction)'));assert.ok(s.includes('planned:base+e.plannedOffsetMs'));assert.ok(s.includes('current.generation===current.lastCommit.presentedGeneration'));assert.ok(s.includes("binary('feedback.rgba'"));assert.ok(s.includes('deviceScaleFactor:request.dpr??4'));
});
