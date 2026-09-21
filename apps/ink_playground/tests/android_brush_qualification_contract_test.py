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
