# Platform Scenario Seed Set + Harness Adapter Skeleton v0.1

> Source page: https://app.notion.com/p/3c44c57a590c8196ac7acd5215dcbf1d
> Snapshot date: 2026-08-24
> Source status: Freeze Candidate — G3 Platform Seed Corpus + Harness Adapter Contract

## Decisions

`platform-seed-v0.1` contains exactly 28 stable scenario IDs. Four platform families read the same scenario files. Platform differences are expressed only through target policy, required capabilities, profile and OPEN observations. Shared Scenario Runner owns schema validation, orchestration, WAIT, sequencing, capture, compare and result; platform adapter maps logical action and reports observed facts. Seed scenarios have no hidden runtime bootstrap.

## Seed IDs

1 PLAT-CREATE-CANVAS-001
2 PLAT-HOST-ATTACH-001
3 PLAT-DOCUMENT-ATTACH-001
4 PLAT-CANONICAL-REPLAY-001
5 PLAT-METRICS-RESIZE-001
6 PLAT-METRICS-DPI-SCALE-001
7 PLAT-METRICS-ORIENTATION-001
8 PLAT-VISIBILITY-001
9 PLAT-APP-BACKGROUND-001
10 PLAT-APP-FOREGROUND-001
11 PLAT-CANVAS-SUSPEND-001
12 PLAT-CANVAS-RESUME-001
13 PLAT-SURFACE-LOST-001
14 PLAT-SURFACE-REBIND-001
15 PLAT-STALE-GENERATION-REJECT-001
16 PLAT-DEVICE-LOST-001
17 PLAT-DEVICE-RECOVER-001
18 PLAT-HOST-DETACH-REATTACH-001
19 PLAT-DATABRIDGE-NO-ECHO-001
20 PLAT-CALLBACK-NONREENTRANT-001
21 PLAT-INPUT-BATCH-NORMALIZED-001
22 PLAT-INPUT-HOTPATH-001
23 PLAT-ARC-PREVIEW-FALLBACK-001
24 PLAT-ARC-CANONICAL-HANDOFF-001
25 PLAT-SURFACE-OWNERSHIP-001
26 PLAT-DESTROY-CANVAS-001
27 PLAT-DESTROY-STALE-WORK-001
28 PLAT-RECOVERY-REPEATED-001

## Target conventions

ALL = Web/Windows/Android/Apple REQUIRED. ORIENTATION = Android/Apple REQUIRED and Web/Windows REQUIRED_WHEN_CAPABLE. NATIVE_ARC = Windows/Android/Apple REQUIRED_WHEN_CAPABLE with arc.preview; Web baseline does not participate.

## Bootstrap

Default starts foreground, canvas not created, host detached, document available, surface unbound, device ready, Arc disabled. Scenarios requiring Running Canvas explicitly execute create→attach host→attach document→replay canonical fixture. Non-trivial semantic state references `REPLAY-MIXED-OPERATIONS-001`; DataBridge external-apply smoke references an existing semantic operation fixture rather than duplicating semantic expected.

## Important scenario oracles

Creation/attachment orthogonality; canonical replay exact semantic equality; metrics changes advance metrics generation but not semantic state; background/foreground and suspend/resume preserve document continuity; surface loss/rebind and device loss/recovery preserve semantic state; stale generation rejects; DataBridge external apply is no-echo; callback is non-reentrant; input batch/hot path obey normalized contract; Arc fallback preserves canonical presentation; canonical handoff obeys presentation proof; surface ownership is correct; destroy rejects stale work; repeated recovery remains coherent.

## Arc trace correction

To prove canonical handoff ordering, normalized trace includes `PREVIEW_CLEAR_REQUESTED` and `PREVIEW_CLEARED` in addition to CanonicalVisible acknowledgement. Required order is canonical qualified presentation → CanonicalVisible acknowledged → preview clear requested → preview cleared. This remains verification vocabulary, not production ABI.

## Adapter surface

Web Browser Driver, Windows Native Test Host, Android Instrumentation Host and Apple XCTest-style Host implement the same logical adapter surface and capability/profile reporting. Adapters never parse scenario expected and never decide PASS/FAIL.
