# MR-10-05 Platform Corpus / Harness Materialization — Closure Audit v0.1

> Audit date: 2026-08-24
> Branch: `docs/notion-bridge-bootstrap`
> Layer: `10-verification`
> Status: **IN PROGRESS — PHASE A CORPUS NAMESPACE MATERIALIZED / TARGETED LOCAL PASS / CI PENDING**

## 1. Purpose

MR-10-05 turns the already-authoritative Platform verification design into repo-local executable corpus and harness artifacts.

It consumes, but does not redesign:

- 10-09 `Platform Scenario Seed Set + Harness Adapter Skeleton v0.1`;
- 10-10 `Platform Harness Execution Protocol + Fault Hook Contract v0.1`;
- 10-11 `Platform Harness Reference Runner + Protocol Test Vectors v0.1`;
- 10-12 `Platform Harness Implementation Plan + CI Job Wiring v0.1`;
- MR-10-04 closed core-six schemas;
- 08 Platform Contract physical realization status.

Source pages remain Freeze Candidate / `proposed-freeze`.

## 2. Non-negotiable corpus separation

Three independently governed corpora exist:

```text
60 semantic vectors
28 platform scenarios
56 harness protocol vectors
```

They are not aliases and must never be collapsed into one seed.

- semantic vectors prove canonical semantic/wire behavior;
- platform scenarios prove cross-platform observable contract;
- harness protocol vectors prove the Shared Runner itself interprets evidence correctly.

A protocol trusted-root failure invalidates platform evidence; a platform adapter cannot certify the runner that evaluates it.

## 3. MR-10-05 decomposition

### Phase A — Platform Corpus Namespace + Reference Resolution

Goal:

- lock `platform-seed-v0.1` to exactly 28 stable IDs;
- establish the repo-local platform corpus path;
- resolve `canonicalFixtureRef` and `SEMANTIC` step `fixtureRef` against the checked-in Semantic Golden case namespace;
- execute the deferred `META-PLATFORM-SCENARIO-MISSING-FIXTURE-REJECT` contract.

### Phase B — 28 Scenario Body Materialization

Materialize:

```text
verification/platform/v1/scenarios/<PLAT-...>/scenario.json
```

No Web/Windows/Android/Apple-specific expected semantic copies are allowed. Target differences remain `targets[].policy`, required capabilities, profile metadata and OPEN observations.

### Phase C — Normalized Trace Comparator Closure

Materialize comparator/oracle behavior for required/forbidden events, partial order, state/generation/ownership assertions and deterministic first divergence.

This phase executes the deferred:

```text
META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL
```

### Phase D — Harness Execution Protocol Schemas

Materialize 10-10 contracts:

```text
platform-harness-envelope
platform-harness-session
platform-fault-hook
platform-late-event-fence
```

Action dispatch, completion and Platform event remain distinct. Source leases and LateEventFence replace quiet-time correctness heuristics.

### Phase E — Protocol Trusted Root

Materialize 10-11:

```text
platform-protocol-suite
platform-protocol-vector
platform-protocol-meta-result
protocol-seed-v0.1   # exactly 56 stable vectors
```

Reference runner + scripted adapter must prove duplicate completion, stale epoch, source leak, fault-state and fence-race handling before real platform scenario evidence is trusted.

### Phase F — Platform Adapters / Hooks / CI Ordering

Implement verification-only platform host seams and deterministic fault hooks without entering Product public ABI.

Logical CI dependency remains:

```text
schema/workspace
  ↓
protocol package / runner
  ↓
56 protocol seed trusted root
  ↓
platform scenario corpus
  ↓
platform adapters / real hosts
  ↓
G3 aggregation
```

## 4. Phase A materialization

Materialized:

```text
verification/platform/v1/suites/platform-seed-v0.1.json
verification/conformance/coordinator/test_platform_corpus_contracts.py
verification/conformance/coordinator/platform_contracts.py
```

`platform-seed-v0.1.json` contains exactly the 28 stable IDs from 10-09, in source order:

```text
PLAT-CREATE-CANVAS-001
PLAT-HOST-ATTACH-001
PLAT-DOCUMENT-ATTACH-001
PLAT-CANONICAL-REPLAY-001
PLAT-METRICS-RESIZE-001
PLAT-METRICS-DPI-SCALE-001
PLAT-METRICS-ORIENTATION-001
PLAT-VISIBILITY-001
PLAT-APP-BACKGROUND-001
PLAT-APP-FOREGROUND-001
PLAT-CANVAS-SUSPEND-001
PLAT-CANVAS-RESUME-001
PLAT-SURFACE-LOST-001
PLAT-SURFACE-REBIND-001
PLAT-STALE-GENERATION-REJECT-001
PLAT-DEVICE-LOST-001
PLAT-DEVICE-RECOVER-001
PLAT-HOST-DETACH-REATTACH-001
PLAT-DATABRIDGE-NO-ECHO-001
PLAT-CALLBACK-NONREENTRANT-001
PLAT-INPUT-BATCH-NORMALIZED-001
PLAT-INPUT-HOTPATH-001
PLAT-ARC-PREVIEW-FALLBACK-001
PLAT-ARC-CANONICAL-HANDOFF-001
PLAT-SURFACE-OWNERSHIP-001
PLAT-DESTROY-CANVAS-001
PLAT-DESTROY-STALE-WORK-001
PLAT-RECOVERY-REPEATED-001
```

## 5. Semantic fixture namespace policy

Individual 60-case Semantic Golden bodies are still not all materialized. Therefore MR-10-05A does **not** pretend fixture files exist.

Current repo-local semantic case namespace is derived from checked-in suite manifests under:

```text
verification/golden/v1/suites/*.json
```

This is sufficient to answer whether a Platform Scenario references a published semantic case ID without fabricating expected content.

`semantic_case_ids()` resolves that namespace. `validate_platform_scenario_references()` checks:

- optional top-level `canonicalFixtureRef`;
- every `SEMANTIC` step `action.fixtureRef`;
- missing `fixtureRef` on a `SEMANTIC` step;
- referenced ID membership in the checked-in semantic suite namespace.

Bridge artifact paths and normalized pointer input fixtures are separate namespaces and are intentionally not conflated with semantic case IDs.

## 6. Deferred MR-10-04 meta-contract intake

The first deferred MR-10-04 cross-artifact check is now executable:

```text
META-PLATFORM-SCENARIO-MISSING-FIXTURE-REJECT
```

A scenario referencing an unpublished semantic fixture now fails closed with a stable validation error rather than being silently accepted.

The second deferred check remains Phase C:

```text
META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL
```

It requires actual normalized trace/comparator execution and is not faked during namespace materialization.

## 7. TDD / validation evidence

Test-first commit:

```text
a62aeb8a24d0c56b5789c4a357040dbea15523c6
```

Observed RED before production materialization:

```text
test_platform_seed_suite_is_materialized ... FAIL
AssertionError: False is not true : platform-seed-v0.1.json must be materialized
```

Then materialized:

- suite manifest commit `68ef69febc81f1767d4e15c1028dc11b9128e5fb`;
- semantic reference resolver commit `e839a20d8f1c5cb31ffd700e28bbba77130b695f`.

Targeted local GREEN verified:

- suite ID = `platform-seed-v0.1`;
- exactly 28 unique IDs;
- semantic namespace contains `REPLAY-MIXED-OPERATIONS-001` and `OP-SETTRANSFORMS-VALID-001`;
- known references pass;
- missing semantic fixture reference fails closed.

Targeted command result:

```text
MR-10-05A targeted corpus namespace checks: PASS
```

Full GitHub Actions evidence for these new commits is still pending; Phase A is therefore not yet recorded CI-closed.

## 8. Authority boundaries preserved

MR-10-05 must not:

- promote 08 OPEN physical realization;
- make adapters read `expected` or decide PASS/FAIL;
- create platform-specific semantic goldens;
- use arbitrary sleep as correctness synchronization;
- turn verification-only fault/source/fence objects into Product ABI;
- make current runner implementation behavior specification authority;
- let platform scenario evidence bypass the protocol trusted root once Phase E is materialized.

## 9. Current verdict

MR-10-04 is formally closed with green run `32742201698`.

MR-10-05 is **IN PROGRESS**.

Current state:

```text
Phase A corpus namespace            MATERIALIZED
28-ID stable suite                  MATERIALIZED
Semantic reference fail-close       MATERIALIZED
Missing-fixture meta-contract       MATERIALIZED
Targeted RED/GREEN                  PASS
Full CI evidence                    PENDING
28 scenario bodies                  NOT YET MATERIALIZED
Partial-order comparator             NOT YET MATERIALIZED
Harness protocol schemas             NOT YET MATERIALIZED
56 protocol vectors / runner         NOT YET MATERIALIZED
Real platform adapters/hooks         NOT YET MATERIALIZED
```

Next executable step after Phase A CI is **Phase B — 28 Scenario Body Materialization**.
