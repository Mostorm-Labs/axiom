# Axiom Build Environment Authority Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Replace ordinary semantic CI source-bootstrap with a locked immutable semantic SDK consumer path, centralize dependency environment wiring, and prove that existing historical Skia Release assets remain source-free and reusable.

**Architecture:** Keep dependency production and dependency consumption separate. A new repository-local build-environment setup helper owns core dependency bootstrap, semantic SDK fetch/verification, canonical CMake environment export, and machine-readable build-environment facts; ordinary workflows consume that contract instead of rebuilding Protobuf/Abseil or calculating package directories themselves. Skia remains on the existing `r1-full-skia-sdk.lock.json` + `tools/skia/fetch.py` path and is validated by a representative historical Release fetch without changing Skia producer/profile/lock/release state.

**Tech Stack:** Python 3.12, GitHub Actions, CMake/Ninja, existing `tools/bootstrap_deps.py`, existing `tools/semantic_fetch.py`/`tools/semantic_sdk.py`, existing Skia SDK consumer tooling, Python `unittest`.

**Spec:** `docs/superpowers/specs/2026-09-07-build-environment-authority-design.md`

## Global Constraints

- Do not modify Axiom semantic/runtime/render behavior.
- Do not modify `r1-full-skia-sdk.lock.json`, existing Skia SDK identities, Skia source, Skia producer workflows, Skia profiles, or any existing Skia Release asset.
- Do not rebuild or republish historical Skia assets.
- A locked Skia asset failure must fail closed; no source fallback is permitted.
- Ordinary semantic consumer CI must not run `tools/bootstrap_deps.py --semantic-codec`.
- `bootstrap_deps.py --semantic-codec` remains valid for the semantic producer and explicit developer/recovery use only.
- Cache, if added later, is only a transport optimization and never dependency Authority.
- Exact-source repository identity checks, semantic protobuf-on/off test meaning, and Gate evidence semantics must remain intact.
- v0.1 hosted consumer target is `linux-x86_64`; existing cross-platform Skia consumer behavior remains unchanged.
- The active `GT-G1-07` rollout may modify only its already-authorized workflow path; it must not add new runtime/schema/source paths to that task branch.

---

## File Structure

- Create `tools/setup_build_environment.py` — single CI/developer consumer entry point for core + semantic dependency acquisition, validation, canonical environment export, and facts materialization.
- Create `verification/tests/test_build_environment.py` — unit contract for canonical paths, fail-closed validation, facts, and GitHub environment export.
- Modify `.github/workflows/g1-semantic-codec.yml` — general semantic lane consumes the shared setup contract and records build-environment facts.
- Create `.github/workflows/build-environment-contract.yml` — path-scoped hosted proof that semantic consumers use the locked Release asset and that one existing R1 Full Skia historical Release asset still resolves/verifies source-free.
- Modify `verification/tests/test_ci_trigger_boundaries.py` — static routing/anti-regression checks for the new contract workflow and semantic lane.
- Branch-local rollout only: modify `.github/workflows/g1-07-exact-source.yml` on `codex/gt-g1-07-replay-inspector` — replace `--semantic-codec` bootstrap with the already-present locked semantic fetch, without introducing any new task source path.

## Interfaces

The shared helper exposes these stable functions and CLI outputs:

```python
def validate_semantic_install(root: Path) -> dict[str, str]: ...
def setup_environment(*, core: bool, semantic: bool, target: str) -> dict[str, object]: ...
def write_github_env(path: Path, environment: dict[str, str]) -> None: ...
def write_facts(path: Path, facts: dict[str, object]) -> None: ...
```

CLI:

```text
python3 tools/setup_build_environment.py \
  --core \
  --semantic \
  --target linux-x86_64 \
  --github-env "$GITHUB_ENV" \
  --facts-output <path>
```

Canonical exported environment:

```text
AXIOM_DEPS_DIR=<repo>/.deps
AXIOM_SEMANTIC_SDK_ROOT=<repo>/.deps/protobuf
CMAKE_PREFIX_PATH=<repo>/.deps/protobuf
PROTOBUF_DIR=<repo>/.deps/protobuf/lib/cmake/protobuf
ABSL_DIR=<repo>/.deps/protobuf/lib/cmake/absl
UTF8_RANGE_DIR=<repo>/.deps/protobuf/lib/cmake/utf8_range
AXIOM_PROTOC=<repo>/.deps/protobuf/bin/protoc
```

