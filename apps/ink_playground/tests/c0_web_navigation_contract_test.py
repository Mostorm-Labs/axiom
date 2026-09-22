from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
PAGE = ROOT / "platform" / "web" / "index.html"
BRIDGE = ROOT / "platform" / "web" / "bridge.cpp"
CMAKE = ROOT / "CMakeLists.txt"

page = PAGE.read_text(encoding="utf-8")
bridge = BRIDGE.read_text(encoding="utf-8")
cmake = CMAKE.read_text(encoding="utf-8")

# C0 ownership oracle: browser code may normalize events, but cumulative
# viewport state and navigation formulas must live in common runtime.
assert "let trackpadScale" not in page
assert "let trackpadTranslationX" not in page
assert "let trackpadTranslationY" not in page
assert "_axiom_ink_apply_viewport_wheel_pan" in page
assert "_axiom_ink_apply_viewport_ctrl_wheel_zoom" in page
assert "_axiom_ink_apply_viewport_gesture" in page
assert "axiom_ink_apply_viewport_wheel_pan" in bridge
assert "axiom_ink_apply_viewport_ctrl_wheel_zoom" in bridge
assert "axiom_ink_apply_viewport_gesture" in bridge
assert "_axiom_ink_apply_viewport_wheel_pan" not in cmake
assert "_axiom_ink_apply_viewport_ctrl_wheel_zoom" not in cmake
assert "_axiom_ink_apply_viewport_gesture" not in cmake

# Frozen navigation mapping remains explicit at the platform boundary.
assert "deltaX" in page and "deltaY" in page
assert "Math.exp(-event.deltaY * 0.01)" in page
assert "gestureStartScale" in page
