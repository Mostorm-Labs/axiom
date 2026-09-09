# GT-G1-07 P31 Task Package v0.2

Status: BINDING REFRESHED / READY / P32 NOT AUTHORIZED

- package_id: `GT-G1-07-P31-v0.2`
- package_ref: `notion://3d64c57a-590c-810e-93d9-ff721caceb8a/GT-G1-07-P31-v0.2`
- task_id: `GT-G1-07`
- purpose: Semantic Replay Inspector
- repository: `Mostorm-Labs/axiom`
- canonical branch: `main`
- task anchor: `8d3570c7c5df6f718b444b102b8b01042c2af3dd`, relation `ancestor`
- P32 authorized: `false`
- P34 started: `false`
- GT-G1-08 authorized: `false`

## Exact Verification bindings

These bindings are normative for `PackageBindingPreflight` and `EvidenceContractPreflight` and were formalized by `GT-G1-07 P20 Targeted Verification Reconciliation v0.1`.

- verification_spec_id: `GT-G1-07-VS-v0.1`
- obligation_set_id: `GT-G1-07-OBL-v0.1`
- trusted_basis_id: `GT-G1-07-TB-v0.1`
- scope_contract_id: `GT-G1-07-SCOPE-v0.1`
- acceptance_oracle_id: `GT-G1-07-ORACLE-v0.1`
- evidence_compilation_contract_id: `GT-G1-07-ECC-v0.1`
- verification authority ref: `notion://3d64c57a-590c-81e2-8e9d-dbdf085e7618/GT-G1-07-P20-v0.1`

This binding refresh does not alter `T07-R01` through `T07-R12`, source scope, the exact 12-file evidence inventory, the `EXECUTION_CLOSURE_CONTRACT`, or the same `PRIMARY_GATE_01` occurrence.

## Rebind facts

GT-G1-02R-INGRESS is closed and integrated.

- source ref: `734a7b7dacf4087c9305d3834c92da51cb22f3ba`
- evidence ref: `fcab1b50c664f0e3238c133eca12465ac4668501`
- main integration ref: `8d3570c7c5df6f718b444b102b8b01042c2af3dd`

The historical package `GT-G1-07-P31-v0.1` is non-executable. The historical source asset ref `ec8795c9bfa3104faf7af2568589c020933ddd3d` may be used only as selective source reference. Historical evidence/materialized refs `58870f6b89ed1a0a02ef9ee4fe03f55df7fe5ed9` and `36e21e8f51b02ff15d168eebc975a79120031720` must not be used as source bases or cherry-picked into the new source lane.

## Execution branch policy

The future execution branch is `codex/gt-g1-07-rebind-v2` and must start from this package materialization commit. Whole-branch merge/rebase of either historical GT-G1-07 branch is forbidden. Valid old implementation assets may be selectively ported and reconciled against current `main`.

## Rebound trace ingress

For protobuf-enabled trace file / CLI execution:

`operations[].bytesHex -> SemanticCodec::decodeProtobufOperation -> normalizeOperation -> typed ReplayTrace.operations -> OperationEngine / ReplayCoordinator`

The historical `SemanticCodec::decodeOperation` trace-file binding is superseded and must not remain as a second operation-byte ingress path.

Protobuf-disabled builds may exercise typed inspector models directly; non-empty protobuf-byte CLI decoding is not applicable and must fail/classify consistently rather than silently using the historical custom-wire decoder.

## Locked Semantic SDK provider

Current lock:

- format: `axiom-semantic-sdk-lock-v2`
- releaseSetId: `14e3d492c9b7f9705dcb89df8dd3f8abbddb7d1bc026bf3084de45bdc317d5ea`
- releaseTag: `semantic-sdk-v2-14e3d492c9b7f970`
- indexSha256: `f207d71d3ba0614755ff2a4533581dd227dd369219e090a95478819ff8a79bc5`

Hosted verification must use current `tools/setup_build_environment.py` wiring, `AXIOM_PROTOC`, and `AXIOM_SEMANTIC_RUNTIME_ROOT` / `CMAKE_PREFIX_PATH`. Reintroducing `.deps/protobuf` as primary provider, hard-coded `Protobuf_DIR` / absl / utf8-range paths, or ad-hoc dependency acquisition is forbidden.

## Authorized source/control scope

CREATE or selectively port/reconcile:

- `runtime/semantic/tools/g1_07_replay_inspector.hpp`
- `runtime/semantic/tools/g1_07_replay_inspector.cpp`
- `runtime/semantic/tools/g1_07_replay_corpus.hpp`
- `runtime/semantic/tools/g1_07_replay_corpus.cpp`
- `runtime/semantic/tools/g1_07_replay_inspector_main.cpp`
- `runtime/semantic/tests/g1_07_replay_inspector_test.cpp`
- `runtime/semantic/tests/g1_07_replay_inspector_cli_test.cpp`
- `.github/workflows/g1-07-exact-source.yml`
- `verification/tools/generate_g1_07_evidence.mjs`
- `verification/packages/semantic-conformance-cli/test/g1-07-exact-source-evidence.test.mjs`

MODIFY only:

- `runtime/semantic/tools/CMakeLists.txt`
- `runtime/semantic/tests/CMakeLists.txt`
- `runtime/semantic/include/canvas/semantic/codec.hpp` — comment/contract correction only; no signature or semantic change