`PROTOBUF_DIR`/`ABSL_DIR`/`UTF8_RANGE_DIR` remain exported for compatibility during migration, but workflow YAML must configure semantic builds through the single `CMAKE_PREFIX_PATH="$AXIOM_SEMANTIC_SDK_ROOT"` contract rather than calculating those directories itself.

---

### Task 1: Add the shared build-environment consumer helper

**Files:**
- Create: `tools/setup_build_environment.py`
- Create: `verification/tests/test_build_environment.py`
- Read/Reuse: `tools/semantic_fetch.py`
- Read/Reuse: `tools/bootstrap_deps.py`
- Read/Reuse: `semantic-toolchain.lock.json`

**Interfaces:**
- Consumes: existing `python3 tools/bootstrap_deps.py --core` and `python3 tools/semantic_fetch.py --target linux-x86_64 --destination .deps/protobuf`.
- Produces: `validate_semantic_install`, `setup_environment`, `write_github_env`, `write_facts`, plus the CLI/environment contract listed above.

- [ ] **Step 1: Write failing unit tests for canonical semantic paths and fail-closed validation**

Create `verification/tests/test_build_environment.py` with tests using a temporary fake SDK root:

```python
import json
from pathlib import Path
import tempfile
import unittest
from unittest import mock

import tools.setup_build_environment as build_env


class BuildEnvironmentTest(unittest.TestCase):
    def make_semantic_root(self, directory: str) -> Path:
        root = Path(directory) / "protobuf"
        (root / "bin").mkdir(parents=True)
        (root / "lib/cmake/protobuf").mkdir(parents=True)
        (root / "lib/cmake/absl").mkdir(parents=True)
        (root / "lib/cmake/utf8_range").mkdir(parents=True)
        (root / "bin/protoc").write_text("protoc\n", encoding="utf-8")
        (root / "lib/cmake/protobuf/protobuf-config.cmake").write_text("# protobuf\n", encoding="utf-8")
        (root / "lib/cmake/absl/abslConfig.cmake").write_text("# absl\n", encoding="utf-8")
        (root / "lib/cmake/utf8_range/utf8_range-config.cmake").write_text("# utf8\n", encoding="utf-8")
        return root

    def test_validate_semantic_install_returns_one_canonical_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            environment = build_env.validate_semantic_install(root)
            self.assertEqual(environment["AXIOM_SEMANTIC_SDK_ROOT"], str(root.resolve()))
            self.assertEqual(environment["CMAKE_PREFIX_PATH"], str(root.resolve()))
            self.assertEqual(environment["PROTOBUF_DIR"], str((root / "lib/cmake/protobuf").resolve()))
            self.assertEqual(environment["ABSL_DIR"], str((root / "lib/cmake/absl").resolve()))
            self.assertEqual(environment["UTF8_RANGE_DIR"], str((root / "lib/cmake/utf8_range").resolve()))
            self.assertEqual(environment["AXIOM_PROTOC"], str((root / "bin/protoc").resolve()))

    def test_validate_semantic_install_fails_when_package_contract_is_incomplete(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            (root / "lib/cmake/absl/abslConfig.cmake").unlink()
            with self.assertRaisesRegex(RuntimeError, "semantic SDK consumer contract"):
                build_env.validate_semantic_install(root)
```

- [ ] **Step 2: Run the new tests and verify they fail before implementation**

Run:

```bash
python3 -m unittest verification.tests.test_build_environment -v
```

Expected: FAIL because `tools.setup_build_environment` does not exist.

- [ ] **Step 3: Implement canonical semantic install validation and environment generation**

Create `tools/setup_build_environment.py` with the following concrete structure:

