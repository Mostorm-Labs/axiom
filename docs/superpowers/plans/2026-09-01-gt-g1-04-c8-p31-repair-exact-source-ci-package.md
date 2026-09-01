# GT-G1-04-C / C8 — P31 Superseding Implementation Task Package

Status: `READY_FOR_P32`

Lifecycle owner: `aegis-implementation`

Stage: `GT-G1-04-C / C8 -> P31 Task Package Planning`

This document is the durable superseding P31 package for C8. The immutable `package_ref` is the Git commit that first introduces this exact document. That commit SHA is authoritative and MUST be used literally by P32. This package supersedes the insufficient package at `c9a160a654762ebaa93b7aa53e87cedc6b4e8e56`.

P32 is **not** started by this package commit.

---

## 1. Why the previous package is superseded

The previous C8 P31 package authorized the goal but not an executable file-level change envelope. It did not freeze:

- exact implementation paths;
- exact-source hosted-CI trigger and run identity binding;
- exact durable evidence filenames and materialization root;
- required verification commands / evaluator entrypoints.

That package therefore returned `P31_PACKAGE_INSUFFICIENT / P31_REPAIR_REQUIRED` and MUST NOT be used as a P32 execution package.

---

## 2. Immutable task anchor and trusted authority

### Task anchor

```text
34c8db4f247849c5850e16226b0e556f57497053
```

Relation required for every later `source_ref`: `ancestor`.

This is the accepted C7-R2 materialized ref and is the immutable implementation task anchor for C8.

### Current Authority

- P20 Verification Design: `notion:3cc4c57a-590c-81ae-ab73-d75501c47169`
- P20 TerminalPhase Classification Addendum: `notion:3cc4c57a-590c-81c4-9e7b-d404c3fdba4b`
- P30 Implementation Plan: `notion:3cd4c57a-590c-8165-973f-ee31d93f1116`

### Accepted inherited refs

```text
P36 source_ref:          492d2f914f078a6e4ac8b567e07f7ec813c10107
P36 materialized_ref:    9b73be589ae070bc602b8989f83d89745a54774e
C7-R2 source_ref:        4abd5a472c84457cfecd763957e68a6dc06c18d3
C7-R2 materialized_ref:  34c8db4f247849c5850e16226b0e556f57497053
```

C0-C7 remain closed and are inherited, not reopened.

---

## 3. C8 purpose

C8 is the final `GT-G1-04-C` slice: **Exact-Source CI + Durable Evidence Materialization**.

P32 is authorized only to add the infrastructure necessary to prove that the exact immutable C8 implementation source commit was rebuilt and reverified on hosted CI, and that the hosted evidence was subsequently committed byte-for-byte into a reviewer-resolvable source-bound evidence directory.

C8 does not change C semantic truth.

Frozen semantic endpoint:

```text
PreparedApplyPlan
terminalPhase = PREPARE
pre-apply validation only
```

Atomic Apply remains out of scope.

Frozen expected-truth rules:

```text
expectedTruthWrites == 0
providerOutputUsedAsExpected == false
```

---

## 4. Exact P32 source authorization

The P32 implementation source commit MAY add exactly these four new files and no others:

```text
.github/workflows/g1-04-c-exact-source.yml
verification/tools/generate_g1_04_c8_evidence.mjs
verification/packages/semantic-conformance-cli/test/c8-exact-source-evidence.test.mjs
verification/schemas/semantic/g1-04-c-ci-run.schema.json
```

All four paths are `NEW` for C8.

No existing file is authorized for modification or deletion.

The diff from this package's immutable `package_ref` to the later P32 `source_ref` MUST contain exactly the four paths above. A missing path, extra path, rename, deletion, or edit outside this set is `BLOCKED_SCOPE`.

### Explicitly forbidden source changes

P32 MUST NOT modify any of the following roots or surfaces:

