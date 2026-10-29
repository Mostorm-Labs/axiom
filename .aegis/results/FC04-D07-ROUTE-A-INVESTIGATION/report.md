# FC04 D07 Route A — Semantic SDK exception-safety investigation v0.1

## Decision

**SDK_EXCEPTION_SAFETY_FIX_REQUIRES_UPSTREAM_RELEASE**

The failure is reproducible in the exact locked Windows consumer and cannot be made safe by an Axiom Runtime catch, descriptor prewarming, fault relocation, or an ad-hoc SDK replacement. A bounded correction is technically identifiable, but it must change and retest the Abseil/Protobuf producer inputs and publish a new immutable Semantic SDK release set before Axiom can consume it.

## Exact baseline and inputs

- Repository: `Mostorm-Labs/axiom`; branch `codex/fc04-d07-selection-audit`; starting/result HEAD `3615a38422a103024bc23924c570e3c585656670`; feature ancestor `c7f874395d55952e9d5288e2a336a21747f37f11`.
- P32-002 report, owner handoff, source lock and debugger logs are copied byte-for-byte under `raw/`.
- `deps.lock.json` SHA-256: `11f19f907af2b489951773d4ccec6e766123b1e0b7b11faa4ce6d9f443906e4b`.
- Locked Protobuf: v36.0, source archive SHA-256 `399931c793f4ac6db81045b00b06dd07c877b48aeecf36c797f65c541fb533e7`.
- Locked Abseil: 20250512.1, source archive SHA-256 `9b7a064305e9fd94d124ffa6cc358592eb42b5da588fb4e07d09254aa40086db`.
- Semantic SDK lock SHA-256: `edc69603f4ab43b045abbd67792795ca98273844a71eddd59710dd9bebd3e514`; release set `14e3d492c9b7f9705dcb89df8dd3f8abbddb7d1bc026bf3084de45bdc317d5ea`; release tag `semantic-sdk-v2-14e3d492c9b7f970`.
- Installed `absl/base/call_once.h` SHA-256: `9f65cf0aa3632a53eb3c162ebe1badc89b86d3d33fab3bc05da93853fa911e4f`. Installed Protobuf headers are also hashed in `raw/lock-hashes.json`.
- Upstream exact source references are pinned in `raw/upstream-links.json`; the local source archive download was network-blocked, so no local source archive was introduced or modified.

## Root-cause evidence

The fresh Windows run of the existing real allocation sweep is `raw/fresh-sweep.log`: attempts 0–101 are printed and CTest times out at 15 seconds with exit 8. The P32-002 debugger fault stack shows the injected `operator new` failure in `ArenaStringPtr::Mutable`, called by `AssignDescriptorsOnceInnerCall`, then Abseil `CallOnceImpl`, Protobuf metadata and `SemanticCodec::measureProtobufOperation`. The second debugger dump shows the same descriptor path waiting in `SpinLockWait` inside `CallOnceImpl`; it has two worker threads waiting in the process. The canonical Runtime/Scene/History state remains precommit at the observed fault, but the process is no longer safely retryable.

The exact Abseil source has a state machine `init → running/waiter → done`; `std::invoke` is followed directly by the exchange to done and waiter wake. There is no exception handler that restores `init` or wakes waiters. This is the direct mechanism behind the observed permanent running state.

The exact Protobuf v36.0 source calls `absl::call_once(*table->once, AssignDescriptorsOnceInnerCall)`. In `AddDescriptors`, Protobuf checks `table->is_initialized`, sets it to `true`, and only then enters `AddDescriptorsImpl`. That implementation recursively registers dependencies, adds the file to the generated pool, registers the generated message factory, registers extensions, and then `AssignMessageDescriptor` mutates class data and allocates `Reflection` objects. An OOM after the early flag or any of those mutations cannot be proven safe to retry by resetting Abseil alone. The upstream lines are retained in `raw/reflection-snippet.txt` and linked to the exact v36.0 files.

## Why the fix must be upstream

A Runtime-only catch cannot access or atomically restore Abseil’s private once control, Protobuf’s `DescriptorTable::is_initialized`, generated-pool entries, message-factory registrations, extension registrations, class-data descriptor/reflection pointers, or shutdown-owned allocations. Resetting only the once flag risks duplicate or partial registration. Bypassing `measureProtobufOperation` changes the frozen payload budget oracle and cannot establish byte-identical normal encoding. Prewarming removes the real failure boundary and is forbidden.

The correction therefore needs a coordinated upstream patch: exception-aware once state transition with safe waiter wake/retry semantics, plus Protobuf registration/descriptor initialization staging or rollback so a failed initializer leaves a clean retry basis. The patch must be proven under concurrency and real allocation faults, not just unit-level exception catching.

## Authorized patch scope after a new P31

Abseil owner: `absl/base/call_once.h` and its internal spinlock/wait implementation/tests. Protobuf owner: `src/google/protobuf/generated_message_reflection.cc`, `src/google/protobuf/message.cc` if metadata flow changes, the descriptor-pool/message-factory/registration owners identified by upstream tests, and new generated reflection/descriptor exception-safety tests. Axiom producer owners: `tools/semantic/build_runtime.py`, `tools/semantic/contract.py`, `tools/semantic/classify_producer_changes.py`, `tools/semantic/aggregate.py`, `tools/semantic/publish_release.py`, `tools/semantic/reuse_release.py`, and `.github/workflows/semantic-sdk-release.yml`. `runtime/semantic/src/codec.cpp` remains outside this investigation and requires a separate P31 decision if the new SDK contract needs an explicit stable failure mapping.

No files in this investigation changed; the source/SDK/worktree remains clean. `raw/decision.json` contains the machine-readable owner map and test plan.

## Verification required before SDK publication

1. Inject OOM inside the once initializer and prove the first call returns a controlled failure or safely retries.
2. Run one initializer with at least two concurrent waiters; every caller must terminate and no waiter may deadlock.
3. Observe descriptor pool, `is_initialized`, generated message factory, extension registry, default-instance and reflection ownership around each failure; a failed attempt must leave a clean retry basis.
4. Retry the same descriptor and compare normal encode/measure bytes to a no-fault control byte-for-byte.
5. Run the complete real allocation sweep at the original boundary; no timeout, deadlock, or canonical mutation is allowed.
6. Run Axiom Runtime/Scene/History containment around every failure and preserve truthful receipts.

The complete matrix and forbidden shortcuts are in `raw/verification-design.json`.

## Release path

Version the source patch and producer contract, verify the exact source archives and toolchain, build Abseil then Protobuf through `tools/semantic/build_runtime.py`, run SDK unit/reproducibility/smoke and the matrix above, publish an immutable release set, and open the existing lock-only candidate PR. Only after the candidate lock passes its identity/hash checks may `semantic-sdk.lock.json` change. Resolve the candidate through `setup_build_environment.py` and its offline mode, then rebuild the Axiom consumer and rerun the blocked D07 obligations. This investigation does not authorize that package or implementation.

## Status boundaries

This result does not claim D07-A/B, DEV, P34, Gate, PHYS, Selection-160 current-revision, or Web/WASM completion. It routes to control review for a versioned SDK correction P31.
