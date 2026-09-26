from pathlib import Path


ROOT = Path(__file__).resolve().parents[1]
JAVA = next((ROOT / "platform" / "android" / "app" / "src" / "main" / "java").rglob("InkPlaygroundView.java"))
ACTIVITY = next((ROOT / "platform" / "android" / "app" / "src" / "main" / "java").rglob("MainActivity.java"))
BRIDGE = (ROOT / "platform" / "android" / "android_jni.cpp").read_text()
NATIVE = (ROOT / "platform" / "android" / "bridge.cpp").read_text()
JAVA_TEXT = JAVA.read_text()
ACTIVITY_TEXT = ACTIVITY.read_text()
RENDER_SURFACE = next((ROOT / "platform" / "android" / "app" / "src" / "main" / "java").rglob("InkRenderSurfaceView.java"))
PROVIDER = ROOT / "platform" / "android" / "android_egl_surface_provider.cpp"


assert "nativePlatformBatch" in JAVA_TEXT
assert "nativePlatformBatch" in (ROOT / "platform" / "android" / "android_jni.cpp").read_text()
assert "(viewX - viewportTranslationX)" not in JAVA_TEXT
assert "(viewY - viewportTranslationY)" not in JAVA_TEXT
assert "contentX" not in BRIDGE
assert "contentY" not in BRIDGE
assert "brushSelector" in ACTIVITY_TEXT
assert "selectBrushFamily" in ACTIVITY_TEXT
assert "selectedBrushFamily" in JAVA_TEXT
assert "drawTexturedBrush" not in JAVA_TEXT
assert "BrushRuntime" not in JAVA_TEXT
assert "毛笔质感" in ACTIVITY_TEXT
assert "g4-5-android" in JAVA_TEXT
assert "InkPlaygroundHost" in NATIVE
assert "PlatformPointerBatch" in NATIVE
assert "BrushRuntime" not in NATIVE
assert "BrushDefinition" not in NATIVE
assert "BrushPrimitive" not in NATIVE
assert "BrushFamily" not in NATIVE
assert "programmable_brush" not in NATIVE
assert RENDER_SURFACE.is_file()
assert PROVIDER.is_file()
assert "SurfaceView" in RENDER_SURFACE.read_text()
assert "nativeAttachSurface" in JAVA_TEXT
assert "nativeDetachSurface" in JAVA_TEXT
assert "eglSwapBuffers" in PROVIDER.read_text()
assert "GrDirectContexts::MakeGL" in PROVIDER.read_text()
assert "SkSurfaces::WrapBackendRenderTarget" in PROVIDER.read_text()
# A SurfaceView can be destroyed and recreated without destroying the Runtime
# provider registry.  The bridge must retain the provider identity after lose
# so the next surfaceChanged can attach+rebind the same provider instead of
# attempting a duplicate profile registration and leaving canonical rendering
# permanently lost.
assert "provider = nullptr" not in NATIVE.split("void axiom_ink_android_detach_surface", 1)[1].split("int axiom_ink_android_present_preview", 1)[0]
assert "existing->attach(window, width, height)" in NATIVE
assert "rebindSurface()" in NATIVE
# GLES/Skia window providers do not expose readback.  Evidence capture must
# fail closed before touching the EGL context; it must never call the
# production canonical present path from the background evidence executor.
brush_render = NATIVE.split("int axiom_ink_android_brush_render", 1)[1].split(
    "int axiom_ink_android_preview_render", 1)[0]
assert "supportsReadback" in brush_render
assert brush_render.index("supportsReadback") < brush_render.index("presentCanonicalFrame")
on_draw = JAVA_TEXT.split("@Override protected void onDraw", 1)[1].split("@Override", 1)[0]
assert "nativeBrushRgba" not in on_draw
assert "Bitmap.createBitmap" not in on_draw