```text
runtime/semantic/include/**
runtime/semantic/src/**
runtime/semantic/tools/**
runtime/semantic/tests/**
schema/axiom/v1/**
verification/corpus/semantic/**
verification/fixture-author/**
verification/tools/g1_04_c_contract.mjs
verification/tools/generate_g1_04_c4_evidence.mjs
verification/tools/generate_g1_04_c5_evidence.mjs
verification/tools/generate_g1_04_c6_evidence.mjs
verification/tools/generate_g1_04_c7_evidence.mjs
verification/tools/generate_g1_04_c_p36_golden_repair_evidence.mjs
verification/packages/semantic-conformance-cli/src/**
docs/**
```

The only schema change authorized is the new C8 CI-run schema named above.

ObjectStore APIs, model/public surfaces, semantic operation behavior, provider behavior, canonical expected truth, fixture authoring, architecture documentation, and production instrumentation are all out of scope.

---

## 5. Exact-source hosted-CI contract

The new workflow is authoritative only for a GitHub Actions **`push`** event on:

```text
refs/heads/codex/gt-g1-04-c8-exact-source-ci
```

Do not use `pull_request` merge-commit checkout as C8 authority. Do not use a local-only run as C8 authority. A GitHub UI re-run of the same push workflow occurrence is permitted because it retains the original run `head_sha`; a new workflow dispatch is not an authoritative substitute.

Workflow permissions MUST remain:

```yaml
permissions:
  contents: read
```

The workflow MUST explicitly checkout the exact event commit, not rely on an implicit PR merge ref:

```yaml
- uses: actions/checkout@v4
  with:
    ref: ${{ github.sha }}
    fetch-depth: 0
```

The authoritative source identity is:

```text
SOURCE_REF = GITHUB_SHA
```

Immediately after checkout the workflow MUST fail unless all of these are equal:

```text
GitHub Actions run head_sha
GITHUB_SHA
SOURCE_REF
git rev-parse HEAD
```

The workflow MUST also assert:

```text
GITHUB_EVENT_NAME == push
GITHUB_REF == refs/heads/codex/gt-g1-04-c8-exact-source-ci
```

Any mismatch is `BLOCKED_EVIDENCE`; no C8 PASS evidence may be emitted.

The hosted workflow MUST NOT create a git commit or push repository state. It produces an immutable Actions artifact only. Repository materialization happens later as a separate evidence-only commit.

---

## 6. Required hosted verification sequence

The C8 workflow MUST execute the following logical sequence from a clean exact-source checkout. Command spelling may use normal shell variable extraction, but the listed entrypoints and proof obligations are mandatory and may not be replaced by weaker checks.

### 6.1 Install verification dependencies and build the semantic CLI

From repository root:

```bash
cd verification
npm ci
npm run build
npm run validate
cd ..
```

This preserves the existing workspace test surface and rebuilds the semantic conformance CLI from the exact source.

### 6.2 Recompile C fixtures from canonical authoring sources twice

Do not write into the tracked accepted corpus. Compile into two temporary directories:

```bash
python3 verification/fixture-author/compile_g1_04_c.py --root . --output "$RUNNER_TEMP/g1-04-c8-fixtures-a"
python3 verification/fixture-author/compile_g1_04_c.py --root . --output "$RUNNER_TEMP/g1-04-c8-fixtures-b"
diff -ru "$RUNNER_TEMP/g1-04-c8-fixtures-a" "$RUNNER_TEMP/g1-04-c8-fixtures-b"
diff -ru verification/corpus/semantic/v1/g1-04-c/generated "$RUNNER_TEMP/g1-04-c8-fixtures-a"
```

Required result:

```text
90 cases
90 blocking cases
A == B
tracked generated corpus == A
```

No accepted authoring or generated fixture bytes may be changed.

### 6.3 Build production semantic runtime and exact full CTest suite

Reuse the repository's locked semantic toolchain path used by the existing hosted semantic workflow:

```bash
python3 tools/semantic_fetch.py --target linux-x86_64
python3 tools/bootstrap_deps.py --core
cmake -S . -B out/g1-04-c8 -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCANVAS_BUILD_POC01=OFF \
  -DCANVAS_BUILD_SEMANTIC=ON \
  -DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON \
  -DProtobuf_DIR="$PROTOBUF_DIR" \
  -Dabsl_DIR="$ABSL_DIR" \
  -Dutf8_range_DIR="$UTF8_RANGE_DIR"
cmake --build out/g1-04-c8 --parallel
ctest --test-dir out/g1-04-c8 --output-on-failure
```

