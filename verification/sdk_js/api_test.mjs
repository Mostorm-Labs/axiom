import assert from 'node:assert/strict';
import {readFileSync} from 'node:fs';
import { SDK_VERSION, AxiomError, createRuntime } from '../../sdk/web/axiom.mjs';
assert.equal(SDK_VERSION, JSON.parse(readFileSync(new URL('../../sdk/version.json',import.meta.url),'utf8')).version);
const error = new AxiomError('STALE_TARGET', 'refresh and retry', {state: 4});
assert.equal(error.code, 'STALE_TARGET');
assert.equal(error.details.state, 4);
await assert.rejects(createRuntime(), error=>error instanceof AxiomError && error.code==='UNSUPPORTED_ENVIRONMENT');
console.log('JS SDK public error contract PASS');
