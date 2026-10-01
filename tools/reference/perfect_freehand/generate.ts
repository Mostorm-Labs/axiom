// Run: npx --yes tsx@4.23.15 tools/reference/perfect_freehand/generate.ts [--write]
// Executes ONLY the blob-verified upstream implementation; never calls the C++ port.
import { readFileSync, writeFileSync } from 'node:fs'
import { createHash } from 'node:crypto'
import { execFileSync } from 'node:child_process'
import { getStrokePoints } from '../../../verification/reference/perfect_freehand/src/getStrokePoints'
import { getStrokeOutlinePoints } from '../../../verification/reference/perfect_freehand/src/getStrokeOutlinePoints'

const pkg = '.aegis/packages/GT-G4-5-R01/'
const result = '.aegis/results/GT-G4-5-R01/'
const source = 'verification/reference/perfect_freehand/'
const read = (p: string) => JSON.parse(readFileSync(p, 'utf8'))
const sha = (p: string) => createHash('sha256').update(readFileSync(p)).digest('hex')
const lock = read(pkg + 'reference.lock.json')
const bindings = { ...lock.blobs, LICENSE: lock.license.blob_sha }
for (const [upstream, expected] of Object.entries(bindings)) {
  const local = upstream.includes('/src/test/') ? source + 'inputs.json' :
    upstream.endsWith('draw.tsx') ? source + 'draw.tsx' :
    upstream === 'LICENSE' ? source + 'LICENSE' : source + 'src/' + upstream.split('/').pop()
  const actual = execFileSync('git', ['hash-object', local], { encoding: 'utf8' }).trim()
  if (actual !== expected) throw Error('Reference blob mismatch: ' + upstream)
}
const plan = read(pkg + 'corpus-plan.json').required_fixtures
const corpus = read(result + 'corpus-inputs.json').fixtures
if (plan.length !== corpus.length) throw Error('Corpus count mismatch')
for (let i = 0; i < plan.length; ++i) {
  const p = plan[i], c = corpus[i]
  if (p.id !== c.id || p.class !== c.class || JSON.stringify(p.options) !== JSON.stringify(c.options))
    throw Error('Corpus metadata mismatch: ' + p.id)
  if (p.points && JSON.stringify(p.points) !== JSON.stringify(c.points)) throw Error('Corpus input mismatch')
  if (p.generator) {
    if (p.id !== 'long_wave_512' || c.points.length !== 512) throw Error('Unknown formula')
    c.points.forEach((v: number[], j: number) => {
      const want = [j * 2, 60 * Math.sin(j * .08) + .015 * j, .5 + .35 * Math.sin(j * .031)]
      if (v.some((x, k) => Math.abs(x - want[k]) > 1e-12)) throw Error('Formula mismatch')
    })
  }
}