```python
#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import subprocess
import sys
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEPS_ROOT = ROOT / ".deps"
SEMANTIC_ROOT = DEPS_ROOT / "protobuf"
SEMANTIC_LOCK = ROOT / "semantic-toolchain.lock.json"


def file_sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for chunk in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(chunk)
    return digest.hexdigest()


def validate_semantic_install(root: Path) -> dict[str, str]:
    root = root.resolve()
    required = {
        "AXIOM_PROTOC": root / "bin/protoc",
        "PROTOBUF_DIR": root / "lib/cmake/protobuf",
        "ABSL_DIR": root / "lib/cmake/absl",
        "UTF8_RANGE_DIR": root / "lib/cmake/utf8_range",
    }
    required_files = (
        root / "bin/protoc",
        root / "lib/cmake/protobuf/protobuf-config.cmake",
        root / "lib/cmake/absl/abslConfig.cmake",
        root / "lib/cmake/utf8_range/utf8_range-config.cmake",
    )
    missing = [str(path) for path in required_files if not path.is_file()]
    if missing:
        raise RuntimeError("semantic SDK consumer contract is incomplete: " + ", ".join(missing))
    return {
        "AXIOM_DEPS_DIR": str(DEPS_ROOT.resolve()),
        "AXIOM_SEMANTIC_SDK_ROOT": str(root),
        "CMAKE_PREFIX_PATH": str(root),
        **{name: str(path.resolve()) for name, path in required.items()},
    }


def setup_environment(*, core: bool, semantic: bool, target: str) -> dict[str, Any]:
    if target != "linux-x86_64":
        raise RuntimeError(f"unsupported build-environment target: {target}")
    if core:
        subprocess.run([sys.executable, "tools/bootstrap_deps.py", "--core"], cwd=ROOT, check=True)
    semantic_fetch: dict[str, Any] | None = None
    environment = {"AXIOM_DEPS_DIR": str(DEPS_ROOT.resolve())}
    if semantic:
        result = subprocess.run(
            [sys.executable, "tools/semantic_fetch.py", "--target", target, "--destination", str(SEMANTIC_ROOT)],
            cwd=ROOT, check=True, capture_output=True, text=True,
        )
        if result.stdout:
            print(result.stdout, end="")
        semantic_fetch = json.loads(result.stdout.splitlines()[-1])
        environment.update(validate_semantic_install(SEMANTIC_ROOT))
    lock = json.loads(SEMANTIC_LOCK.read_text(encoding="utf-8")) if semantic else None
    return {
        "format": "axiom-build-environment-v1",
        "target": target,
        "environment": environment,
        "core": {"enabled": core, "depsLockSha256": file_sha256(ROOT / "deps.lock.json")},
        "semantic": None if not semantic else {
            "enabled": True,
            "lockSha256": file_sha256(SEMANTIC_LOCK),
            "releaseTag": lock["releaseTag"],
            "asset": lock["asset"],
            "sdkId": lock["sdkId"],
            "sha256": lock["sha256"],
            "fetch": semantic_fetch,
        },
    }


def write_github_env(path: Path, environment: dict[str, str]) -> None:
    with path.open("a", encoding="utf-8") as output:
        for name in (
            "AXIOM_DEPS_DIR", "AXIOM_SEMANTIC_SDK_ROOT", "CMAKE_PREFIX_PATH",
            "PROTOBUF_DIR", "ABSL_DIR", "UTF8_RANGE_DIR", "AXIOM_PROTOC",
        ):
            if name in environment:
                output.write(f"{name}={environment[name]}\n")


def write_facts(path: Path, facts: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(facts, indent=2, sort_keys=True) + "\n", encoding="utf-8")
```

Add the CLI parser at the bottom with `--core`, `--semantic`, `--target`, `--github-env`, and `--facts-output`, call `setup_environment`, optionally write both outputs, and print the facts JSON. Do not add a `--semantic-codec` source-build fallback.

- [ ] **Step 4: Add tests for GitHub env output and orchestration without network access**

Extend `verification/tests/test_build_environment.py`:

