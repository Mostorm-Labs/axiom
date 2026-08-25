#!/usr/bin/env python3
"""Static integration checks for the Android visual smoke host."""

from pathlib import Path
import unittest


ROOT = Path(__file__).resolve().parents[1]


class AndroidVisualSmokeIntegrationTest(unittest.TestCase):
    def test_manifest_exposes_dedicated_visual_activity(self) -> None:
        manifest = (ROOT / "platform/android/app/src/main/AndroidManifest.xml").read_text()
        self.assertIn(".CanvasVisualSmokeActivity", manifest)
        self.assertIn(
            'android:exported="true"',
            manifest.split(".CanvasVisualSmokeActivity", 1)[1].split("</activity>", 1)[0],
        )
        self.assertIn(
            'android:screenOrientation="landscape"',
            manifest.split(".CanvasVisualSmokeActivity", 1)[1].split("</activity>", 1)[0],
        )

    def test_visual_view_uses_native_canvas_and_assets(self) -> None:
        source = (ROOT / "platform/android/app/src/main/java/dev/mostorm/canvas/CanvasVisualSmokeView.java").read_text()
        self.assertIn("SurfaceView", source)
        self.assertIn('readAsset("Roboto-Regular.ttf")', source)
        self.assertIn('readAsset("scene.ndjson")', source)
        self.assertIn("setFixedSize(800, 600)", source)
        self.assertIn("nativeRunVisualSmoke", source)

    def test_jni_calls_existing_gles_adapter_and_writes_rgba(self) -> None:
        source = (ROOT / "platform/android/android_jni.cpp").read_text()
        self.assertIn("CanvasVisualSmokeView_nativeRunVisualSmoke", source)
        self.assertIn("CanvasVisualSmokeView_nativeDetach", source)
        self.assertIn("CanvasPocView_nativeRunAcceptance", source)
        self.assertIn("CanvasPocView_nativeDestroy", source)
        self.assertIn("AndroidGlesAdapter", source)
        self.assertIn("adapter.Render(*document, &rgba)", source)
        self.assertIn("rgba_bytes", source)

    def test_visual_runner_does_not_mutate_system_display_state(self) -> None:
        source = (ROOT / "tools/run_android_visual_smoke.py").read_text()
        self.assertNotIn('"wm", "size", "800x600"', source)
        self.assertNotIn('"wm", "size", "reset"', source)
        self.assertIn('"exec-out", "screencap", "-p"', source)
        self.assertIn('"manifest.json"', source)


if __name__ == "__main__":
    unittest.main()
