# GT-G0-15 Nightly / Release Wiring Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立 provider-neutral 的完整平台验证契约，并用同一可复用执行图驱动 Nightly 与 Release Conformance。

**Architecture:** `GT-G0-14` 的 schema、protocol、semantic bootstrap 和平台 observation/result 继续作为可信输入。本任务新增 Full Run-set、Evidence Index、Platform Release Decision 与 Reproducibility Comparison，由共享 runner 聚合，CLI 只做文件边界适配，GitHub Actions 只负责调度和 artifact 传递。

**Tech Stack:** TypeScript 7、Node.js test runner、JSON Schema Draft 2020-12、GitHub Actions、Python/Node 仓库校验工具。

**Spec:** `docs/architecture/verification/GT_G0_15_NIGHTLY_RELEASE_WIRING_DESIGN.md`

## Global Constraints

- 只实现 `GT-G0-15`，不得生成 G0 Gate Report 或正式 G3 GateDecision。
- 所有 decision 的 authority 固定为 `G0_WIRING_ONLY`。
- 平台矩阵固定为 Web、Windows、Android、Apple 四个 family，以及 `web/windows/android/ios/ipados` 五个独立 profile。
- Release 缺少同 source commit 的物理 Evidence 时必须阻断，不得由 hosted、simulator 或 emulator 替代。
- Nightly 与 Release 使用同一 reusable workflow；workflow 只授予 `contents: read`。
- 不创建 GitHub Release，不引入性能 SLO，不改产品代码，不修改或纳入未跟踪的 Android visual-smoke 数据。
- 所有行为改动先运行失败测试，再写最小实现。
- 本轮不 commit、push 或创建 PR。

---

### Task 1: Provider-neutral full-conformance domain

**Files:**
- Create: `verification/packages/platform-harness-runner/src/ci/FullConformance.ts`
- Modify: `verification/packages/platform-harness-runner/src/index.ts`
- Create: `verification/packages/platform-harness-runner/test/full-conformance.test.mjs`

**Interfaces:**
- Produces: `createFullRunSet(options): FullRunSetManifest`
- Produces: `createPlatformEvidenceIndex(runSet, records): PlatformEvidenceIndex`
- Produces: `aggregatePlatformRelease(runSet, index): PlatformReleaseDecision`
- Produces: `comparePlatformReleaseDecisions(left, right): ReproducibilityComparison`

- [ ] **Step 1: Write the failing domain tests**

  覆盖固定五 profile、iOS/iPadOS 不可替代、状态优先级、release physical 阻断、环境诊断排除和 identity 不匹配。

- [ ] **Step 2: Run tests to verify RED**

  Run: `cd verification && npm test --workspace @axiom/platform-harness-runner`

  Expected: FAIL because `FullConformance` exports do not exist.

- [ ] **Step 3: Implement minimal deterministic domain logic**

  使用稳定枚举顺序、去重检查、SHA-256 格式校验和显式 identity 比较；不解析 GitHub 日志。

- [ ] **Step 4: Run tests to verify GREEN**

  Run: `cd verification && npm test --workspace @axiom/platform-harness-runner`

  Expected: all runner tests pass.

### Task 2: Versioned schemas and fixtures

**Files:**
- Create: `verification/schemas/platform/full-run-set.schema.json`
- Create: `verification/schemas/platform/platform-evidence-record.schema.json`
- Create: `verification/schemas/platform/platform-evidence-index.schema.json`
- Create: `verification/schemas/platform/platform-release-decision.schema.json`
- Create: `verification/schemas/platform/reproducibility-comparison.schema.json`
- Create: matching files under `verification/schemas/platform/fixtures/`
- Modify: `verification/tools/validate_schemas.mjs`
- Modify: `verification/tests/schema_meta.test.mjs`

**Interfaces:**
- Consumes: exact object shapes exported by `FullConformance.ts`.
- Produces: strict Draft 2020-12 validation contracts with `additionalProperties: false`.

- [ ] **Step 1: Extend schema inventory and rejection tests first**

  Add expected schema names and tests for unknown field, missing Apple profile, invalid SHA and invalid authority.

- [ ] **Step 2: Run schema tests to verify RED**

  Run: `cd verification && node --test tests/schema_meta.test.mjs && node tools/validate_schemas.mjs`

  Expected: FAIL because the five schema/fixture pairs are missing.

- [ ] **Step 3: Add strict schemas and valid fixtures**

  Mirror the TypeScript contract exactly; all identity and artifact hash fields use lowercase 64-hex patterns.

- [ ] **Step 4: Run schema tests to verify GREEN**

  Run: `cd verification && node --test tests/schema_meta.test.mjs && node tools/validate_schemas.mjs`

  Expected: all schema tests and fixtures pass.

### Task 3: CLI file boundary

**Files:**
- Create: `verification/packages/platform-conformance-cli/src/commands/full_run_set.ts`
- Create: `verification/packages/platform-conformance-cli/src/commands/aggregate_full.ts`
- Create: `verification/packages/platform-conformance-cli/src/commands/compare_full.ts`
- Modify: `verification/packages/platform-conformance-cli/src/main.ts`
- Modify: `verification/packages/platform-conformance-cli/test/cli.test.mjs`

