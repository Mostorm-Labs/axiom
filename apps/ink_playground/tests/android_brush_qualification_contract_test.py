from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
JAVA = next((ROOT / "platform" / "android" / "app" / "src" / "main" / "java").rglob("InkPlaygroundView.java"))
ACTIVITY = next((ROOT / "platform" / "android" / "app" / "src" / "main" / "java").rglob("MainActivity.java"))
BRIDGE = (ROOT / "platform" / "android" / "android_jni.cpp").read_text()
NATIVE = (ROOT / "platform" / "android" / "bridge.cpp").read_text()
JAVA_TEXT = JAVA.read_text()
ACTIVITY_TEXT = ACTIVITY.read_text(encoding="utf-8")


assert "nativeBrushBegin" in JAVA_TEXT
assert "nativeBrushSample" in JAVA_TEXT
assert "nativeBrushFinish" in JAVA_TEXT
assert "brushSelector" in ACTIVITY_TEXT
assert "selectBrushFamily" in ACTIVITY_TEXT
assert "selectedBrushFamily" in JAVA_TEXT
assert "drawTexturedBrush" in JAVA_TEXT
assert "case 2:" in JAVA_TEXT
assert "case 3:" in JAVA_TEXT
assert "case 4:" in JAVA_TEXT
assert "case 5:" in JAVA_TEXT
assert "毛笔质感" in ACTIVITY_TEXT
assert "g4-5-android" in JAVA_TEXT
assert "programmable_brush" in NATIVE
assert "BrushFamily" in NATIVE
assert "family <= 7" in NATIVE
assert "canonicalMutation" in NATIVE

# Performance/visual-continuity contract: the Android view must retain the
# last committed Skia frame and avoid a full JNI readback on every UI frame
# while a stroke is in flight.  The preview and the committed frame are still
# required to use the same BrushPrimitive parameters; this contract only
# checks that the expensive full-frame transfer is not on the hot path.
assert "committedBitmap" in JAVA_TEXT
assert "nativeBrushRgba" in JAVA_TEXT
assert "activeStrokes.isEmpty()" in JAVA_TEXT
assert "committedBitmapValid" in JAVA_TEXT
