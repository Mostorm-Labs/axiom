import assert from 'node:assert/strict';
import fs from 'node:fs';
const source=fs.readFileSync(new URL('../../sdk/web/bridge.cpp',import.meta.url),'utf8');
const body=source.slice(source.indexOf('sdk::Json frame(bool force)'),source.indexOf('bool shutdown() override'));
assert.match(body,/const bool wasPending/);
assert.match(body,/if \(wasPending && result == platform::WebFrameResult::kRetry/);
console.log('Only a completed GPU fence may prepare again in the same opportunity');