`PROTOBUF_DIR`, `ABSL_DIR`, and `UTF8_RANGE_DIR` MUST resolve to the locked toolchain under `.deps/protobuf`, exactly as in `.github/workflows/g1-semantic-codec.yml`.

The unfiltered CTest result is the authoritative full semantic CTest result. C8 MUST NOT substitute a focused-only test run.

### 6.4 Rerun the native C provider observation path

The existing target is authoritative and MUST be rebuilt from the exact source:

```text
canvas_semantic_g1_04_c_observation_export
```

Its existing runtime contract is:

```text
canvas_semantic_g1_04_c_observation_export <manifest-path>
```

The workflow MUST run it against:

```text
verification/corpus/semantic/v1/g1-04-c/generated/manifest.json
```

and capture its JSON stdout in an untracked CI staging file. The expected runtime facts are:

```text
acceptedCases == 90
observationCount == 180
noMutationObservations == 180
unexpectedHarnessErrors == 0
providers == [reference, indexed]
factsOnly == true
```

This command exercises the existing production semantic path for both reference and indexed providers. No new production observer is authorized.

### 6.5 Rerun the independent verification-side evaluator

`verification/tools/generate_g1_04_c8_evidence.mjs` is authorized to consume:

- the accepted C authoring/suite inputs;
- the deterministic generated manifest;
- the fresh native observation-export JSON;
- existing semantic-conformance CLI modules (`coordinateCase`, core-corpus, cross-cutting, provider-diff/gate logic);
- the inherited accepted P36 and C7-R2 evidence as lineage/trusted-basis inputs only;
- GitHub Actions identity metadata.

It MUST independently evaluate the fresh native observations. It MUST NOT manufacture expected truth from provider output, write expected truth, invoke Atomic Apply, add a production mutation path, or edit accepted corpus/schema inputs.

This is the C8 realization of P30's independent production-path observation requirement. If the existing verifier-side modules are insufficient to establish independence without changing a fifth source file, P32 MUST stop with `BLOCKED_SCOPE` rather than expand implementation scope.

### 6.6 C8-specific evidence contract test

The new test entrypoint is exactly:

```bash
node --test verification/packages/semantic-conformance-cli/test/c8-exact-source-evidence.test.mjs
```

It MUST cover at least:

- run head SHA / checkout SHA / `sourceRef` equality;
- wrong event or wrong branch rejection;
- wrong `packageRef` / `taskAnchor` rejection;
- missing required evidence rejection;
- `expectedTruthWrites != 0` rejection;
- `providerOutputUsedAsExpected != false` rejection;
- production semantic / Authority delta rejection;
- stale or foreign `sourceRef` evidence rejection;
- local-only CI identity rejection;
- materialization path escape rejection.

Negative tests are required. A happy-path-only test is insufficient.

---

## 7. C8 evidence generator CLI contract

The new generator MUST expose one deterministic non-interactive CLI with this required identity surface:

```text
node verification/tools/generate_g1_04_c8_evidence.mjs \
  --package-ref <P31_PACKAGE_REF> \
  --task-anchor 34c8db4f247849c5850e16226b0e556f57497053 \
  --source-ref <SOURCE_REF> \
  --observation <fresh-native-observation-json> \
  --output-dir <staging-output-directory> \
  --ci-run-id <GITHUB_RUN_ID> \
  --ci-run-attempt <GITHUB_RUN_ATTEMPT> \
  --ci-event push \
  --ci-ref refs/heads/codex/gt-g1-04-c8-exact-source-ci \
  --ci-head-sha <GITHUB_SHA> \
  --checkout-sha <git-rev-parse-HEAD> \
  --workflow-ref <GITHUB_WORKFLOW_REF>
```

`<P31_PACKAGE_REF>` is the immutable commit that introduces this superseding package.

The generator MUST fail closed if `packageRef` is not that commit, if `taskAnchor` is not the frozen anchor, if source identity differs anywhere, or if the package/source ancestry and source diff envelope cannot be proven from git.