```python
    def test_write_github_env_exports_single_semantic_prefix(self):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            environment = build_env.validate_semantic_install(root)
            output = Path(directory) / "github-env"
            build_env.write_github_env(output, environment)
            text = output.read_text(encoding="utf-8")
            self.assertIn(f"AXIOM_SEMANTIC_SDK_ROOT={root.resolve()}\n", text)
            self.assertIn(f"CMAKE_PREFIX_PATH={root.resolve()}\n", text)
            self.assertEqual(text.count("CMAKE_PREFIX_PATH="), 1)

    @mock.patch("tools.setup_build_environment.subprocess.run")
    def test_setup_environment_never_requests_semantic_source_bootstrap(self, run):
        with tempfile.TemporaryDirectory() as directory:
            root = self.make_semantic_root(directory)
            original = build_env.SEMANTIC_ROOT
            build_env.SEMANTIC_ROOT = root
            run.side_effect = [
                mock.Mock(stdout=""),
                mock.Mock(stdout=json.dumps({"sdkId": "a" * 64, "asset": "asset.zip", "sha256": "b" * 64, "url": "https://example.invalid/asset.zip"}) + "\n"),
            ]
            try:
                build_env.setup_environment(core=True, semantic=True, target="linux-x86_64")
            finally:
                build_env.SEMANTIC_ROOT = original
            commands = [call.args[0] for call in run.call_args_list]
            self.assertIn([sys.executable, "tools/bootstrap_deps.py", "--core"], commands)
            self.assertFalse(any("--semantic-codec" in command for command in commands))
            self.assertTrue(any("tools/semantic_fetch.py" in command for command in commands))
```

Import `sys` in the test file.

- [ ] **Step 5: Run helper, semantic SDK, and lock unit contracts**

Run:

```bash
python3 -m unittest \
  verification.tests.test_build_environment \
  verification.tests.test_semantic_sdk \
  verification.tests.test_semantic_lock -v
```

Expected: PASS.

- [ ] **Step 6: Run whitespace validation and commit Task 1**

Run:

```bash
git diff --check
git add tools/setup_build_environment.py verification/tests/test_build_environment.py
git commit -m "ci: add build environment consumer setup"
```

Expected: clean diff check and one focused commit.

---

### Task 2: Migrate the general G1 semantic lane to the shared consumer contract

**Files:**
- Modify: `.github/workflows/g1-semantic-codec.yml`
- Modify: `verification/tests/test_ci_trigger_boundaries.py`

**Interfaces:**
- Consumes: Task 1 CLI and exported `AXIOM_SEMANTIC_SDK_ROOT`/`CMAKE_PREFIX_PATH`.
- Produces: `evidence-ci/g1-semantic-codec/build-environment.json` and a workflow with no workflow-local Protobuf/Abseil/utf8 path calculation.

- [ ] **Step 1: Add failing static tests for the semantic lane**

Extend `test_g1_semantic_lane_owns_semantic_inputs_without_poc03_dependency` with:

```python
        self.assertIn('"tools/setup_build_environment.py"', trigger)
        workflow = (WORKFLOWS / "g1-semantic-codec.yml").read_text(encoding="utf-8")
        self.assertIn("tools/setup_build_environment.py --core --semantic", workflow)
        self.assertIn('-DCMAKE_PREFIX_PATH="$AXIOM_SEMANTIC_SDK_ROOT"', workflow)
        self.assertNotIn("bootstrap_deps.py --semantic-codec", workflow)
        self.assertNotIn("-DProtobuf_DIR=", workflow)
        self.assertNotIn("-Dabsl_DIR=", workflow)
        self.assertNotIn("-Dutf8_range_DIR=", workflow)
```

- [ ] **Step 2: Run static contract and verify it fails**

Run:

```bash
python3 -m unittest verification.tests.test_ci_trigger_boundaries -v
```

Expected: FAIL because the workflow still owns package-specific directories and does not call the shared helper.

- [ ] **Step 3: Replace workflow-local semantic acquisition/wiring**

In `.github/workflows/g1-semantic-codec.yml`:

1. Add these paths to both PR and main push path lists:

```yaml
      - "tools/setup_build_environment.py"
      - "verification/tests/test_build_environment.py"
```

2. Remove job-level `PROTOBUF_DIR`, `ABSL_DIR`, and `UTF8_RANGE_DIR` variables.

3. Replace the current standalone `Fetch locked semantic toolchain Release asset` step and the later `python3 tools/bootstrap_deps.py --core` call with one early step after build tools are installed:

