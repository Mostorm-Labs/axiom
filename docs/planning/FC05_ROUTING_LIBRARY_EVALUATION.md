# FC-05 routing library evaluation notes

Status: research candidate, not a dependency or algorithm decision. Source blob
identities and SHA-256 values are saved in
[research evidence](fc05/evidence/design-research/source-identities.json). No library
has been built or benchmarked in this worktree.

## Recommendation

Do not make libavoid or ELK a hard dependency before measuring the Axiom boundary.
Implement and benchmark a replaceable `ConnectorRouteProvider` contract first:

- input: immutable connector endpoints, target perimeter geometry, obstacle query,
  routing policy and revisions;
- output: derived polyline, endpoint normals, diagnostics and deterministic route key;
- no canonical mutation and no persisted route points;
- bounded cancellation and explicit no-route/budget diagnostics; an obstacle-crossing
  straight line must never be reported as a valid avoided orthogonal route.

libavoid is the strongest external candidate for the orthogonal phase. Its public
documentation describes fast object-avoiding orthogonal/polyline routing and
incremental routing, and its Router API exposes transactions, shape movement and
connector deletion. The repository README identifies LGPL-2.1-or-later licensing and
commercial licensing. No maintained distribution/release candidate has been accepted
for Axiom. Adoption requires a pinned source revision, license/notice treatment,
Windows static build, WebAssembly build,
and ABI/performance qualification before adoption. Sources:

- https://www.adaptagrams.org/documentation/libavoid.html
- https://www.adaptagrams.org/documentation/classAvoid_1_1Router.html
- https://raw.githubusercontent.com/mjwybrow/adaptagrams/master/cola/libavoid/README
- https://raw.githubusercontent.com/mjwybrow/adaptagrams/master/cola/libavoid/router.h

ELK/elkjs is suitable for graph layout experiments and Web Worker operation. Its own
README describes a JavaScript graph layout engine, layered layout with ports, and
optional Web Worker execution; it is not a renderer or shared C++ connector service.
Its EPL-2.0 license and separate JavaScript/WASM path make it a poor first shared
Runtime dependency. Source: https://raw.githubusercontent.com/kieler/elkjs/master/README.md

Excalidraw is useful as an MIT-licensed algorithm reference for elbow routing, grid
search, fixed segments, snapping and path simplification. It is TypeScript and must
not become the Windows/Web Runtime implementation. Source:
https://raw.githubusercontent.com/excalidraw/excalidraw/master/LICENSE and
https://raw.githubusercontent.com/excalidraw/excalidraw/master/packages/element/src/elbowArrow.ts

tldraw demonstrates normalized-anchor binding and attached terminals, but its current
license is product-restricted. It can inform behavior only; no code/dependency reuse.
Source: https://raw.githubusercontent.com/tldraw/tldraw/main/LICENSE.md

## Performance test matrix

If adopted for an experiment, run the same fixed route corpus through each candidate:

| Corpus | Obstacles | Connectors | Mutation | Metrics |
|---|---:|---:|---|---|
| small | 100 | 20 | one target move | p50/p95, route validity |
| medium | 1,000 | 200 | 10% target moves | affected count, cache hit, p95 |
| large | 10,000 | 1,000 | continuous drag | frame budget, retries, cancellation |

Required invariants: deterministic route for identical revisions, no obstacle
intersection except permitted endpoint gaps, bounded work per drag sample, no
canonical generation change from a derived-only route refresh, and matching route
topology and coordinates under an accepted tolerance on Windows/Web for the same
input corpus. Numeric budgets and precise obstacle/escape rules
remain open; these measurements do not yet establish performance feasibility.
