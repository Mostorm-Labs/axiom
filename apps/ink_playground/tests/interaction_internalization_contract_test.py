from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
BRIDGE = ROOT / "platform" / "web" / "bridge.cpp"
WINDOWS = ROOT / "platform" / "windows" / "main.cpp"
HOST = ROOT / "common" / "ink_playground_host.cpp"
HOST_HEADER = ROOT / "common" / "ink_playground_host.hpp"


def test_web_platform_batch_does_not_commit_or_cancel_sessions_directly():
    source = BRIDGE.read_text(encoding="utf-8")
    submit = source.split("int submitPlatformBatch", 1)[1].split("}\n}\n\nextern \"C\"", 1)[0]
    assert "presentCanonicalFrame" not in submit
    assert "cancelBrushSession" not in submit


def test_windows_main_has_no_independent_status_hud():
    source = WINDOWS.read_text(encoding="utf-8")
    assert "TextOutW(" not in source
    assert "preview_present:" not in source
    assert "Arc preview:" not in source
    assert "runtime preview:" not in source
    assert "SPACE = preview" not in source


def test_host_exposes_runtime_dispatch_boundary():
    source = HOST.read_text(encoding="utf-8") + HOST_HEADER.read_text(encoding="utf-8")
    assert "dispatchRuntime_" in source
    assert "InteractionDispatchPort" in source or "InteractionDispatchRuntime" in source