```yaml
      - name: Set up locked Axiom build environment
        run: |
          mkdir -p evidence-ci/g1-semantic-codec
          python3 tools/setup_build_environment.py \
            --core \
            --semantic \
            --target linux-x86_64 \
            --github-env "$GITHUB_ENV" \
            --facts-output evidence-ci/g1-semantic-codec/build-environment.json
```

4. Keep the existing `Record toolchain manifest` step, but use `$AXIOM_PROTOC` for the protoc version command:

```bash
"$AXIOM_PROTOC" --version > evidence-ci/g1-semantic-codec/protoc.txt
```

5. In the semantic CMake configure command, replace the three explicit package variables with:

```bash
-DCMAKE_PREFIX_PATH="$AXIOM_SEMANTIC_SDK_ROOT"
```

6. In descriptor/generator/semantic-contract steps, replace direct `.deps/protobuf/bin/protoc` references with `$AXIOM_PROTOC` where shell expansion is available. Keep paths embedded in Python/tool arguments semantically identical.

- [ ] **Step 4: Run static/unit tests**

Run:

```bash
python3 -m unittest \
  verification.tests.test_build_environment \
  verification.tests.test_semantic_sdk \
  verification.tests.test_semantic_lock \
  verification.tests.test_ci_trigger_boundaries -v
```

Expected: PASS.

- [ ] **Step 5: Verify the locked semantic Release can be consumed locally/hosted without source bootstrap**

Run on a network-capable Linux environment:

```bash
rm -rf .deps/protobuf out/build-env-semantic-smoke
python3 tools/setup_build_environment.py \
  --core --semantic --target linux-x86_64 \
  --facts-output out/build-environment-smoke.json
cmake -S . -B out/build-env-semantic-smoke -G Ninja \
  -DCMAKE_BUILD_TYPE=Debug \
  -DCANVAS_BUILD_POC01=OFF \
  -DCANVAS_BUILD_SEMANTIC=ON \
  -DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON \
  -DCMAKE_PREFIX_PATH="$PWD/.deps/protobuf"
cmake --build out/build-env-semantic-smoke --parallel
ctest --test-dir out/build-env-semantic-smoke --output-on-failure
```

Expected: semantic SDK download/verification succeeds, Axiom builds, CTest passes, and no Protobuf/Abseil source compilation appears in the helper output.

- [ ] **Step 6: Commit Task 2**

Run:

```bash
git diff --check
git add .github/workflows/g1-semantic-codec.yml verification/tests/test_ci_trigger_boundaries.py
git commit -m "ci: consume locked semantic build environment"
```

---

### Task 3: Add hosted build-environment and historical Skia compatibility proof

**Files:**
- Create: `.github/workflows/build-environment-contract.yml`
- Modify: `verification/tests/test_ci_trigger_boundaries.py`
- Reuse without modification: `r1-full-skia-sdk.lock.json`
- Reuse without modification: `tools/skia/fetch.py`
- Reuse without modification: `tools/skia/profiles/r1-full-v1.json`

**Interfaces:**
- Consumes: Task 1 helper and existing Skia consumer.
- Produces: a path-scoped hosted CI contract that proves semantic prebuilt consumption and historical Skia Release resolution without invoking either producer.

- [ ] **Step 1: Add failing static tests for the new hosted contract**

Add this test to `verification/tests/test_ci_trigger_boundaries.py`:

```python
    def test_build_environment_contract_is_consumer_only(self):
        workflow = (WORKFLOWS / "build-environment-contract.yml").read_text(encoding="utf-8")
        self.assertIn("tools/setup_build_environment.py --core --semantic", workflow)
        self.assertIn("tools/skia/fetch.py", workflow)
        self.assertIn("r1-full-skia-sdk.lock.json", workflow)
        self.assertIn("android-x86_64-gles3", workflow)
        self.assertNotIn("bootstrap_deps.py --semantic-codec", workflow)
        self.assertNotIn("tools/skia/build.py", workflow)
        self.assertNotIn("git-sync-deps", workflow)
        self.assertNotIn("gh release create", workflow)
```

- [ ] **Step 2: Run static contract and verify it fails because the workflow does not exist**

