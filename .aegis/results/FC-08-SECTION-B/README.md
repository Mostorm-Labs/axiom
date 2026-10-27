# FC-08 Section B — canonical operation implementation

Exact source result: `21f06e48f849812a82a2aaf389459f4c7ab6d76f`.
Materialized ref: `refs/heads/codex/fc08-section-operations`.
Source start: `fecd797a3f6a6fb9c27ac6d96f7edeafdfb9dc4c`.

The user's explicit next-slice instruction authorizes B through package
`FC-08-SECTION-B-v0.1` at `cbe90563cf45f16be54fb8b4f8b6d765de9203ac`,
SHA-256 `4978b9c012a7167538067ffeac16c2d7dd77bf4e3fa073388c78d86fc1f7a7a3`.
Both real-worktree package preflights used `--repo .`; authority/source locks,
accepted P12-P17/P20 and bootstrap-003/A evidence are preserved. No original P31
flag or global registry was rewritten. This package authorizes B only.

## Delivered code and ownership

- `container_operation.hpp`, `operation_payload.hpp`, `operation.hpp`: append-only
  typed17/18 variants; old1-16 identities/indexes retained.
- Private `container_operation_normalization/validation/mapping.*` and existing
  codec/normalizer/validator: accepted mandatory presence, closed oneof effects,
  unsigned-ID canonical arrays, duplicate rejection, explicit root semantics,
  finite/domain rules,65,535 keyed limits and full encoded forward32MiB admission.
  Typed model writers normalize; new raw wire requires canonical keyed arrays.
- `container_topology_plan.cpp` through existing `prepareApplyPlan`:17 exact
  expected content, geometry basis only when boundary changes;18 exact structural
  and complete-member guards, old/new ancestor coverage, Group/Section-only
  lifecycle effects, precise surviving frontier, staged final forest/capabilities,
  sibling-key uniqueness and connector-reference validity. No leaf create/delete,
  subtree cascade, JSON Patch or generic transaction payload was added.
- Existing `OperationEngine`, atomic store and AppliedOperationLedger owners:
  idempotency still precedes current-state guards. Real ChangeSet and ledger node
  allocation occur before publication. Indexed storage reserves only touched
  record map nodes/replacement values and affected parent sets, then publishes
  without allocation. Connector reverse index is preserved because endpoint
  content does not change. Independent reference storage stages a vector copy.
  Order-only changes publish Placement impact without a false Transform impact.
- Shared owner fixes required by actual fault tests: StagedObjectView registers
  allocating lookup acceleration in its constructor rather than noexcept reads;
  its derived state is cleaned/copied with its lifetime. New operation/Section/
  Group/fill oneof writers construct owned child messages before transferring
  to generated setters, preventing failed allocations from leaving an invalid
  selected branch. SDK/generated source/qualification checkpoint semantics were
  not patched. These changes retain legacy wire output and operation behavior.
- Source and test commands are recorded under `qualified-001/`; exact source
  hashes in its invocation record bind198 Semantic files. `terminal-result.json`
  lists changed source files and hashes.

## Actual verification at the source result

- MSVC19.51.36246 x64 Release, `/W4 /WX /MT`: configure0/build0.
- B117/117 PASS; A48/48 PASS;0 skipped.
- Full selected matrix550:549 PASS,1 FAIL,0 skipped; CTest exit8.
- The sole failure remains
  `StatefulKindApplicability.StateRuleMatrixMatchesOracleForAllKinds`, already
  independently reproduced against unmodified427580ae in A. That old Shape label
  expectation remains unchanged. No all-regressions-pass assertion is made.
- Python current descriptor/semantic contract:10 PASS,0 skipped, exit0.
- PackageBindingPreflight, EvidenceContractPreflight and scope/preservation audit:
  exit0. All302 frozen input/expected bindings and descriptor locks are preserved.
- Actual frozen B assertions:44 wire/model profiles plus29 stateful profiles in
  each of Reference/Indexed stores. Canonical inverse/redo fixtures are executed
  for T01-T04 with fresh fixture operation IDs and retry/metadata checks; these
  exercise real canonical operations, not EditorHistory construction.
- Eight initialized-runtime C++ operator-new failure sweeps cover Section,
  Section fill, Group and SetSectionContent in both stores. Actual rejected-site
  counts are in `allocation-observations.json`. Each failure checks canonical
  records, index consistency, ledger, generation/ordinal and absence of commit.
  Warm-up and allocator scope are explicit; no cold-start/malloc/arena guarantee
  or qualification-hook verdict follows from this test.

Exact argv/cwd/exits and XML/raw output are under `qualified-001`,
`qualified-python-001`, `qualified-preflight-001`, `qualified-scope-001`.
Compiler/raw version, compile commands, library/executable hashes are under
`qualified-environment-001`. Published locked Semantic SDK was resolved offline;
no dependency source rebuild or SDK lock change occurred. Hooks remain OFF.

## Preserved failures and limits

- `red-001` was a harness discovery failure caused by a temporary JSON lifetime;
  corrected before production mutation. `red-002` at exact68cdd636 has unchanged
  A production source:13 real wire cases,11 PASS/2 expected positive-op18 failures.
  It proves that unsupported baseline path; it is not a full17/18/302 product RED.
- `green-wire-001` caught an unreachable-return /WX error; no warnings were lowered.
- `green-semantic-001` includes a harness boundary error (W06 acceptance is
  stateful) and exploratory T06 redo assertions. W06 now invokes actual engine
  admission; model permutations use the real typed writer/planner entry.
- `fixture-limitations.json` preserves exact T06 input/expected hashes and shows
  its amended forward OrderOnlyEdit missing from shared redo. Frozen expected
  predicates are forward-only and all pass. The extra History assertion is not
  silently promoted or its carrier rewritten; it remains C Verification debt.
- Allocation diagnostics retain the generated-oneof exception crash, noexcept
  derived-state allocation termination and the cold sweep termination record.
  Final tests state an initialized-codec prerequisite; cold initialization remains
  unqualified. The attempted Group-mask sweep used an invalid legacy domain;
  masks remain forbidden on Group, and the final Group sweep uses legal properties.
- The first production all-record copy draft failed the unchanged
  IndexedProductionApplyDoesNotScanAllObjects test. It was replaced with actual
  touched-node/parent-set reservation, not a weakened or hidden scan assertion.
  `implementation-design-002.json` records the resolved owner flow.

Full graph validation in prepare is still correctness-first; this is not G5
performance qualification. No local inverse construction/History admission,
Section Scene/snapshot recovery or public Windows/Web UI has been delivered.
All full-runner, History, Scene, Chrome/WASM and physical verdicts remain NOT_RUN.
PH01-PH04 stay FC13; INT-01 remains FC04/FC10 beyond required prewrite rejection.
No DEV/P34/PHYS/release verdict is claimed.

Next smallest slice is C: actual F/U construction and precise bidirectional budget
admission plus precommit EditorHistory reservation through existing owners.
The T06 redo input mismatch needs Verification-owner resolution for its History
oracle. B authorization does not automatically authorize C-E.
