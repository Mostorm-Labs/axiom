from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
MAIN = (ROOT / "platform" / "windows" / "main.cpp").read_text()
SKIA_HEADER = (ROOT.parent.parent / "runtime" / "render" / "include" /
               "canvas" / "render" / "skia_ink_backend.hpp").read_text()

# Windows production preview must use the same Skia render adapter as the
# other clients.  The native layer may present pixels, but it may not derive
# or rasterize preview geometry itself.
assert "submitPreview" in MAIN
assert "CreateWindowsBackend" not in MAIN
assert "appendPreviewSamples" not in MAIN
assert "MoveToEx" not in MAIN
assert "LineTo" not in MAIN
assert "submitPreview" in SKIA_HEADER