Run:

```bash
python3 -m unittest verification.tests.test_ci_trigger_boundaries -v
```

Expected: FAIL with missing `.github/workflows/build-environment-contract.yml`.

- [ ] **Step 3: Create the path-scoped hosted workflow**

Create `.github/workflows/build-environment-contract.yml` with two jobs:

```yaml
name: Build Environment Contract

on:
  pull_request:
    paths:
      - "deps.lock.json"
      - "semantic-toolchain.lock.json"
      - "r1-full-skia-sdk.lock.json"
      - "tools/setup_build_environment.py"
      - "tools/semantic_fetch.py"
      - "tools/semantic_sdk.py"
      - "tools/skia/fetch.py"
      - "tools/skia/consumer.py"
      - "verification/tests/test_build_environment.py"
      - "verification/tests/test_semantic_lock.py"
      - "verification/tests/test_semantic_sdk.py"
      - "verification/tests/test_ci_trigger_boundaries.py"
      - ".github/workflows/build-environment-contract.yml"
      - ".github/workflows/g1-semantic-codec.yml"
  push:
    branches: [main]
    paths:
      - "deps.lock.json"
      - "semantic-toolchain.lock.json"
      - "r1-full-skia-sdk.lock.json"
      - "tools/setup_build_environment.py"
      - "tools/semantic_fetch.py"
      - "tools/semantic_sdk.py"
      - "tools/skia/fetch.py"
      - "tools/skia/consumer.py"
      - "verification/tests/test_build_environment.py"
      - "verification/tests/test_semantic_lock.py"
      - "verification/tests/test_semantic_sdk.py"
      - "verification/tests/test_ci_trigger_boundaries.py"
      - ".github/workflows/build-environment-contract.yml"
      - ".github/workflows/g1-semantic-codec.yml"
  workflow_dispatch:

permissions:
  contents: read

jobs:
  semantic-consumer:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.12" }
      - name: Install build tools
        run: sudo apt-get update && sudo apt-get install -y ninja-build
      - name: Run build-environment contracts
        run: >-
          python3 -m unittest
          verification.tests.test_build_environment
          verification.tests.test_semantic_sdk
          verification.tests.test_semantic_lock -v
      - name: Consume locked semantic environment
        run: |
          python3 tools/setup_build_environment.py \
            --core --semantic --target linux-x86_64 \
            --github-env "$GITHUB_ENV" \
            --facts-output "$RUNNER_TEMP/build-environment.json"
      - name: Configure, build, and test Axiom semantic consumer
        run: |
          cmake -S . -B out/build-environment-contract -G Ninja \
            -DCMAKE_BUILD_TYPE=Debug \
            -DCANVAS_BUILD_POC01=OFF \
            -DCANVAS_BUILD_SEMANTIC=ON \
            -DCANVAS_SEMANTIC_ENABLE_PROTOBUF=ON \
            -DCMAKE_PREFIX_PATH="$AXIOM_SEMANTIC_SDK_ROOT"
          cmake --build out/build-environment-contract --parallel
          ctest --test-dir out/build-environment-contract --output-on-failure
      - uses: actions/upload-artifact@v4
        with:
          name: build-environment-${{ github.sha }}
          path: ${{ runner.temp }}/build-environment.json
          if-no-files-found: error
          retention-days: 14

  skia-historical-consumer:
    runs-on: ubuntu-24.04
    steps:
      - uses: actions/checkout@v4
      - uses: actions/setup-python@v5
        with: { python-version: "3.12" }
      - name: Validate locked historical R1 Full consumer
        run: |
          python3 -m unittest tools.skia.tests.test_consumer tools.skia.tests.test_sdk_tools -v
          python3 tools/skia/fetch.py \
            --profile tools/skia/profiles/r1-full-v1.json \
            --lock r1-full-skia-sdk.lock.json \
            --target android-x86_64-gles3 \
            --variant release \
            --summary-file "$GITHUB_STEP_SUMMARY"
          test ! -d .deps/skia
```

This intentionally fetches one existing release variant as the migration guard instead of duplicating the full 8-target R1 consumer matrix.

- [ ] **Step 4: Extend static trigger checks and run all relevant unit tests**

