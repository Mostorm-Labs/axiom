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

Web display continuity correction: Shell chrome overlays the mount rather than
participating in its layout. Continuous mount resize observations are coalesced
for 120 ms; drawing layers keep their previous logical pixel size and top-left
origin during that interval. The next pointer Down flushes pending metrics before
the shared collector translates coordinates. Actual resize/rebind still follows
the Runtime-owned paired provider transaction and generation validation, with no
implicit camera change. Retained pixels are disposable display feedback until a
qualified presentation of the new binding. Close cancels pending resize callbacks
and restores the mount's previous inline overflow. No public API or semantic schema
change is introduced.


Image export extension (preview.4): `exportImage` is a queued read-only capability;
it does not call text finish or schedule live presentation. Requests/results and
limits are defined in `image-export-v1-contract.json` and `sdk/web/README.md`.
The Runtime owns canonical frame preparation and the shared renderer. The root
facade returns an owned byte copy and value metadata only. Demo Blob/download
behavior remains in the Shell. This extension adds no canonical operation or
consumer-visible rendering owner and does not retroactively change published
preview assets.


Object clipboard v1 and live partial erase increment: see `../object-clipboard-and-live-erase-v1.json` and `sdk/web/README.md`. Root JS synchronous Copy returns owned opaque bytes and an owner-bound full Stamp; queued Cut/Duplicate/Paste retain the full captured basis. The shared Runtime owns identity remapping, hierarchy, resource validation, atomic History and publication; the Demo owns OS clipboard/key translation only. No Semantic Operation schema or conflict protocol changes.


Version identity increment (0.4.4): the existing read-only `version` Session command
returns `sdkVersion` as X.Y.Z.Build, `sdkSemver` as X.Y.Z and lossless numeric
`buildNumber` (0 for local builds, positive for release builds). Root JS exports
SDK_VERSION, SDK_SEMVER and SDK_BUILD_NUMBER; generated wrappers/declarations and
engine must match. This adds value metadata without exposing Runtime owners,
changing DLL exports, buffer lifetime, owner-thread/reentrancy or receipt semantics.
See sdk/VERSIONING.md for immutable npm/tag and build lifecycle rules.

CPU Worker candidate ordered editing (local unpublished development):
`RuntimeState.pending` is an optional owned read-only value snapshot. It contains
at most eight intents with decimal-string runtime/lifetime/serial tokens, kind,
phase, object IDs and dependency serials, plus pending selection bounds. It
contains no ObjectRecord, Scene, path, native handle or mutable store. The normal
OFF build may omit it or return an empty intent list. Canonical `objects`,
`generation`, `lastCommit`, DataPort packets and existing typed object reads never
include an uncommitted created object; canonical selection reads are empty or
unavailable for that pending selection. `pending.selection` describes its visible
HUD. Selection and presentation revisions do not assert canonical commit.

Mutation promises remain pending until the owning Runtime applies the actual
ordered intent or reports its terminal failure. Their short owner admission is
serialized independently of waiting for commit. Pointer, wheel, navigation and
frame/Worker pumps continue while these promises wait. Undo/Redo are ordered
History actions; admission establishes a temporary dependency barrier that returns
Busy for subsequent canonical-dependent writes until the History action finishes.
Read, navigation and tool selection stay available. Remote/replay return Busy
without applying while local intents are pending; callers retry the same IDs.

Image import preserves existing manifest-then-Insert semantics: registration
failure inserts nothing; Insert failure after successful manifest registration may
leave an unreferenced resource and rejects the promise. No cross-Operation atomic
rollback is claimed. Snapshot has an independent 128 MiB whole-message envelope;
encode and decode share it, while Operation 32 MiB, ObjectRecord 16 MiB and existing
collection limits remain. Unsupported or oversized capture does not return a
successful restorable document. This is not a 1M-object scale acceptance.

本地未发布编译的逻辑版本保持 `0.4.3`，Build 为 `0`；root `SDK_VERSION`、`Runtime.version` 和 `readBuildInfo().sdkVersion` 显示 `0.4.3.0`，`readBuildInfo().version` 与 npm `version` 保持三段，npm `buildNumber` 为数值 `0`。此元数据不构成发布或正式 Gate 通过。

Release integration 0.5.0: the preceding 0.4.3.0 candidate build note is historical.
Current SDK identity follows sdk/VERSIONING.md and the 0.4.4 version metadata contract.
Private pending CPU editing remains build-profile controlled; the 0.5.0 Web
release explicitly enables it. Native platform release profiles retain OFF. PendingLocalState is an additive read-only DTO.
