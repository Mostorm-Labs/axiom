# FC-08 Verification Materialization Implementation Plan

> For agentic workers: use the existing Aegis implementation owner. Execute this fixed plan to terminal success or a named blocker; do not ask for another design approval at each step.

**Goal:** Materialize the already frozen FC08-P20-v0.1+CR01 into exact inputs, independent oracles, real baseline probes, and reproducible Windows/Web/headless profiles. Do not implement Section product behavior.

**Architecture:** Existing Semantic SDK and Axiom owners remain the DUT. Independent fixtures/oracles live outside production. New test programs may express expected future behavior, but absent production capabilities must be reported truthfully, never implemented by the test adapter.

**Tech Stack:** Python standard library for packaging; the repository-locked Semantic/Skia SDKs and existing CMake/C++/WASM toolchains for integration. No new SDK production.

**Spec:** accepted-inputs.zip contains the exact selected P23 wire/capability bindings, both acceptance receipts, effective 60-case CR01 catalog, ten obligations and seed fixtures. authority.lock.json pins further normative sources to accepted versions. Source acquisition is part of M01, not permission to invent missing requirements.

## Global constraints

- Read AGENTS.md. Resolve Semantic through tools/setup_build_environment.py, never ad-hoc system protobuf or rebuilding the published SDK. Do not change dependency locks.
- Preserve the original preparation package and all frozen source bytes. Outputs belong under tests/fc08/, tools/fc08/, and .aegis/materialized/FC-08/.
- Ten obligation families and sixty logical cases retain their frozen acceptance meaning. No product GREEN or physical PASS is a prerequisite or a result of this task.
- Use exact IDs and ancestor relation; preserve valid descendant work. No merge, force push, production mutation, or self-authorization of the later product task.

## Review focus

- Missing or duplicated input variants must not disappear behind an aggregate count.
- A trace with a transient wrong frame or stale selection must fail even when its last frame looks correct; an always-Unavailable positive trace is not success.
- Baseline unsupported capability is not an oracle PASS; environment failure is not semantic RED.
- A stale descriptor must not erase accepted fields such as ShapeContent.label=4.
- The same source tests on Linux/Node do not replace actual Windows/browser adapter discovery.

## Task M01: accepted sources and isolated descriptor

Files: .aegis/materialized/FC-08/sources/, source-index.json, baseline-descriptor/, proposed-descriptor/.

- [ ] Import the package seed archive with its manifest, preserving raw file bytes. Reconcile the accepted source pages named by authority.lock.json, exporting only accepted technical sections and the explicit CAP/E01/C01/C02/CR01 deltas. Keep capture metadata separate from normative text. Pin every export by source ID, accepted version/section, SHA-256 and acceptance event.
- [ ] Resolve the source-complete inherited schema at the actual starting commit, using the locked native host protoc. First generate a BASE-01-only descriptor in the isolated output tree without FC08 additions; then generate a separate overlay descriptor with the accepted section.proto/container_operations.proto and outer mappings. Do not write production schema/ or any historical descriptor.lock.
- [ ] Assert preservation of inherited 1..16 operations, Brush content10/11 and ShapeContent.label4; compare only the accepted FC08 delta (Section10/1/content12; operations17/18). Record exact command, protoc identity and source file hashes. Schema compilation failure is an environment or materialization failure, not expected R01.

## Task M02: complete independent inputs

Files: tests/fc08/corpus/, tests/fc08/reference/, .aegis/materialized/FC-08/variant-index.json.