Run:

```bash
python3 -m unittest \
  verification.tests.test_build_environment \
  verification.tests.test_semantic_sdk \
  verification.tests.test_semantic_lock \
  verification.tests.test_ci_trigger_boundaries \
  tools.skia.tests.test_consumer \
  tools.skia.tests.test_sdk_tools -v
```

Expected: PASS.

- [ ] **Step 5: Confirm Skia Authority files are byte-for-byte untouched in this branch**

Run:

```bash
git diff --exit-code main...HEAD -- r1-full-skia-sdk.lock.json tools/skia/profiles/r1-full-v1.json tools/skia/build.py tools/skia/package.py tools/skia/publish_release.py
```

Expected: exit 0, no output.

- [ ] **Step 6: Commit Task 3**

Run:

```bash
git diff --check
git add .github/workflows/build-environment-contract.yml verification/tests/test_ci_trigger_boundaries.py
git commit -m "ci: verify immutable dependency consumers"
```

---

### Task 4: Validate the architecture branch in hosted CI and record timing evidence

**Files:**
- No runtime source changes.
- Review/verify: GitHub Actions runs for `.github/workflows/build-environment-contract.yml`, `.github/workflows/g1-semantic-codec.yml`, and `ci-boundary-contract.yml`.
- Optional documentation-only update if timing numbers are recorded: append a measured-results section to `docs/superpowers/specs/2026-09-07-build-environment-authority-design.md`.

**Interfaces:**
- Consumes: Tasks 1-3 commits.
- Produces: hosted proof that immutable semantic fetch replaces source compilation and historical Skia asset resolution still passes.

- [ ] **Step 1: Push `ci/build-environment-authority` and open a PR to `main`**

Expected PR scope: helper/tests/workflows/docs only; no `runtime/**`, `schema/**`, Skia lock/profile/producer changes.

- [ ] **Step 2: Inspect `Build Environment Contract / semantic-consumer` logs**

Acceptance checks:

```text
PASS: semantic-toolchain.lock.json asset is downloaded/verified
PASS: build-environment.json contains sdkId/asset/sha256/releaseTag/lockSha256
PASS: CMake configures with CMAKE_PREFIX_PATH rooted at .deps/protobuf
PASS: Axiom semantic build + CTest passes
ABSENT: bootstrap_deps.py --semantic-codec
ABSENT: Abseil source build
ABSENT: Protobuf source build
```

- [ ] **Step 3: Inspect `Build Environment Contract / skia-historical-consumer` logs**

Acceptance checks:

```text
PASS: r1-full-skia-sdk.lock.json is parsed by existing consumer tooling
PASS: historical tag/asset resolves through tools/skia/fetch.py
PASS: size + SHA-256 + SDK/manifest identity verification passes
PASS: .deps/skia does not exist
ABSENT: tools/skia/build.py
ABSENT: git-sync-deps
ABSENT: any Release publish operation
```

- [ ] **Step 4: Inspect G1 Semantic Codec hosted run**

Acceptance checks:

```text
PASS: existing descriptor/generation/differential/evidence semantics remain green
PASS: build-environment.json is uploaded under evidence-ci/g1-semantic-codec
ABSENT: semantic source bootstrap
ABSENT: workflow-local -DProtobuf_DIR/-Dabsl_DIR/-Dutf8_range_DIR
```

- [ ] **Step 5: Compare hosted timing with the previously observed source-bootstrap baseline**

Record at minimum:

```text
previous semantic third-party bootstrap: about 7m42s observed in GT-G1-07 run 34076178377
new semantic locked artifact fetch + verify: measured hosted duration
Axiom configure/build/test: measured hosted duration
```

Do not claim a speedup until the hosted run provides the new measured duration.

- [ ] **Step 6: Run final repository scope verification**

Run:

```bash
git diff --name-only main...HEAD
git diff --check main...HEAD
```

Expected: only the planned docs/tools/tests/workflow paths; no runtime/schema/Skia authority changes.

---

### Task 5: Apply the source-free semantic dependency path to the active GT-G1-07 exact-source workflow

