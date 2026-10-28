# FC04 D07 Route A: SDK Exception-Safety Correction

Status: CONTROL-ACCEPTED P31 SCOPE FOR P32 AFTER ALL PACKAGE PREFLIGHTS PASS. This authorizes a bounded SDK producer correction, not release promotion or SDK lock updates.

## Incident and locked inputs
- Feature baseline: codex/selection-summary-batch3 at c7f874395d55952e9d5288e2a336a21747f37f11.
- Execution branch: codex/fc04-d07-selection-audit.
- Incident result: 3615a38422a103024bc23924c570e3c585656670.
- Route A investigation result: 3d15645d54d44c47474b4b0dd6ca20ade1e3c633.
- Decision: SDK_EXCEPTION_SAFETY_FIX_REQUIRES_UPSTREAM_RELEASE.
- Protobuf v36.0 source archive SHA-256: 399931c793f4ac6db81045b00b06dd07c877b48aeecf36c797f65c541fb533e7.
- Abseil 20250512.1 source archive SHA-256: 9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db.
- Current accepted SDK release set: 14e3d492c9b7f9705dcb89df8dd3f8abbddb7d1bc026bf3084de45bdc317d5ea.

## Accepted technical direction
Use Route A: correct exception safety in the locked Abseil/Protobuf source inputs through the existing versioned Semantic SDK producer and release process. The observed failure is not safely repaired by a caller-side catch or by resetting private once state from Axiom Runtime.

The implementation must address both:
1. Abseil once-control liveness after an initializer throws: active callers return in a controlled way, waiters are released, and the once flag does not remain permanently running.
2. Protobuf descriptor-initialization integrity: descriptor tables, generated pool/message-factory registrations, default instances, extensions, class-data descriptor/reflection pointers and initializer-owned state are either rolled back to a clean retry basis or transitioned into an explicitly supported stable failure state. Resetting Abseil alone is insufficient.

The exact patch algorithm is a P32 design obligation. If the only plausible patch cannot preserve concurrency and descriptor-registration invariants, stop with NO_SAFE_BOUNDED_REMEDY.

## Patch delivery and identity
Store versioned source patches in the Axiom Semantic SDK producer tree. Apply them to verified SHA-locked source archives after safe extraction and before configuring/building Abseil and Protobuf. Patch application fails closed on source mismatch, missing hunk, duplicate application or unexpected already-patched state.

Add a canonical patch-set manifest and include its version/digest in runtime identity. A patch-set change must produce a different runtime package identity and must not reuse an unpatched cached asset. Keep upstream archive hashes in deps.lock.json unchanged unless the source archive itself must change; that requires a new authority review. Do not change semantic-sdk.lock.json in this task. Official host protoc assets are not the patched target runtime; do not modify host-tool identity unless proven necessary.

## Allowed Axiom producer and verification files
- New patch artifacts under tools/semantic/patches/abseil-20250512.1/ and tools/semantic/patches/protobuf-v36.0/, plus tools/semantic/patches/patch-set.json.
- tools/semantic/build_runtime.py, sources.py, contract.py, profile-v2.json, producer_cell.py, producer_plan.py, package_runtime.py, classify_producer_changes.py, aggregate.py, publish_release.py, reuse_release.py.
- .github/workflows/semantic-sdk-producer.yml, semantic-sdk-producer-contract.yml, semantic-sdk-release.yml only where required to integrate patch identity and producer tests.
- Relevant existing tests under tools/semantic/tests/, verification/tests/test_semantic_sdk_workflows.py, and new focused exception-safety tests.

## Conditional upstream patch targets
The source patches may modify only these owners, with exact paths recorded in the patch manifest before edits:
- Abseil 20250512.1: absl/base/call_once.h; associated internal spinlock/wait implementation only if proven necessary; corresponding call_once/waiter tests.
- Protobuf v36.0: src/google/protobuf/generated_message_reflection.cc; src/google/protobuf/message.cc if metadata flow changes; descriptor-pool, message-factory, registration/default-instance and reflection owner files only where real fault tests prove they are needed; corresponding descriptor/reflection tests.

The P32 design preflight must name exact upstream files and ownership. If a new owner/module or broader path is required, stop and return to P31. Preserve upstream license/provenance.

## Required semantics
- Injected std::bad_alloc at the real lazy descriptor initialization allocation returns a controlled failure; no hang or poisoned once state.
- Concurrent callers/waiters all terminate. The same descriptor can be retried safely, or the API returns a stable documented failure without partial descriptor state.
- A failed initialization leaves Protobuf state safe for retry or the selected stable-failure policy, including DescriptorTable::is_initialized, generated pool, message factory, extension/default-instance registration, reflection pointers and owned allocations.
- Normal retry yields byte-identical canonical measure/encode output versus no-fault control.
- Full real allocation sweep has no timeout/deadlock and no canonical/Scene/History mutation for rejected intents.
- No descriptor prewarming, catch-and-return while once state is poisoned, undocumented private-state reset from Axiom, fault relocation or weakened oracle.

## Release boundary
P32 may implement the versioned patch set and producer identity/tests, run qualification and produce a release candidate. It may not publish/promote the immutable SDK Release Set, alter semantic-sdk.lock.json, or replace the installed SDK. After P32 evidence is accepted by control, use the existing release workflow and lock-only candidate PR. Only after that release and lock candidate are verified may consumer update/qualification proceed.

## Exclusions
No changes to runtime/semantic/src/codec.cpp, Axiom canonical schema, public ABI, wire format, ObjectKind, FieldId, OperationKind, global registry or D06-B. No automatic merge to main. No Gate/DEV/PHYS/P34 PASS is implied.
