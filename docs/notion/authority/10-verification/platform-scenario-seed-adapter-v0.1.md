# Platform Scenario Seed Set + Harness Adapter Skeleton v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8196ac7acd5215dcbf1d
> Source page id: `3c44c57a-590c-8196-ac7a-cd5215dcbf1d`
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — G3 Platform Seed Corpus + Harness Adapter Contract
> Migration note: MR-10-04 treats the corrections in this later source as authoritative refinements to the core machine set defined by 10-08.

## Decisions

`platform-seed-v0.1` contains exactly 28 stable scenario IDs. Four platform families read the same scenario files. Platform differences are expressed only through target policy, required capabilities, profile and OPEN observations. Shared Scenario Runner owns schema validation, orchestration, WAIT, sequencing, capture, compare and result; platform adapter maps logical action and reports observed facts. Seed scenarios have no hidden runtime bootstrap.

## Pre-freeze corrections that affect MR-10-04

### `platform-suite.schema.json`

The preceding machine-contract page named the suite but did not yet give it a dedicated field-level manifest. This source closes that gap.

Schema identity:

```text
$id   = urn:auditoryworks:axiom:verification:platform-suite:v1
format = axiom-platform-suite-v1
formatVersion = 1
```

Minimum manifest:

```json
{
  "format": "axiom-platform-suite-v1",
  "formatVersion": 1,
  "id": "platform-seed-v0.1",
  "requiredScenarioFormatVersion": 1,
  "requiredRunnerProtocolVersion": 1,
  "scenarios": []
}
```

`scenarios[]` is a stable ordered scenario-ID list. Duplicate scenario IDs, missing referenced scenarios and required-format mismatch make the suite invalid. Those cross-file rules require semantic validation in addition to JSON Schema structure.

### Arc handoff trace correction

To prove the required handoff order, normalized Verification vocabulary adds:

```text
PREVIEW_CLEAR_REQUESTED
PREVIEW_CLEARED
```

The evidence chain is:

```text
qualified canonical presentation / canonical-visible proof
    ↓
CANONICAL_VISIBLE_ACKNOWLEDGED
    ↓
PREVIEW_CLEAR_REQUESTED
    ↓
PREVIEW_CLEARED
```

These are Verification-only normalized events; they do not freeze production callback/ABI names.

### Seed-required capability extensions

This source adds the Verification capability IDs needed by the 28-case seed:

```text
platform.state.capture
surface.ownership.capture
fault.stale_generation.inject
harness.completion_tokens
harness.source_lease_registry
harness.late_event_fence
harness.source_attempt_trace
```

They describe harness/runtime evidence capability, not Product feature flags.

## Repository logical layout

```text
verification/
├── schemas/
│   ├── platform-suite.schema.json
│   ├── platform-scenario.schema.json
│   ├── platform-profile.schema.json
│   ├── platform-trace.schema.json
│   ├── platform-observation.schema.json
│   └── platform-result.schema.json
├── platform/
│   └── v1/
│       ├── suites/platform-seed-v0.1.json
│       ├── scenarios/<PLAT-...>/scenario.json
│       └── fixtures/input/
└── platform_harness/
    ├── core/
    ├── adapters/web/
    ├── adapters/windows/
    ├── adapters/android/
    └── adapters/apple/
```

The six schema files above are the MR-10-04 **core platform machine contract set**. Later 10-10/10-11 execution-protocol schemas are a separate harness/protocol layer.

## Seed IDs — exactly 28

1. `PLAT-CREATE-CANVAS-001`
2. `PLAT-HOST-ATTACH-001`
3. `PLAT-DOCUMENT-ATTACH-001`
4. `PLAT-CANONICAL-REPLAY-001`
5. `PLAT-METRICS-RESIZE-001`
6. `PLAT-METRICS-DPI-SCALE-001`
7. `PLAT-METRICS-ORIENTATION-001`
8. `PLAT-VISIBILITY-001`
9. `PLAT-APP-BACKGROUND-001`
10. `PLAT-APP-FOREGROUND-001`
11. `PLAT-CANVAS-SUSPEND-001`
12. `PLAT-CANVAS-RESUME-001`
13. `PLAT-SURFACE-LOST-001`
14. `PLAT-SURFACE-REBIND-001`
15. `PLAT-STALE-GENERATION-REJECT-001`
16. `PLAT-DEVICE-LOST-001`
17. `PLAT-DEVICE-RECOVER-001`
18. `PLAT-HOST-DETACH-REATTACH-001`
19. `PLAT-DATABRIDGE-NO-ECHO-001`
20. `PLAT-CALLBACK-NONREENTRANT-001`
21. `PLAT-INPUT-BATCH-NORMALIZED-001`
22. `PLAT-INPUT-HOTPATH-001`
23. `PLAT-ARC-PREVIEW-FALLBACK-001`
24. `PLAT-ARC-CANONICAL-HANDOFF-001`
25. `PLAT-SURFACE-OWNERSHIP-001`
26. `PLAT-DESTROY-CANVAS-001`
27. `PLAT-DESTROY-STALE-WORK-001`
28. `PLAT-RECOVERY-REPEATED-001`

IDs are stable once published and are not renamed/reused to hide history.

## Target conventions

These are authoring shorthands, not scenario schema fields:

- `ALL` = Web/Windows/Android/Apple all REQUIRED.
- `ORIENTATION` = Android/Apple REQUIRED; Web/Windows REQUIRED_WHEN_CAPABLE.
- `NATIVE_ARC` = Windows/Android/Apple REQUIRED_WHEN_CAPABLE and requiring `arc.preview`; Web baseline does not participate.

`REQUIRED_WHEN_CAPABLE` is not ignore-failure: once a profile declares the capability, the scenario participates normally.

## Bootstrap

Default start state:

```text
app foreground
canvas not created
host detached
document available
surface unbound
device ready
Arc preview disabled
```

Scenarios requiring a Running Canvas explicitly perform create → attach host → attach document → replay canonical fixture. Platform adapters may not hide bootstrap steps.

Non-trivial canonical state references existing semantic golden such as `REPLAY-MIXED-OPERATIONS-001`; DataBridge external-apply smoke references an existing semantic operation fixture rather than cloning semantic expected.

## Seed oracle families

The 28 scenarios exercise the already-defined platform observable contract: create/attachment orthogonality; exact canonical replay; metrics without semantic mutation; foreground/background and suspend/resume continuity; surface/device loss and recovery; stale-generation rejection; DataBridge no-echo; callback non-reentrancy; normalized input; Arc fallback/handoff; surface ownership; destroy stale-work rejection; repeated recovery coherence.

The seed does not upgrade physical backend or host-topology choices from 08.

## Adapter boundary

Web Browser Driver, Windows Native Test Host, Android Instrumentation Host and Apple XCTest-style Host expose the same logical adapter responsibilities and capability/profile reporting.

Adapters:

- map logical scenario action to platform mechanism;
- report observed facts and artifacts;
- never parse `expected` as their own oracle;
- never decide PASS/FAIL;
- never create platform-specific scenario authority copies.

## MR-10-04 precedence note

For the six core schemas, use 10-08 as the base field-level contract and apply the corrections above from this later page. The 28 seed scenarios are downstream corpus materialization; the existence of OPEN platform physical decisions does not block core schema materialization because those decisions remain profile/observation metadata or explicit OPEN assertions rather than hidden expected values.