The generator MUST verify:

```text
package_ref is ancestor of source_ref
task_anchor is ancestor of source_ref
diff(package_ref..source_ref) == exact four-file authorization
production semantic source/header delta from task_anchor == 0
schema/axiom/v1 delta from task_anchor == 0
accepted C corpus/fixture-author delta from task_anchor == 0
architecture/docs delta package_ref..source_ref == 0
expectedTruthWrites == 0
providerOutputUsedAsExpected == false
```

---

## 8. Exact durable evidence set

For a valid C8 `source_ref`, the hosted generator MUST emit exactly these durable C8 evidence JSON files:

```text
C-CORE-CORPUS.json
C-IDEMPOTENCY.json
C-NO-MUTATION.json
C-PLAN-PROJECTION.json
C-OPEN-RECONCILIATION.json
C-PROVIDER-DIFF.json
C-GATE.json
C-CI-RUN.json
```

The first seven are fresh C8 source-bound recomputation/consolidation of the already-established C5/C6/C7 proof surfaces. `C-CI-RUN.json` is the new C8 hosted-run binding record.

Historical repair evidence such as:

```text
P36-VERIFICATION-GOLDEN-REPAIR.json
```

is trusted lineage input only and MUST NOT be copied or relabeled as newly produced C8 evidence.

Every durable C8 file MUST be source-bound to the same `sourceRef`. All generator-produced C8 envelope records MUST also bind the superseding `packageRef` and frozen `taskAnchor`.

---

## 9. `C-CI-RUN.json` schema contract

The new schema path is:

```text
verification/schemas/semantic/g1-04-c-ci-run.schema.json
```

The corresponding evidence file is:

```text
C-CI-RUN.json
```

At minimum the schema MUST require fields that prove:

```text
format / formatVersion
gate == GT-G1-04-C
slice == C8
packageRef
taskAnchor.revision
taskAnchor.relation == ancestor
sourceRef
repository
workflowName
workflowPath
workflowRef
event == push
ref == refs/heads/codex/gt-g1-04-c8-exact-source-ci
runId
runAttempt
hostedRunUrl
headSha
checkoutSha
artifactName
requiredEvidenceFiles
verificationResults
expectedTruthWrites == 0
providerOutputUsedAsExpected == false
productionSemanticDelta == 0
authorityDelta == 0
verdict
```

For a PASS record:

```text
sourceRef == headSha == checkoutSha
```

must be structurally and behaviorally enforced.

The record MUST identify the artifact name as:

```text
gt-g1-04-c8-<sourceRef>
```

and list the eight required evidence files.

---

## 10. Hosted artifact publication contract

After all required verification and evidence validation passes, the workflow MUST upload the staging evidence directory using `actions/upload-artifact@v4`.

Required artifact identity:

```text
gt-g1-04-c8-<SOURCE_REF>
```

Required content: the exact eight JSON files in Section 8.

`if-no-files-found` MUST be `error`.

Retention MUST be at least 14 days.

A workflow run that passes tests but fails artifact publication is `BLOCKED_EVIDENCE`, not C8 PASS.

The hosted artifact is the source of bytes for repository materialization. Locally regenerated substitutes are forbidden.

---

## 11. P32 source commit contract

When P32 is later explicitly started:

1. Start from this immutable `package_ref` on `codex/gt-g1-04-c8-exact-source-ci`.
2. Add only the four authorized implementation files.
3. Create one implementation source commit.
4. Record that commit as `source_ref`.
5. Prove `package_ref` and `task_anchor` are ancestors of `source_ref`.
6. Prove `package_ref..source_ref` contains exactly the four authorized files.
7. Push `source_ref` so the new push workflow executes on that exact commit.
8. Do not create the evidence materialization commit until the exact-source hosted run is complete and independently resolvable.

No source amendment is permitted after hosted verification. If source changes, a new `source_ref` and a new exact-source run are required.

---

## 12. Durable materialization contract

After an authoritative hosted run for the exact `source_ref` succeeds:

