# MR-10-05 Platform Corpus / Harness Materialization — Closure Audit v0.1

> Audit date: 2026-08-24
> Branch: `docs/notion-bridge-bootstrap`
> Layer: `10-verification`
> Status: **IN PROGRESS — PHASE A + PHASE B MATERIALIZED / PHASE B CI EVIDENCE PENDING**

## 1. Purpose

MR-10-05 turns the already-authoritative Platform verification design into repo-local executable corpus and harness artifacts. It consumes, but does not redesign, 10-09 through 10-12, the MR-10-04 closed core-six schemas, or 08 Platform physical-realization authority.

Source pages remain Freeze Candidate / `proposed-freeze`.

## 2. Non-negotiable corpus separation

Three independently governed corpora remain separate:

```text
60 semantic vectors
28 platform scenarios
56 harness protocol vectors
```

Semantic vectors prove canonical semantic/wire behavior. Platform scenarios prove cross-platform observable behavior. Protocol vectors prove the Shared Runner/harness itself. None may silently replace another.

## 3. MR-10-05 decomposition

### Phase A — Platform Corpus Namespace + Reference Resolution

- lock `platform-seed-v0.1` to exactly 28 stable IDs;
- resolve Platform semantic references against the checked-in Semantic Golden namespace;
- execute `META-PLATFORM-SCENARIO-MISSING-FIXTURE-REJECT`.

### Phase B — 28 Scenario Body Materialization

- materialize exactly one shared `scenario.json` per stable scenario ID;
- preserve 10-09 requirement status, requirement IDs, target policy and authoring recipe;
- make common Running-Canvas bootstrap explicit;
- reference the shared Semantic Golden instead of copying per-platform semantic expected truth;
- keep 08 OPEN physical realization in profile/open-observation channels only.

### Phase C — Normalized Trace Comparator Closure

Materialize required/forbidden event, partial-order, state/generation/ownership assertions and deterministic first-divergence evaluation. This phase executes `META-PLATFORM-PARTIALORDER-MISSING-EVENT-FAIL`.

### Phase D — Harness Execution Protocol Schemas

Materialize the 10-10 envelope/session/fault/fence contract family.

### Phase E — Protocol Trusted Root

Materialize the 56 stable protocol vectors, protocol meta-result and reference runner/scripted adapter.

### Phase F — Platform Adapters / Hooks / CI Ordering

Implement Web / Windows / Android / Apple verification adapters and deterministic verification-only hooks without entering Product public ABI.

## 4. Phase A materialization

Materialized:

```text
verification/platform/v1/suites/platform-seed-v0.1.json
verification/conformance/coordinator/test_platform_corpus_contracts.py
verification/conformance/coordinator/platform_contracts.py
```

`platform-seed-v0.1.json` contains exactly the 28 stable IDs from 10-09, in source order. `semantic_case_ids()` derives the current repo-local Semantic Golden namespace from checked-in suite manifests. `validate_platform_scenario_references()` fail-closes unpublished top-level `canonicalFixtureRef` and `SEMANTIC` step fixture references.

Phase A test-first commit: `a62aeb8a24d0c56b5789c4a357040dbea15523c6`.

Observed RED before materialization:

```text
test_platform_seed_suite_is_materialized ... FAIL
AssertionError: platform-seed-v0.1.json must be materialized
```

Targeted local GREEN later confirmed exact 28 IDs, namespace resolution, known reference acceptance and missing semantic fixture rejection.

## 5. Phase B authority mapping

The Phase B body set is owned by 10-09 `Platform Scenario Seed Set + Harness Adapter Skeleton v0.1`, with field vocabulary inherited from 10-08. The 28 matrix rows preserve their source status and requirement IDs exactly.

The three target shorthands are materialized as follows:

```text
ALL
  WEB / WINDOWS / ANDROID / APPLE = REQUIRED

ORIENTATION
  ANDROID / APPLE = REQUIRED
  WEB / WINDOWS = REQUIRED_WHEN_CAPABLE

NATIVE_ARC
  WINDOWS / ANDROID / APPLE = REQUIRED_WHEN_CAPABLE
  requires arc.preview
  WEB absent
```

No target policy implies that a physical backend/surface/bridge/thread/process choice is frozen.

## 6. Phase B repo layout

Materialized exactly one body for every suite ID:

```text
verification/platform/v1/scenarios/
  <PLAT-...>/
    scenario.json
```

There are no platform-specific authority copies such as:

```text
scenario.web.json
scenario.windows.json
scenario.android.json
scenario.apple.json
```

GitHub staging was intentionally used so the main authority branch did not expose a half-materialized corpus. The staging branch started at test-first commit:

```text
31178a1c6b79f0a8a4beb48e7393086e21bd2678
```

After all bodies were written, GitHub compare proved:

```text
status: ahead
ahead_by: 28
behind_by: 0
files changed: exactly 28
all files: added verification/platform/v1/scenarios/<ID>/scenario.json
```

The main branch was then fast-forwarded in one ref update to:

```text
0db1f91668c87cb8a1795abc01cdf8242a1251ae
```

## 7. Phase B authoring invariants

### 7.1 Shared semantic authority

The common non-trivial document fixture remains:

```text
REPLAY-MIXED-OPERATIONS-001
```

