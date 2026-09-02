# GT-G1-04-B B6 Evidence Accuracy Repair Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans for this evidence-only repair. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Correct one false machine-readable Reference/Indexed coverage claim in the current B6 B-RESTORE evidence without changing the already-reviewed B6 source/test snapshot or fabricating new verification history.

**Architecture:** This is a P36 evidence-only repair owned by `EVIDENCE_GAP`. The production/source snapshot remains `871eec8172674c8fba85548434b5e96c7c30fbf8`; the existing test occurrence remains the recorded verification occurrence. Only the evidence JSON is corrected to describe the actual parity coverage already present in source/tests.

**Tech Stack:** Git, JSON evidence, GitHub materialization.

**Spec:**
- P34 blocked review: `notion:3cb4c57a-590c-8124-9dd4-f7d602bdb18e`
- P35/P36 authorization: `notion:3cb4c57a-590c-8146-8aba-c707696a8651`
- B-AUTH-02 Current Authority: `notion:3cb4c57a-590c-815a-b8fa-cd785a837da7`
- Source/test snapshot: `871eec8172674c8fba85548434b5e96c7c30fbf8`
- Existing evidence occurrence: `d553622da3890fb16ec221d09f6a0c3ae4475813`

## Global Constraints

- Do not modify C++ source, headers, tests, CMake, authority mirrors, or earlier plans.
- Do not rerun or invent verification solely to rewrite history; preserve the recorded execution occurrence exactly.
- Do not amend, rebase, squash, reset, or force-push.
- Do not overwrite historical B6 evidence under source SHA `06dfefd2f0f6e1094ec6867755307413649e35c3`.
- Do not start B7, PreparedApplyPlan, OperationEngine, GT-G1-04-C, or GT-G1-05.
- The corrected evidence must continue to bind `sourceCommit == testedCommit == 871eec8172674c8fba85548434b5e96c7c30fbf8`.

---

### Task 1: Synchronize to the control-plane package

**Files:**
- Read only: repository state and this plan.

**Interfaces:**
- Consumes: remote branch `codex/gt-g1-04-operation-apply`.
- Produces: safe fast-forwarded local control-plane state.

- [ ] **Step 1: Inspect local state**

Run:

```bash
git status --short
git branch --show-current
git rev-parse HEAD
git fetch origin
git rev-parse origin/codex/gt-g1-04-operation-apply
```

Expected branch: `codex/gt-g1-04-operation-apply`.

Preserve all untracked build/Android visual-smoke data.

- [ ] **Step 2: Verify a safe fast-forward**

If local HEAD is the previous evidence commit `d553622da3890fb16ec221d09f6a0c3ae4475813`, run:

```bash
git merge-base --is-ancestor \
  d553622da3890fb16ec221d09f6a0c3ae4475813 \
  origin/codex/gt-g1-04-operation-apply

git log --oneline HEAD..origin/codex/gt-g1-04-operation-apply
```

Expected only the docs control-plane commit containing this plan.

If unexpected substantive commits exist or tracked local changes prevent a safe fast-forward, stop with `BLOCKED_STARTING_STATE_MISMATCH`.

- [ ] **Step 3: Fast-forward only**

```bash
git merge --ff-only origin/codex/gt-g1-04-operation-apply
```

Do not use reset/rebase/cherry-pick.

---

### Task 2: Correct the machine-readable parity statement

**Files:**
- Modify only: `verification/evidence/gates/G1/871eec8172674c8fba85548434b5e96c7c30fbf8/GT-G1-04-B/B-RESTORE.json`

**Interfaces:**
- Consumes: actual test source at `871eec8172674c8fba85548434b5e96c7c30fbf8`.
- Produces: exact evidence coverage description.

- [ ] **Step 1: Inspect the existing evidence field and test source**

Confirm the current evidence contains:

```json
"allStateOwningHcvCasesReferenceIndexed": true
```

Confirm actual focused-helper coverage is:

```text
HCV-B01..B14: primarily ReferenceObjectStore focused semantic cases
HCV-B15: explicit ReferenceObjectStore / IndexedObjectStore parity + no mutation + index rebuild
HCV-B16: lookup/performance instrumentation
RST-HCV-01..RST-HCV-10: exercised through the B6 both(...) Reference/Indexed harness
```

- [ ] **Step 2: Replace the false blanket assertion with exact structured coverage**

Keep the existing `parityAndMutation` object, but remove or set aside the false blanket boolean. Replace it with fields equivalent to:

```json
"focusedHelperParityCoverage": {
  "HCV-B01-B14": "primarily ReferenceObjectStore focused semantic cases",
  "HCV-B15": "explicit ReferenceObjectStore/IndexedObjectStore parity, no mutation, Indexed indexMatchesRebuild",
  "HCV-B16": "performance/lookup instrumentation"
},
"restoreCapabilityCasesReferenceIndexed": true,
"statefulIssueParity": true,
"createsProjectionParity": true,
"referenceProjectionUnchanged": true,
"indexedProjectionUnchanged": true,
"indexedIndexMatchesRebuild": true,
"outputAtomicOnCapabilityFailure": true
```

Exact JSON key names may follow the file's established naming style, but the semantics must be unambiguous and must not claim that HCV-B01..B14 each execute under both stores.

Do not alter recorded test counts, commands, RED provenance, performance measurements, source SHA, tested SHA, authority references, or scope guards unless required solely to describe the corrected parity coverage accurately.

- [ ] **Step 3: Preserve the nonblocking HCV-B09 review finding as review context only**

Do not change tests in this task. Do not claim HCV-B09 is a structurally complete A-lane Connector fixture. The P34 reviewer already recorded this as a nonblocking test-quality finding; it does not require source/test modification for this evidence-only repair.

---

### Task 3: Validate evidence syntax and diff scope

**Files:**
- Modified evidence JSON only.

**Interfaces:**
- Produces: syntactically valid JSON and one-file diff.

- [ ] **Step 1: Validate JSON syntax**

Use an available local JSON parser, for example:

```bash
python3 -m json.tool \
  verification/evidence/gates/G1/871eec8172674c8fba85548434b5e96c7c30fbf8/GT-G1-04-B/B-RESTORE.json \
  >/dev/null
```

Expected: exit `0`.

This validates syntax only; it is not a semantic test rerun.

- [ ] **Step 2: Verify source/test snapshot remains untouched**

Run:

```bash
git diff --name-only HEAD --
git diff --check
```

Before commit, the only modified path must be:

```text
verification/evidence/gates/G1/871eec8172674c8fba85548434b5e96c7c30fbf8/GT-G1-04-B/B-RESTORE.json
```

- [ ] **Step 3: Review the corrected JSON against source**

Manually verify:

```text
sourceCommit == 871eec8172674c8fba85548434b5e96c7c30fbf8
testedCommit == 871eec8172674c8fba85548434b5e96c7c30fbf8
HCV-B15 is the explicit helper parity test
RST-HCV cases use both(...) Reference/Indexed execution
no field claims all HCV-B01..B14 are dual-store cases
```

---

### Task 4: Materialize the corrected evidence

**Files:**
- Commit only the corrected B-RESTORE JSON.

**Interfaces:**
- Produces: reviewer-accessible corrected evidence commit.

- [ ] **Step 1: Commit evidence only**

```bash
git add verification/evidence/gates/G1/871eec8172674c8fba85548434b5e96c7c30fbf8/GT-G1-04-B/B-RESTORE.json
git commit -m "evidence(g1): correct B6 parity coverage claim"
```

- [ ] **Step 2: Push normally**

Push the branch without force.

- [ ] **Step 3: Verify remote materialization**

Confirm remote branch resolves to the new evidence commit and the diff from the control-plane package commit contains only the corrected `B-RESTORE.json`.

## Exit Criteria

Return `READY_FOR_INDEPENDENT_P34_REREVIEW` only when:

1. The source/test snapshot `871eec8172674c8fba85548434b5e96c7c30fbf8` is unchanged.
2. The only substantive repair is the current B-RESTORE evidence JSON.
3. The false blanket helper parity assertion is removed/replaced by exact coverage semantics.
4. JSON syntax validation and `git diff --check` succeed.
5. The corrected evidence commit is pushed and reviewer-accessible.
6. No semantic test rerun is falsely claimed as part of this evidence-only repair.
7. B7 remains not authorized.

## Required Final Report

```text
GT-G1-04-B B6 EVIDENCE-ONLY P36 RESULT

Starting local HEAD
Fetched remote HEAD
Fast-forward result
Package ref
P34 blocked review
P35/P36 authorization

Evidence path
Old inaccurate claim
Corrected parity coverage fields
JSON syntax check
git diff --check

Source/test changed = NO
Tests rerun = NO
B7 started = NO
PreparedApplyPlan changed = NO
OperationEngine changed = NO
GT-G1-04-C started = NO
GT-G1-05 changed = NO
History rewritten = NO
Force push = NO

Corrected evidence commit SHA
Remote HEAD

Status:
B6 = READY_FOR_INDEPENDENT_P34_REREVIEW
B7 = BLOCKED_DEPENDENCY / NOT_AUTHORIZED
GT-G1-04-C = DEFERRED
GT-G1-05 = NOT_AUTHORIZED
```
