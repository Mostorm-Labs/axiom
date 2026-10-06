"""Frozen C1 ownership oracle.

This intentionally fails against the pre-C1 tree.  It is the RED preflight
for the common platform ingress and migration boundary.
"""
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
RUNTIME_INPUT = ROOT / "runtime" / "input"
PLAYGROUND = ROOT / "apps" / "ink_playground"


def read_source(path: Path) -> str:
    """Read repository sources deterministically across Windows locales."""
    return path.read_text(encoding="utf-8")


def test_common_ingress_contract_exists():
    required = [
        RUNTIME_INPUT / "include/canvas/input/sample_provenance.hpp",
        RUNTIME_INPUT / "include/canvas/input/platform_input_contract.hpp",
        RUNTIME_INPUT / "include/canvas/input/input_capture_gate.hpp",
        RUNTIME_INPUT / "include/canvas/input/platform_interaction_ingress.hpp",
        RUNTIME_INPUT / "src/platform_interaction_ingress.cpp",
    ]
    missing = [str(path.relative_to(ROOT)) for path in required if not path.is_file()]
    assert not missing, f"C1 common ingress files missing: {missing}"


def test_platforms_submit_to_common_ingress():
    web = read_source(PLAYGROUND / "platform/web/bridge.cpp")
    android = read_source(PLAYGROUND / "platform/android/bridge.cpp")
    windows = read_source(PLAYGROUND / "platform/windows/main.cpp")
    assert "PlatformPointerBatch" in web
    assert "PlatformPointerBatch" in android
    assert "PlatformPointerBatch" in windows


def test_platforms_do_not_own_canonical_lifecycle_or_arc_bus():
    web = read_source(PLAYGROUND / "platform/web/index.html")
    android = read_source(PLAYGROUND / "platform/android/bridge.cpp")
    android_java = read_source(next((PLAYGROUND / "platform/android/app/src/main/java").rglob("InkPlaygroundView.java")))
    windows = read_source(PLAYGROUND / "platform/windows/main.cpp")
    assert "_axiom_ink_platform_batch" in web
    for token in ("_axiom_ink_begin_pointer", "_axiom_ink_pointer_sample_phase",
                  "_axiom_ink_commit_pointer", "_axiom_ink_brush_begin",
                  "_axiom_ink_brush_sample", "_axiom_ink_brush_finish"):
        assert token not in web
    assert "ActivePointerRegistry" not in android
    assert "arc_pointer_sample_v0" not in android
    for token in ("nativeBegin", "nativeMotion", "nativeCommit", "nativeBrushBegin",
                  "nativeBrushSample", "nativeBrushFinish"):
        assert token not in android_java
    assert "ActivePointerRegistry" not in windows
    assert "arc_pointer_sample_v0" not in windows


def test_runtime_input_is_arc_independent():
    for path in (RUNTIME_INPUT / "include").rglob("*"):
        if path.is_file():
            assert "arc/" not in path.read_text(errors="ignore"), path
    for path in (RUNTIME_INPUT / "src").rglob("*"):
        if path.is_file():
            assert "arc/" not in path.read_text(errors="ignore"), path


def test_apple_adapter_contract_exists():
    required = [
        PLAYGROUND / "platform/apple/apple_input_adapter.hpp",
        PLAYGROUND / "platform/apple/apple_input_adapter.cpp",
    ]
    missing = [str(path.relative_to(ROOT)) for path in required if not path.is_file()]
    assert not missing, f"C1 Apple adapter files missing: {missing}"