All other paths are read-only unless execution returns `BLOCKED_SCOPE`.

## Required implementation delta

1. Selectively port/reconcile valid Replay Inspector implementation, tests, corpus, CLI and evidence tooling from `ec8795...`.
2. Replace old trace operation ingress with `decodeProtobufOperation -> normalizeOperation`.
3. Add a non-empty canonical protobuf trace representative proving nested semantic values survive into the typed operation consumed by the inspector.
4. Preserve Reference/Indexed parity, deterministic cursor/step/seek/run behavior, object/commit/ChangeSet/generation observations, G1-06 projection correctness oracle and diagnostic digest.
5. Preserve process-level CLI proof: real executable invocation, deterministic repeated stdout/stderr, one JSON document + newline, and exit 0/2/3 independent assertions.
6. Rewrite exact-source workflow to current locked SDK wiring.
7. Evidence generation must consume exact machine/provider facts rather than invent PASS facts.
8. Correct stale `decodeProtobufOperation` comment in `codec.hpp` only.

## Blocking verification

- T07-R01: protobuf trace ingress uses `decodeProtobufOperation -> normalizeOperation`; non-empty representative preserves nested values.
- T07-R02: Reference/Indexed parity for step/seek/run.
- T07-R03: deterministic reconstruction and projection equivalence.
- T07-R04: deterministic operation/object/commit/ChangeSet/generation observations.
- T07-R05: process-level CLI contract and exit 0/2/3 coverage.
- T07-R06: negative/fail-closed matrix.
- T07-R07: protobuf ON full Semantic regression, zero unexpected failures.
- T07-R08: protobuf OFF full Semantic regression, zero unexpected failures; protobuf-byte cases explicitly N/A/runtime-unavailable.
- T07-R09: evidence generator fail-closed contract tests.
- T07-R10: `git diff --check` and exact source-scope inventory.
- T07-R11: fresh clean-checkout reproduction at final source SHA.
- T07-R12: hosted exact-source workflow PASS with exact run/attempt/job/artifact/tested-SHA binding.

## Final evidence contract

Exactly twelve files under `verification/evidence/gates/G1/<FINAL_SOURCE_REF>/GT-G1-07/`:

- `G1-07-PLAN.json`
- `G1-07-TRACE.json`
- `G1-07-STEP.json`
- `G1-07-SEEK.json`
- `G1-07-OBSERVATION.json`
- `G1-07-PROJECTION.json`
- `G1-07-DETERMINISM.json`
- `G1-07-NEGATIVE.json`
- `G1-07-CLI.txt`
- `G1-07-CTEST.txt`
- `G1-07-DIFF.json`
- `G1-07-GATE-MANIFEST.json`

The generator must bind actual source/task/package refs, changed-path inventory, exact commands/results, CLI process facts, provider run/attempt/job/artifact/tested SHA, and evidence hashes. It must fail closed on pre-existing final namespace and invalid source/materialized provenance.

The final materialized ref must be exactly one evidence-only descendant of the immutable final source ref; no source/config/test path may change in that descendant.

## EXECUTION_CLOSURE_CONTRACT

```yaml
EXECUTION_CLOSURE_CONTRACT:
  implementation:
    required_changes:
      - selective clean port of accepted replay-inspector source assets
      - decodeProtobufOperation + normalizeOperation trace ingress
      - current locked Semantic SDK provider wiring
      - codec.hpp stale comment correction only
      - machine-bound final evidence tooling
    forbidden_changes:
      - production semantic include/src changes except codec.hpp comment only
      - schema/proto/tag changes
      - second decoder/validator/replay/apply engine
      - direct ObjectStore mutation semantics
      - replay/cursor/seek semantic redesign
      - public ABI or stable product CLI commitment
      - whole merge/rebase of historical GT-G1-07 branches
      - historical evidence commit reuse as source
      - GT-G1-08 or G2 work

  tests:
    required: [T07-R01, T07-R02, T07-R03, T07-R04, T07-R05, T07-R06, T07-R07, T07-R08, T07-R09, T07-R10, T07-R11]

  hosted_verification:
    required: [T07-R12]
    optional: []

  evidence:
    blocking:
      - exact final source ref
      - exact 12-file evidence inventory
      - one-commit evidence-only materialized ref
      - bound local/clean-checkout machine facts
      - bound hosted run/attempt/job/artifact/tested SHA
    corroborative:
      - historical ec8795 source behavior
      - historical P36 evidence

  terminal_success:
    all_of:
      - all required implementation changes complete
      - all required tests/oracles pass
      - fresh clean checkout passes
      - hosted exact-source verification passes
      - final 12-file evidence package validates
      - reviewer-accessible evidence-only materialized ref exists

  terminal_blockers:
    explicit_classes:
      - AUTHORITY_CONFLICT
      - BLOCKED_SCOPE
      - BLOCKED_REPOSITORY_IDENTITY
      - MISSING_REQUIRED_INPUT
      - ENVIRONMENT_BLOCKER
      - FROZEN_VERIFICATION_FAILURE
      - NEW_HIGH_IMPACT_FAILURE_MODE

  return_policy:
    continue_until_terminal_state: true
```

Execution must return exact final source ref, materialized ref, evidence input refs, hosted run/attempt/job/artifact/tested SHA, ancestry result, and changed-path inventory. It must not emit an official P34 PASS claim.