**Interfaces:**
- Produces command: `full-run-set --cadence nightly|release --source-commit SHA --schema-sha256 SHA --corpus-sha256 SHA --runner-version V --runtime-version V --repeat N --seed N --output PATH`
- Produces command: `aggregate-full --run-set PATH --records PATH --index PATH --output PATH`
- Produces command: `compare-full --left PATH --right PATH --output PATH`

- [ ] **Step 1: Write CLI failure and success tests first**

  Cover exact revision validation, deterministic output, missing profiles, release physical blocking, comparable runs and mismatched identity.

- [ ] **Step 2: Run CLI tests to verify RED**

  Run: `cd verification && npm test --workspace @axiom/platform-conformance-cli`

  Expected: FAIL with invalid/unknown full-conformance commands.

- [ ] **Step 3: Implement the three commands**

  Commands only read/write JSON and map domain decisions to existing stable exit codes; they never mutate corpus or generate expected output.

- [ ] **Step 4: Run CLI tests to verify GREEN**

  Run: `cd verification && npm test --workspace @axiom/platform-conformance-cli`

  Expected: all CLI tests pass.

### Task 4: Reusable workflow and cadence entrypoints

**Files:**
- Create: `.github/workflows/g0-full-platform-conformance.yml`
- Create: `.github/workflows/g0-nightly.yml`
- Create: `.github/workflows/g0-release-conformance.yml`
- Create: `verification/tests/nightly_release_contract.test.mjs`
- Modify: `verification/package.json`

**Interfaces:**
- Reusable workflow inputs: `cadence`, `source_commit`, `repeat_count`.
- Nightly calls reusable workflow for latest `main` with two repeats.
- Release calls reusable workflow for an explicit 40-hex `main` commit and never publishes assets.

- [ ] **Step 1: Write workflow static tests first**

  Assert shared `uses`, read-only permissions, exact revision guard, five profiles, trusted-root dependencies, `always()` aggregate, artifact upload and absence of release publishing/write permissions.

- [ ] **Step 2: Run workflow test to verify RED**

  Run: `cd verification && node --test tests/nightly_release_contract.test.mjs`

  Expected: FAIL because the workflows do not exist.

- [ ] **Step 3: Add reusable and trigger workflows**

  Preserve machine-readable outputs at every layer. Hosted Release dry-run may conclude `BLOCKED_AUTHORITY`; the workflow must still upload its decision and evidence chain before returning the expected non-PASS conclusion.

- [ ] **Step 4: Run workflow tests to verify GREEN**

  Run: `cd verification && node --test tests/nightly_release_contract.test.mjs`

  Expected: static workflow contract passes.

### Task 5: Evidence generator and route tracking

**Files:**
- Create: `verification/tools/generate_g0_15_evidence.mjs`
- Create: `verification/evidence/g0/gt-g0-15/` generated JSON files
- Create: `docs/quality/evidence/g0/gt-g0-15-nightly-release-wiring-20260825.md`
- Modify: `verification/README.md`
- Modify: `docs/planning/GATE_TASK_TRACKER.md`
- Modify: `docs/planning/R_MILESTONE_STATUS.md`

**Interfaces:**
- Generator consumes `AXIOM_EVIDENCE_SOURCE_COMMIT`, defaulting to `WORKTREE` only for local validation.
- Generator emits run-set, profile records, evidence index, decision, reproducibility comparison, deliberate failures, summary and manifest hashes.

- [ ] **Step 1: Write Evidence validation tests first**

  Add checks for manifest hashes, authority, five profiles, PG-01～PG-06, deliberate failure attribution and WORKTREE limitation.

- [ ] **Step 2: Run Evidence tests to verify RED**

  Run: `cd verification && npm run validate`

  Expected: FAIL because GT-G0-15 Evidence is absent.

- [ ] **Step 3: Implement generator and update documentation**

  Mark design `Pass`, implementation/validation/final `Validating` while source commit is `WORKTREE` or hosted runs are absent. Do not mark Gate or R1 Accepted.

- [ ] **Step 4: Generate and validate WORKTREE Evidence**

  Run: `AXIOM_EVIDENCE_SOURCE_COMMIT=WORKTREE node verification/tools/generate_g0_15_evidence.mjs`

  Expected: deterministic local bundle with explicit hosted/commit-bound limitations.

### Task 6: Full verification

**Files:** all files changed above.

**Interfaces:** validates the entire GT-G0-15 worktree without advancing to GT-G0-16.

- [ ] **Step 1: Run build, typecheck, schemas and all tests**

  Run: `cd verification && npm run build && npm run typecheck && npm run validate && npm run test`

- [ ] **Step 2: Run repository checks**

  Run: `python3 verification/tools/validate_workspace.py && python3 tools/check_docs.py && git diff --check`

- [ ] **Step 3: Audit scope and status**

  Confirm no product files, GT-G0-16 implementation, commits, pushes or untracked Android visual-smoke files were consumed. Report hosted and commit-bound Evidence as remaining conditions.