**Files:**
- Modify on branch `codex/gt-g1-07-replay-inspector`: `.github/workflows/g1-07-exact-source.yml`
- Do not add `tools/setup_build_environment.py` to the GT-G1-07 branch in this task; that would expand its task source path set.

**Interfaces:**
- Consumes: the already-present and identical `semantic-toolchain.lock.json`, `tools/semantic_fetch.py`, and `tools/semantic_sdk.py` on the GT-G1-07 branch.
- Produces: the same protobuf-on/off Axiom build/test/evidence flow with locked semantic artifact acquisition instead of third-party source compilation.

- [ ] **Step 1: Make the smallest workflow-only migration**

In `.github/workflows/g1-07-exact-source.yml`, replace:

```yaml
      - name: Bootstrap locked core dependencies
        run: python3 tools/bootstrap_deps.py --core --semantic-codec
```

with:

```yaml
      - name: Set up locked core and semantic dependencies
        run: |
          set -euo pipefail
          python3 tools/bootstrap_deps.py --core
          python3 tools/semantic_fetch.py --target linux-x86_64
```

Keep the existing `PROTOBUF_DIR`, `ABSL_DIR`, and `UTF8_RANGE_DIR` variables for this task-local rollout so the only behavioral change is acquisition strategy. Do not alter runtime/test/evidence source paths or CMake/test selection.

- [ ] **Step 2: Verify the exact-source allowlist remains unchanged in meaning**

Check the workflow's generated expected source-path list still contains exactly the same GT-G1-07 task files. Because `.github/workflows/g1-07-exact-source.yml` is already an allowed path, this migration must not add any new source path.

- [ ] **Step 3: Commit only the workflow file on the GT-G1-07 execution branch**

Run:

```bash
git add .github/workflows/g1-07-exact-source.yml
git diff --cached --check
git commit -m "ci(g1-07): consume locked semantic toolchain"
```

Expected: one-file CI commit.

- [ ] **Step 4: Let the branch push trigger the existing GT-G1-07 Exact Source run and inspect it**

Acceptance checks:

```text
PASS: repository/source/anchor/workflow identity step
PASS: semantic Release fetch + SHA/SDK verification
PASS: protobuf-on configure/build
PASS: protobuf-off configure/build
PASS: focused + G1-06 + G1-05 + full semantic CTest matrices
PASS: evidence generation/upload
ABSENT: Abseil source compile
ABSENT: Protobuf source compile
```

- [ ] **Step 5: Compare the dependency setup duration to the prior GT-G1-07 run**

Use run `34076178377` as the source-bootstrap baseline (~7m42s dependency bootstrap). Record the new `Set up locked core and semantic dependencies` duration from the hosted job.

- [ ] **Step 6: If the hosted run fails, fail closed by classifying the failure before changing scope**

Allowed repair classes in this task are limited to:

```text
semantic Release fetch/verification
CMake dependency path wiring
workflow syntax/environment propagation
```

A runtime semantic/test failure is not authorized as part of this CI optimization and must not be repaired by changing production source.

---

## Plan Self-Review

### Spec coverage

- Producer/consumer separation: Tasks 1-3.
- Ordinary semantic source bootstrap removed: Tasks 1-2 and active rollout Task 5.
- Canonical dependency prefix and workflow path deduplication: Tasks 1-2.
- Machine-readable environment facts: Tasks 1-2 and hosted verification Task 4.
- Skia frozen and historical Release reusable: Task 3, with explicit byte-for-byte no-change check.
- Fail-closed behavior/no source fallback: Tasks 1, 3, and 5.
- Exact-source semantics preserved: Task 5 changes workflow acquisition only and keeps the existing source allowlist/test/evidence semantics.
- Cache non-authoritative: no cache implementation is added in v0.1.
- Hosted timing evidence: Tasks 4-5.

### Placeholder scan

No `TBD`, `TODO`, deferred implementation placeholder, or unspecified test requirement remains in this plan.

### Interface consistency

The helper names, CLI flags, exported environment names, facts filename, and CMake prefix contract are identical across Tasks 1-4. Task 5 intentionally uses the existing branch-local package-directory variables as a minimal live rollout and does not depend on the new helper file, avoiding GT-G1-07 source-scope expansion.