// Focused completeness cases supplement (never replace) the immutable 17-case corpus.
const extra = [
  { id: 'empty', points: [], options: {} },
  { id: 'duplicate_dot', points: [[5, 9], [5, 9], [5, 9]], options: { last: true } },
  { id: 'two_point_pressure_discard', points: [[0, 0, .2], [80, 20, .9]], options: { last: true, simulatePressure: false } },
  { id: 'invalid_missing_pressure', points: [[0, 0, -1], [20, 3], [42, 10, -2], [80, 5, 0]], options: { last: true, simulatePressure: false } },
  { id: 'full_length_taper', points: [[0, 0, .2], [20, 8, .8], [60, 9, .4]], options: { last: true, start: { taper: true }, end: { taper: true } } },
  { id: 'only_start_taper', points: [[0, 0], [30, 9], [70, 0]], options: { last: true, start: { taper: 25 } } },
  { id: 'only_end_taper', points: [[0, 0], [30, 9], [70, 0]], options: { last: true, end: { taper: 25 } } },
  { id: 'incomplete_tapered_dot', points: [[5, 9], [5, 9], [5, 9]], options: { start: { taper: true } } },
  { id: 'complete_tapered_dot', points: [[5, 9], [5, 9], [5, 9]], options: { last: true, start: { taper: true } } },
  { id: 'end_noise', points: [[0, 0], [30, 0], [59, 0], [60, 0]], options: { streamline: 0, last: true } },
  { id: 'zero_size', points: [[0, 0], [30, 0], [60, 0]], options: { size: 0, simulatePressure: false } },
  { id: 'negative_size', points: [[0, 0], [30, 0], [60, 0]], options: { size: -1, simulatePressure: false } },
  { id: 'incomplete_streamline', points: [[0, 0], [30, 0], [60, 10]], options: { streamline: 1, last: false } },
].map(x => ({ ...x, class: 'supplemental' }))
const fixtures = [...corpus, ...extra].map(f => {
  const points = getStrokePoints(f.points, f.options)
  const outline = getStrokeOutlinePoints(points, f.options)
  return { ...f, conditioned: points, outline }
})
const oldPoints = read(result + 'reference-stroke-points.json').fixtures
const oldOutlines = read(result + 'reference-outlines.json').fixtures
// Regeneration is a provenance check, not an opportunity to update frozen expectations.
function compare(a: any, b: any): boolean {
  if (typeof a === 'number' && typeof b === 'number') return Math.abs(a - b) <= 1e-12
  if (Array.isArray(a)) return Array.isArray(b) && a.length === b.length && a.every((v, i) => compare(v, b[i]))
  if (a && typeof a === 'object') return b && Object.keys(a).length === Object.keys(b).length && Object.keys(a).every(k => compare(a[k], b[k]))
  return a === b
}
for (const f of fixtures.slice(0, corpus.length)) {
  if (!compare(f.conditioned, oldPoints[f.id]) || !compare(f.outline, oldOutlines[f.id]))
    throw Error('Frozen upstream output differs: ' + f.id)
}
const num = (v: number) => Number.isNaN(v) ? 'std::numeric_limits<double>::quiet_NaN()' : String(v)
const vec = (v: number[]) => '{' + v.map(num).join(',') + '}'
const rows = fixtures.map(f => {
  const o = f.options, start = o.start ?? {}, end = o.end ?? {}
  const options = [o.size ?? 16, o.thinning ?? .5, o.smoothing ?? .5, o.streamline ?? .5,
    o.simulatePressure ?? true, start.cap ?? true, typeof start.taper === 'number' ? start.taper : 0,
    start.taper !== undefined && start.taper !== false, end.cap ?? true,
    typeof end.taper === 'number' ? end.taper : 0, end.taper !== undefined && end.taper !== false,
    o.last ?? false, start.taper === true, end.taper === true].join(',')
  // The original 17 expectations remain exactly the frozen JSON values.
  const points = oldPoints[f.id] ?? f.conditioned, outline = oldOutlines[f.id] ?? f.outline
  return `Fixture{"${f.id}", {${f.points.map(vec).join(',')}}, StrokeOptions{${options}}, {${points.map(p => vec([...p.point,p.pressure,...p.vector,p.distance,p.runningLength])).join(',')}}, {${outline.map(vec).join(',')}}}`
})
const header = `// Generated by tools/reference/perfect_freehand/generate.ts; do not edit.\n#pragma once\n#include "canvas/ink/vector_stroke_reference.hpp"\n#include <string_view>\n#include <vector>\nnamespace canvas::ink::reference::test_fixture {\nstruct Fixture { std::string_view id; std::vector<VectorStrokeInput> input; StrokeOptions options; std::vector<StrokePoint> points; std::vector<StrokeOutlinePoint> outline; };\ninline std::vector<Fixture> all() { return {\n${rows.join(',\n')}\n}; }\n}\n`
const target = 'runtime/ink/tests/vector_stroke_reference_fixtures.hpp'
if (process.argv.includes('--write')) writeFileSync(target, header)
else if (readFileSync(target, 'utf8') !== header) throw Error('Generated fixture header differs; run --write')
console.log(JSON.stringify({ status: 'PASS', reference_commit: lock.commit, verified_blobs: Object.keys(bindings).length,
  frozen_fixture_count: corpus.length, supplemental_fixture_count: extra.length, node: process.version,
  generator_sha256: sha('tools/reference/perfect_freehand/generate.ts'), corpus_sha256: sha(result + 'corpus-inputs.json'),
  stroke_points_sha256: sha(result + 'reference-stroke-points.json'), outline_sha256: sha(result + 'reference-outlines.json'),
  header_sha256: sha(target), fixtures: fixtures.map(f => ({ id: f.id, class: f.class, points: f.conditioned.length, outline: f.outline.length })) }, null, 2))
