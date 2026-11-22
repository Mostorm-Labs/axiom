// Offline actual-target comparison. Observes differences; sets no new AA policy.
import fs from 'node:fs';
import path from 'node:path';
import {gunzipSync} from 'node:zlib';
import {createHash} from 'node:crypto';
const [root, output] = process.argv.slice(2);
if (!root || !output) throw Error('web-output-directory result-json required');
const rows = [];
for (const name of ['AB', 'undo', 'redo', 'followup']) {
  const read = variant => gunzipSync(fs.readFileSync(path.join(root, 'postup-batch-'+variant, 'primary', name+'.rgba.gz')));
  const a = read('B'), b = read('C'), width = 5724;
  if (a.length !== b.length || a.length !== width*4040*4) throw Error('target dimensions/length mismatch');
  let channels = 0, pixels = 0, maximum = 0, alpha = 0, above2 = 0;
  const bounds = [Infinity, Infinity, 0, 0], examples = [];
  for (let i = 0; i < a.length; i += 4) {
    let changed = false;
    for (let k = 0; k < 4; ++k) {
      const difference = Math.abs(a[i+k]-b[i+k]);
      if (!difference) continue;
      ++channels; changed = true; maximum = Math.max(maximum, difference);
      if (difference > 2) ++above2;
      if (k === 3) ++alpha;
    }
    if (!changed) continue;
    ++pixels;
    const x = (i/4)%width, y = Math.floor(i/4/width);
    bounds[0] = Math.min(bounds[0], x); bounds[1] = Math.min(bounds[1], y);
    bounds[2] = Math.max(bounds[2], x); bounds[3] = Math.max(bounds[3], y);
    if (examples.length < 5) examples.push({x, y, a: [...a.subarray(i,i+4)], b: [...b.subarray(i,i+4)]});
  }
  rows.push({name, bytes: a.length, differentChannels: channels, differentPixels: pixels,
    maximumChannelDifference: maximum, channelsGt2: above2, alphaChanges: alpha,
    bounds: pixels ? bounds : null, examples,
    B_sha256: createHash('sha256').update(a).digest('hex'),
    C_sha256: createHash('sha256').update(b).digest('hex')});
}
fs.writeFileSync(output, JSON.stringify({classification: 'OBSERVED_PIXEL_ROUNDING_FINDING',
  policy: 'Raw differences retained; no new tolerance or byte-equality claim.', rows}, null, 2)+'\n');
console.log(JSON.stringify(rows.map(({name,differentPixels,maximumChannelDifference,alphaChanges}) =>
  ({name,differentPixels,maximumChannelDifference,alphaChanges}))));
