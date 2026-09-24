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
import tempfile

ROOT = Path(__file__).resolve().parents[3]

def source_text(path: str) -> str:
    return (ROOT / path).read_text(encoding="utf-8")

def run_legacy_runtime_probe() -> dict[str, object]:
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
            os.environ.get("CXX", "c++"), "-std=c++20", "-O0", "-I", str(ROOT / "runtime/ink/include"),
            str(cpp), str(ROOT / "runtime/ink/src/programmable_brush.cpp"), "-o", str(exe),
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
    missing_new_surface = not (ROOT / "runtime/ink/include/canvas/ink/brush_session.hpp").exists()
    host = source_text("apps/ink_playground/common/ink_playground_host.cpp")
    web = source_text("apps/ink_playground/platform/web/bridge.cpp")
    android = source_text("apps/ink_playground/platform/android/bridge.cpp")
    windows = source_text("apps/ink_playground/platform/windows/main.cpp")
    schema_missing = not (ROOT / "schema/axiom/v1/proto/auditoryworks/axiom/v1/brush_engine.proto").exists()
    legacy_owner_tokens = ["BrushRuntime", "BrushDefinition", "BrushPrimitive"]
    platform_hits = {
        "web": any(token in web for token in legacy_owner_tokens),
        "android": any(token in android for token in legacy_owner_tokens),
        "windows": any(token in windows for token in legacy_owner_tokens),
    }
    runtime_probe = run_legacy_runtime_probe()
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
