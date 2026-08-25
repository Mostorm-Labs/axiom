package dev.mostorm.canvas;

import android.content.Context;
import android.util.Log;
import android.view.Surface;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

import java.io.ByteArrayOutputStream;
import java.io.IOException;
import java.io.InputStream;

/** Native CanvasView for the independent POC-01 Android visual smoke. */
public final class CanvasVisualSmokeView extends SurfaceView implements SurfaceHolder.Callback {
    static {
        System.loadLibrary("canvas_poc01_android");
    }

    private final byte[] checker;
    private final byte[] font;
    private final byte[] replay;
    private boolean started;

    public CanvasVisualSmokeView(Context context) {
        super(context);
        getHolder().addCallback(this);
        // Keep the POC fixture at its canonical size without changing the
        // device's global wm size or density. The Android shell remains in
        // its original portrait/landscape configuration after this smoke.
        getHolder().setFixedSize(800, 600);
        try {
            checker = readAsset("checker.png");
            font = readAsset("Roboto-Regular.ttf");
            replay = readAsset("scene.ndjson");
        } catch (IOException error) {
            throw new IllegalStateException("POC fixture load failed", error);
        }
    }

    private byte[] readAsset(String name) throws IOException {
        try (InputStream input = getContext().getAssets().open(name);
             ByteArrayOutputStream output = new ByteArrayOutputStream()) {
            byte[] buffer = new byte[16 * 1024];
            int count;
            while ((count = input.read(buffer)) >= 0) output.write(buffer, 0, count);
            return output.toByteArray();
        }
    }

    @Override public void surfaceCreated(SurfaceHolder holder) {}

    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        if (started || width <= 0 || height <= 0) return;
        started = true;
        java.io.File directory = getContext().getExternalFilesDir(null);
        if (directory == null) {
            Log.e("CanvasPOC01", "CANVAS_POC01_VISUAL_FAILURE output directory unavailable");
            return;
        }
        String output = new java.io.File(directory, "android-visual-smoke.rgba").getAbsolutePath();
        new Thread(() -> {
            String result = nativeRunVisualSmoke(
                    holder.getSurface(), width, height, checker, font, replay, output);
            if (result.startsWith("{")) {
                Log.i("CanvasPOC01", "CANVAS_POC01_VISUAL_RESULT " + result);
            } else {
                Log.e("CanvasPOC01", "CANVAS_POC01_VISUAL_FAILURE " + result);
            }
        }, "CanvasPOC01VisualSmoke").start();
    }

    @Override public void surfaceDestroyed(SurfaceHolder holder) { nativeDetach(); }

    public void destroyRuntime() { nativeDestroy(); }

    private native String nativeRunVisualSmoke(
            Surface surface, int width, int height,
            byte[] checker, byte[] font, byte[] replay, String outputPath);
    private native void nativeDetach();
    private native void nativeDestroy();
}
