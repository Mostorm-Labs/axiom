from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
DEBUG_CMAKE = ROOT / "debug_ui" / "CMakeLists.txt"


def test_windows_debug_ui_compiles_utf8_source_as_utf8():
    source = DEBUG_CMAKE.read_text(encoding="utf-8")
    assert "/utf-8" in source
