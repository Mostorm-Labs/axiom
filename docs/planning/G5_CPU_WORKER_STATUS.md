# G5 independent CPU Worker development checkpoint

Current development branch: `codex/g5-cpu-worker`.

This checkpoint preserves the shared C++ exact-path implementation compiled to
an isolated CPU-only WASM Worker. JavaScript transports jobs and results; the
existing CanvasEditingRuntime owns pending erase intents and commit order, and
OperationEngine remains the only canonical mutation owner.

Implemented candidate capabilities:

- Partial-erase Up freezes confirmed world samples and ends the contact before
  expensive path computation. Subsequent partial erasing can be admitted.
- The existing Runtime queue confirms and atomically commits erases in order.
- Render publishes fully validated finished paths and reuses them for subsequent
  historical queries. Full geometry comparison, bounded leases and critical pins
  preserve correctness across cache and presentation transitions.
- The internal `AXIOM_CPU_PENDING_ERASE=ON` build connects the Worker. The default
  is OFF. Navigation has a pending-feedback path, but its full browser acceptance
  is incomplete.

The complete background-computation plan is not finished. Creation and dragging
of shape, sticky, table and image objects during pending erase are not queued
with immediate feedback yet. The candidate explicitly rejects competing canonical
mutations as Busy while the erase queue is pending. This restriction does not
establish the requested all-object interaction behavior.

## Measured limitations

The frozen CWH browser run retained the original exact path semantics:

| Case | Ordinary build | Candidate build |
| --- | ---: | ---: |
| 1600-segment Up | 3880.9 ms | 0.4 ms |
| 1600-segment Up to settled canonical | 5028.7 ms | 7178.6 ms |
| 7200-segment Up to settled canonical | Not sampled | 52202.7 ms |

The candidate completed both erases and qualified presentation in the 7200 case,
with zero main-thread new-query and historical full unions. About 44.7 seconds
were still spent constructing 16 distinct local query paths in the Worker;
canonical commit work also remains synchronous. Moving computation off the main
thread did not reduce that total calculation work.

The cold snapshot check exposed an existing codec boundary: the resulting
snapshot encoded to 35,456,391 bytes, but decoding rejected it against the
33,554,432-byte limit. This checkpoint does not change Semantic limits or schemas.

The run is BLOCKED. Pending feedback/navigation, actual browser fault recovery,
strict independent pixel qualification and complete snapshot/Undo/Redo closure
remain outstanding. Native and focused SDK checks are scoped evidence and do not
make the overall performance or interaction milestone pass.

## Local candidate demo

The user-facing candidate is a copy of the exact tested candidate JS, WASM,
Worker and resources; only its HTML title distinguishes it. It uses the existing
SDK Demo and public root exports. No SDK release, version change or new tag is
part of this checkpoint. The publication record is under
`.aegis/results/G5-CPUW-publication-20261112-r1/`.

Rebuild with the accepted locked dependency environment and
`-DAXIOM_CPU_PENDING_ERASE=ON -DAXIOM_RENDER_MEMORY_PROFILE=DESKTOP`.
The candidate path-cache resident upper bound uses the existing profile's
`min(maxEntryBytes, maxBytes / 4)` (64 MiB on desktop). Default and explicitly
requested 8 MiB paths keep their original limit. The synchronous construction
and optional draw-plan limits remain 8 MiB; the fixed Worker heap remains 64 MiB
and the shared desktop budget remains 512 MiB. The earlier CW1/CW2 wording in
`sdk/web/README.md` describes those slices before this CWH resident-policy change.

Frozen source, SDK, numerical and failure records remain in their original
namespaces. Compact text evidence and their complete manifests are published;
bulk SDK assets, raw pixels, binary job buffers and local fixtures stay on disk
at hash-bound paths. No historical failure report is rewritten to success.
