#!/usr/bin/env python3
"""C2 RED precondition for the task-anchor brush implementation.

This probe intentionally runs against the legacy anchor surface.  It exits zero
only when every frozen pre-change gap is observed as RED; it never mutates
production sources or accepts an implementation result.
"""
from __future__ import annotations

import json
import os
from pathlib import Path
import subprocess
import tarfile
import tempfile
import inspect

ROOT = Path(__file__).resolve().parents[3]
ANCHOR = "4144dfd80b665df435d141382de05196d3b8eb0f"

def materialize_anchor(destination: Path) -> None:
    archive = subprocess.run(
        ["git", "archive", ANCHOR], cwd=ROOT, check=True, stdout=subprocess.PIPE
    )
    archive_path = destination / "anchor.tar"
    archive_path.write_bytes(archive.stdout)
    with tarfile.open(archive_path) as stream:
        target = destination / "source"
        # Python 3.12+ supports tarfile's data filter; older hosted/macOS
        # runners do not.  git archive is trusted input, so retain the same
        # extraction semantics while keeping the frozen oracle runnable.
        if "filter" in inspect.signature(stream.extractall).parameters:
            stream.extractall(target, filter="data")
        else:
            stream.extractall(target)

def source_text(anchor_root: Path, path: str) -> str:
    return (anchor_root / path).read_text(encoding="utf-8")

def run_legacy_runtime_probe(anchor_root: Path, probe_root: Path) -> dict[str, object]:
    source = r'''
#include "canvas/ink/programmable_brush.hpp"
#include <iostream>
#include <vector>
int main() {
  using namespace canvas::ink;
  BrushDefinition definition;
  definition.definitionId = 1;
  definition.nominalSize = 16.0F;
  const auto compiled = BrushCompiler{}.compile(definition, {});
  if (!compiled) return 2;
  ResourceCatalog resources;
  BrushRuntime runtime(resources);
  if (!runtime.begin({1}, *compiled.program, 42)) return 3;
  const std::vector<BrushInputSample> first{{0,0,.5F,0,0,1},{8,0,.5F,0,0,2}};
  const auto a = runtime.append({1}, first);
  if (!a) return 4;
  const std::vector<BrushInputSample> second{{16,0,.5F,0,0,3}};
  const auto b = runtime.append({1}, second);
  if (!b) return 5;
  std::cout << "{\"first_preview_count\":" << a.preview.primitives.size()
            << ",\"second_preview_count\":" << b.preview.primitives.size()
            << ",\"full_history_re_evaluated\":"
            << (b.preview.primitives.size() == 3 ? "true" : "false") << "}\n";
  return b.preview.primitives.size() == 3 ? 0 : 6;
}
'''
    with tempfile.TemporaryDirectory(prefix="axiom-c2-red-") as td:
        td = Path(td)
        cpp = td / "probe.cpp"
        exe = td / "probe"
        cpp.write_text(source, encoding="utf-8")
        compile_cmd = [
            os.environ.get("CXX", "c++"), "-std=c++20", "-O0", "-I", str(anchor_root / "runtime/ink/include"),
            str(cpp), str(anchor_root / "runtime/ink/src/programmable_brush.cpp"), "-o", str(exe),
        ]
        build = subprocess.run(compile_cmd, cwd=ROOT, text=True, capture_output=True)
        if build.returncode != 0:
            return {"status": "probe_compile_failed", "command": compile_cmd,
                    "exit_code": build.returncode, "stderr": build.stderr[-4000:]}
        run = subprocess.run([str(exe)], cwd=ROOT, text=True, capture_output=True)
        observed = {}
        try:
            observed = json.loads(run.stdout)
        except json.JSONDecodeError:
            observed = {"raw_stdout": run.stdout.strip()}
        observed.update({"status": "executed", "command": compile_cmd + ["&&", str(exe)],
                         "exit_code": run.returncode})
        return observed

def main() -> int:
    with tempfile.TemporaryDirectory(prefix="axiom-c2-red-anchor-") as td:
        workspace = Path(td)
        materialize_anchor(workspace)
        anchor_root = workspace / "source"
        missing_new_surface = not (anchor_root / "runtime/ink/include/canvas/ink/brush_session.hpp").exists()
        web = source_text(anchor_root, "apps/ink_playground/platform/web/bridge.cpp")
        android = source_text(anchor_root, "apps/ink_playground/platform/android/bridge.cpp")
        windows = source_text(anchor_root, "apps/ink_playground/platform/windows/main.cpp")
        schema_missing = not (anchor_root / "schema/axiom/v1/proto/auditoryworks/axiom/v1/brush_engine.proto").exists()
        legacy_owner_tokens = ["BrushRuntime", "BrushDefinition", "BrushPrimitive"]
        platform_hits = {
            "web": any(token in web for token in legacy_owner_tokens),
            "android": any(token in android for token in legacy_owner_tokens),
            "windows": any(token in windows for token in legacy_owner_tokens),
        }
        runtime_probe = run_legacy_runtime_probe(anchor_root, workspace)
    full_history = runtime_probe.get("full_history_re_evaluated") is True
    # The anchor API exposes only full preview vectors on every append; the
    # probe therefore treats this as the frozen observable one-shot path.
    observed = {
        "common_preview_contract_missing": "RED" if missing_new_surface else "GREEN",
        "move_append_full_history_evaluation": "RED" if full_history else "GREEN",
        "web_platform_brush_runtime_ownership": "RED" if platform_hits["web"] else "GREEN",
        "android_platform_brush_runtime_ownership": "RED" if platform_hits["android"] else "GREEN",
        "windows_raw_pointer_preview_derivation": "RED" if "preview" in windows and "appendPreviewSamples" in windows else "GREEN",
        "new_wire_schema_carrier_missing": "RED" if schema_missing else "GREEN",
    }
    expected_red = set(observed)
    unexpected_green = sorted(name for name, value in observed.items() if value != "RED")
    result = {
        "schema_version": "0.1",
        "kind": "C2_RED_ORACLE",
        "anchor_surface": "legacy task-anchor implementation",
        "anchor_revision": ANCHOR,
        "expected_red": sorted(expected_red),
        "observed": observed,
        "platform_source_hits": platform_hits,
        "legacy_runtime_probe": runtime_probe,
        "unexpected_green": unexpected_green,
        "production_mutation": False,
        "exit_code": 0 if not unexpected_green else 1,
    }
    print(json.dumps(result, sort_keys=True))
    return result["exit_code"]

if __name__ == "__main__":
    raise SystemExit(main())