1. Resolve the GitHub Actions workflow occurrence and confirm its `head_sha == source_ref`.
2. Resolve artifact `gt-g1-04-c8-<source_ref>` from that occurrence.
3. Download that exact artifact; do not regenerate evidence locally.
4. Validate all eight JSON files and their source/package/task identities.
5. Commit the artifact bytes under exactly:

```text
verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/
```

6. The materialization commit may add only the eight evidence files in that source-bound directory. It MUST contain no implementation/source changes.
7. Record that evidence-only commit as `materialized_ref`.
8. Re-read the committed files from `materialized_ref` and corroborate them against the hosted artifact bytes and hosted run identity.

The source/materialization relation MUST therefore be:

```text
package_ref
  -> source_ref          (four authorized infrastructure files only)
  -> materialized_ref    (eight evidence JSON files only)
```

The evidence directory is keyed by `source_ref`, never by `materialized_ref`.

---

## 13. Fail-closed matrix

Any of the following blocks C8 downstream closure:

| Condition | Required result |
|---|---|
| branch head diverges before P32 starts | `BLOCKED_REPOSITORY_STATE` |
| `task_anchor` not ancestor of source | `BLOCKED_LINEAGE` |
| `package_ref` not ancestor of source | `BLOCKED_LINEAGE` |
| source diff contains any path outside four-file authorization | `BLOCKED_SCOPE` |
| required authorized file missing | `BLOCKED_SCOPE` |
| hosted event is not authoritative push | `BLOCKED_EVIDENCE` |
| run `head_sha != sourceRef` | `BLOCKED_EVIDENCE` |
| checkout SHA `!= sourceRef` | `BLOCKED_EVIDENCE` |
| fixture double-build differs | `FAIL` |
| fresh fixtures differ from accepted generated corpus | `FAIL` |
| semantic CLI build/validate fails | `FAIL` |
| full CTest fails | `FAIL` |
| native observer fails or inventory is not 90/180/180 | `FAIL` |
| fresh provider observations disagree with C expected truth | `FAIL` |
| expected truth is written | `FAIL` |
| provider output is used as expected truth | `FAIL` |
| production semantic source/header delta is non-zero | `BLOCKED_SCOPE` |
| schema / accepted corpus / fixture-author / Authority delta is non-zero | `BLOCKED_SCOPE` |
| any required evidence JSON is missing or invalid | `BLOCKED_EVIDENCE` |
| hosted artifact is absent/unresolvable | `BLOCKED_EVIDENCE` |
| only local evidence exists | `BLOCKED_EVIDENCE` |
| materialization contains non-evidence paths | `BLOCKED_SCOPE` |
| materialized bytes differ from hosted artifact | `BLOCKED_EVIDENCE` |

Fail closed. Do not repair by broadening P32 scope.

---

## 14. Required P32 return contract

A later completed P32 must return at least:

```text
package_ref: <this superseding P31 commit>
task_anchor: 34c8db4f247849c5850e16226b0e556f57497053
source_ref: <exact four-file implementation commit>
materialized_ref: <evidence-only materialization commit>
ci_result: <hosted exact-source run identity + PASS/BLOCKED>
evidence_paths:
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-CORE-CORPUS.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-IDEMPOTENCY.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-NO-MUTATION.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-PLAN-PROJECTION.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-OPEN-RECONCILIATION.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-PROVIDER-DIFF.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-GATE.json
  verification/evidence/gates/G1/<source_ref>/GT-G1-04-C/C-CI-RUN.json
```

P32 must additionally report the exact source diff, full semantic CTest result, semantic CLI validation result, fixture reproducibility result, native observer result, artifact identity, and materialized-artifact byte corroboration result.

---

## 15. Exit criteria for this P31 occurrence

This superseding P31 occurrence is `READY_FOR_P32` only when:

- this document is committed as one immutable docs-only commit;
- its parent is the insufficient package commit `c9a160a654762ebaa93b7aa53e87cedc6b4e8e56`;
- no implementation file is changed by the package commit;
- the resulting commit SHA is frozen as the sole valid C8 P32 `package_ref`;
- P32 remains `NOT_STARTED`.

`GT-G1-05` remains `NOT_STARTED`.