- [ ] Expand every listed parameter variant for each of the sixty logical cases. Each variant has one exact input, an expected state/selection/trace or precise invariant, and a stable input/expected hash. Valid negatives violate exactly one intended condition.
- [ ] Produce typed operation/snapshot bytes using the accepted IDL plus an independent writer, not DUT recordings. Preserve the six original SectionContent message byte vectors. Derive expected states from literal graph tables and the independent reference, not the production planner.
- [ ] Materialize the non-dyadic PoseConstruction/1 expectations using the accepted ordered binary64 operations and quality bound, while keeping canonical equality exact. If any accepted source does not determine a necessary expected value, report the exact owning clause conflict; do not guess or reopen all P20.
- [ ] Use generators/streaming for boundary-sized inputs where necessary. Freeze their recipe, seed and expected hash before corresponding production changes. Do not commit meaningless giant repeated binaries merely to satisfy storage rituals.

## Task M03: executable oracles and real observation seams

Files: tools/fc08/run_verification.py, tools/fc08/check_materialization.py, tests/fc08/CMakeLists.txt, tests/fc08/native/, tests/fc08/web/, tests/fc08/headless/.

- [ ] Implement the fixed runner CLI in verification.lock.json and enumerate all case/variant IDs. Implement real graph, equality, numeric, wire, capability and timeline predicates. Use the literal correct/incorrect control inputs already frozen; no new proof-of-proof suite.
- [ ] Build the old-baseline raw codec/ingress probe for independently valid new Section/17/18 inputs. Capture actual unknown/unsupported results for R01. If the baseline already supports a capability, record applicability instead of manufacturing a RED.
- [ ] Bind actual observation seams to public owner/ingress and existing qualification read/failure hooks. Test adapters may observe and schedule faults; they may not create Section behavior, mutate production stores directly, patch the ABI, or substitute mock product dependencies.
- [ ] Future APIs absent from the baseline: register explicit, nonpassing CAPABILITY_ABSENT observations and the exact accepted future seam. Do not count them as executed product positives. No compilation-error-as-RED and no compile-time skip masquerading as coverage. Baseline probe plus oracle readiness is the materialization result; full FC08 GREEN remains the separate product task's target.
- [ ] Run the frozen R02 bad C01/C02 traces and a matching valid trace through the same oracle. This is oracle control evidence only, not observed Axiom behavior.

## Task M04: real profiles and bounded return

Files: .aegis/materialized/FC-08/profiles.json, inherited-regression.json, executable-verification.json; .aegis/results/FC-08-VERIFY-MATERIALIZE/.

- [ ] On available Windows Native, real Web/WASM/browser and headless environments, resolve the existing locked SDKs and capture actual compiler/runtime/browser versions, ABI/configuration, commands and nonempty discovery. Use the repository consumer_cmake_arguments helper. Do not invent successful profiles from a preset name.
- [ ] Add only minimal test-build glue; prefer a standalone tests/fc08/CMakeLists.txt and existing production targets. If a necessary product observation hook is absent and cannot be bound without unauthorized source mutation, return the exact interface gap as TASK_PACKAGE_DEFECT/MISSING_REQUIRED_INPUT rather than adding behavior.
- [ ] Bind relevant inherited regression by precise executable/test IDs and versions, reusing existing observation for W07/K01/P04 when identities match. Do not require every lower-numbered FC suite or physical checks.
- [ ] Produce an immutable output manifest covering source exports, generated descriptors, fixtures, reference/oracle files, runner, observations, profiles and regression list. Generate a non-authorized product-package candidate only; never set product_P32_authorized true.
- [ ] Return READY_FOR_CONTROL_REVIEW only with all M01-M04 outputs and actual profile bindings complete. Missing Windows/browser/toolchain yields BLOCKED_ENVIRONMENT with exact missing component and preserved completed outputs. Push the result to a new codex/fc08-verification-materialization branch or an explicitly recorded same-repository descendant; never mutate the original control input.

## Result contract

Record exact result revision/materialized ref, source and output hashes, actual invocations and statuses. The task does not issue FC08 DEV/P34/PHYS PASS. A checker cannot certify readiness from labels alone: require discoverable fixtures, replayable evidence and actual program/profile identities. No per-family duplicate report is required when one raw observation serves several obligations.
