from pathlib import Path


root = Path(__file__).resolve().parents[1] / "platform" / "android"
assert (root / "settings.gradle.kts").is_file()
assert (root / "build.gradle.kts").is_file()
assert (root / "app" / "build.gradle.kts").is_file()
assert (root / "app" / "src" / "main" / "AndroidManifest.xml").is_file()
assert (root / "app" / "src" / "main" / "java").is_dir()
manifest = (root / "app" / "src" / "main" / "AndroidManifest.xml").read_text()
gradle = (root / "app" / "build.gradle.kts").read_text()
view = next((root / "app" / "src" / "main" / "java").rglob("InkPlaygroundView.java")).read_text()
cmake = (root.parents[1] / "CMakeLists.txt").read_text()
assert "android.intent.action.MAIN" in manifest
assert "android.intent.category.LAUNCHER" in manifest
assert "axiom_ink_playground_android" in gradle
assert "arm64-v8a" in gradle
assert "getHistorySize" in view
assert "getHistoricalX" in view
assert "sequence = 0" not in view.replace("private long sequence;", "")
assert "-z,max-page-size=16384" in cmake
assert "-z,common-page-size=16384" in cmake
