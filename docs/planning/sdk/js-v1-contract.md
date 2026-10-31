# JavaScript SDK API v1 implementation contract

The user prioritized a complete independently consumable JS SDK over the
remaining Windows sample. This versioned contract binds that implementation
to the existing public RuntimeComposition, RuntimeFacade and WebRuntimeAdapter
owners at `fc536ad0586d58834d7abec0caeee1ea177ab9d2` plus the existing local
SDK integration work. It does not change native ABI, semantic schema, dependency
locks, expected-value corpora, global registries or the deferred D07 OOM work.

The public source lives in `sdk/web/`, with an ES module factory, matching
TypeScript declarations and a modular WASM engine. Each instance owns a fresh
engine, document and input lifetime. Application HTML IDs, toolbar state, globals
and sample `_sdk_*` exports are not dependencies of the public module.

- All Id128 values retain 32 lowercase hexadecimal digits. All uint64 revisions,
  stamps and tokens use decimal strings. Unknown enums and malformed values
  reject; they do not select a fallback object or primary-only operation.
  The bridge enables the locked Semantic SDK's `-fexceptions` model at compile
  and link, so validation exceptions reach the JSON error boundary. Integer
  and byte-array decoding checks bounds before narrowing. This does not qualify
  the deferred global allocation-failure sweep.
- Commands serialize on the JS owner thread and inspect the existing Runtime
  receipts. Admission is not application or presentation. A resolved mutation
  does not acknowledge storage or network durability.
- DOM pointer/composition facts reuse the existing collectors. Composition is
  committed only on native result facts. Timeout leaves a rejected/pending
  completion; no synthetic commit is created. Runtime owns geometry, layout,
  gestures, canonical objects, operation planning and History.
- Multiselection submissions carry the caller's captured complete stamp and
  explicit eligible list. Unknown permissions reject. Local single-user permission
  must be configured explicitly. Excluded objects and stale observations are
  preserved in readback and receipts.
- Snapshot capture uses externally supplied checkpoint revision/frontier. Restore
  creates a new owner and verifies the checkpoint binding; History is not restored.
  Remote operations go through the canonical remote owner and do not echo as local
  changes. Operation bytes delivered to JS are owned copies.
- Resource delivery retains the full binding token and validates bytes through the
  existing owner. Builtin font loading is startup-only. An external manifest is an
  explicit alternative; neither path may silently replace a live manifest.
- The application owns DOM placement, file/database/network storage, synchronization
  policy and durable-close coordination. SDK close removes input listeners and
  borrowed surface state only after Runtime close succeeds. Forced browser teardown
  cannot guarantee asynchronous composition or persistence completion.

Required proof: strict Emscripten build with locked SDKs; a freshly copied
outside-repository, JS-only browser consumer exercising the real engine, isolated
instances, input, readback, History, text, document/operation channels and rejection;
TypeScript positive/negative consumers; packaged assets with the same factory,
engine, wasm, data and collector modules. Physical IME/touch results and stable
release acceptance remain separate evidence. Do not claim stability from merely
enumerating declarations or a successful synthetic test name.


Image export extension (preview.4): `exportImage` is a queued read-only capability;
it does not call text finish or schedule live presentation. Requests/results and
limits are defined in `image-export-v1-contract.json` and `sdk/web/README.md`.
The Runtime owns canonical frame preparation and the shared renderer. The root
facade returns an owned byte copy and value metadata only. Demo Blob/download
behavior remains in the Shell. This extension adds no canonical operation or
consumer-visible rendering owner and does not retroactively change published
preview assets.