The DataBridge external-apply smoke references the existing semantic case:

```text
OP-SETTRANSFORMS-VALID-001
```

Platform scenarios do not contain Web/Windows/Android/Apple-specific semantic expected projections.

### 7.2 Explicit runtime bootstrap

10-09 PSS-04 is materialized: scenarios authored from a Running Canvas explicitly perform the required setup rather than relying on hidden harness state:

```text
CREATE_CANVAS
→ ATTACH_HOST
→ ATTACH_DOCUMENT
→ REPLAY_CANONICAL_FIXTURE
```

The Foundation cases and source-defined DataBridge / normalized-input special paths remain explicit exceptions rather than being forcibly rewritten into the common recipe.

### 7.3 OPEN realization preservation

`PLAT-SURFACE-REBIND-001` requests `SURFACE_PRIMITIVE` and `RENDER_BACKEND` only through `openObservations`. Arc cases encode capability/ownership requirements without selecting a platform-native implementation primitive.

### 7.4 No wall-clock correctness oracle

Input hot-path, Arc handoff and recovery bodies use event completion / partial-order assertions. No correctness `sleep`, fixed millisecond latency requirement or arbitrary quiet-time threshold was introduced.

## 8. Representative Phase B coverage

The 28 bodies now cover the source-defined groups:

- Foundation: create, host attach, document attach, canonical replay;
- Metrics: resize, DPI/scale, orientation, visibility;
- App/Canvas lifecycle: background, foreground, suspend, resume;
- Surface: lost, rebind, stale-generation rejection;
- Device: lost, recovery;
- Host lifecycle: detach/reattach;
- DataBridge/callback: no-echo, non-reentrant callback;
- Input: normalized batch, hot path;
- Arc/native ownership: preview fallback, canonical handoff, target ownership;
- Teardown/recovery: destroy, stale work, repeated recovery.

Freeze Candidate rows remain Freeze Candidate in the body files; materialization does not upgrade them.

## 9. Phase B TDD / validation evidence

Test-first body requirement commit:

```text
31178a1c6b79f0a8a4beb48e7393086e21bd2678
```

Observed RED before body materialization:

```text
test_phase_b_all_28_scenario_bodies_are_materialized_and_valid ... FAIL
AssertionError: missing materialized scenario body: PLAT-CREATE-CANVAS-001
```

The Phase B test set now checks:

- all 28 bodies exist;
- scenario ID matches directory/suite ID;
- JSON Schema + platform semantic validation;
- semantic fixture references;
- exact 10-09 status/requirement mapping;
- exact ALL / ORIENTATION / NATIVE_ARC target policy;
- no `scenario.<platform>.json` authority variants;
- explicit bootstrap on common Running scenarios.

Local Draft 2020-12 preflight against the current `platform-scenario.schema.json` reported:

```text
files 28 failed 0
```

Full GitHub Actions for the Phase B branch head remains the final Phase B close evidence. Until that run is green, Phase B is recorded **MATERIALIZED / CI EVIDENCE PENDING**, not CLOSED.

## 10. Deliberately deferred dependencies

Phase B does not fabricate evidence that belongs to later closures.

### Pointer input fixture bodies

10-09 names the following checked-in fixture paths but does not define their complete body contract in the Phase B authority text:

```text
pointer-pen-down-001.json
pointer-pen-move-coalesced-001.json
pointer-pen-up-001.json
```

Scenario references are materialized, but fixture content is not invented here.

### Destroy stale-work protocol-grade proof

`PLAT-DESTROY-STALE-WORK-001` materializes the PlatformScenario-expressible core lifecycle/fault path and declares the required source-lease/fence/source-attempt capabilities. Full `DROPPED_STALE_*`, SourceLease closure and LateEventFence proof remain owned by the 10-10/10-11 protocol layer and are not faked as new 10-08 state assertion kinds.

### Actual partial-order comparison

Scenario partial-order declarations are now materialized, but executing those oracles against normalized trace artifacts is Phase C.

## 11. Authority boundaries preserved

MR-10-05 still must not:

- promote 08 OPEN physical realization;
- let adapters read `expected` or decide PASS/FAIL;
- create per-platform semantic golden truth;
- use arbitrary sleep as correctness synchronization;
- expose verification fault/source/fence objects through Product public ABI;
- let implementation behavior become specification authority;
- let platform scenario evidence bypass the protocol trusted root after Phase E exists.

## 12. Current verdict

MR-10-04 is CLOSED / CI VERIFIED by run `32742201698`.

MR-10-05 remains **IN PROGRESS**:

```text
Phase A corpus namespace             MATERIALIZED
28-ID stable suite                   MATERIALIZED
Semantic fixture fail-close          MATERIALIZED
Phase B 28 scenario bodies           MATERIALIZED
Phase B schema preflight             PASS (28/28)
Phase B exact Git diff               PASS (28 added files only)
Phase B full CI evidence             PENDING
Phase C trace comparator             NOT YET MATERIALIZED
Phase D protocol schemas             NOT YET MATERIALIZED
Phase E 56 vectors/reference runner  NOT YET MATERIALIZED
Phase F real adapters/hooks          NOT YET MATERIALIZED
```

Next transition after Phase B CI is **Phase C — Normalized Trace Comparator + Partial-order Oracle**.
