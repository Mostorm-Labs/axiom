package dev.mostorm.canvas;

import android.app.Activity;
import android.os.Bundle;
import android.view.View;
import android.view.WindowManager;

/** Dedicated POC-01 visual smoke host; it is not the G0 instrumentation host. */
public final class CanvasVisualSmokeActivity extends Activity {
    private CanvasVisualSmokeView canvasView;

    @Override
    protected void onCreate(Bundle state) {
        super.onCreate(state);
        getWindow().setFlags(WindowManager.LayoutParams.FLAG_FULLSCREEN,
                WindowManager.LayoutParams.FLAG_FULLSCREEN);
        getWindow().getDecorView().setSystemUiVisibility(
                View.SYSTEM_UI_FLAG_FULLSCREEN
                        | View.SYSTEM_UI_FLAG_HIDE_NAVIGATION
                        | View.SYSTEM_UI_FLAG_IMMERSIVE_STICKY);
        canvasView = new CanvasVisualSmokeView(this);
        setContentView(canvasView);
    }

    @Override
    protected void onDestroy() {
        if (canvasView != null) canvasView.destroyRuntime();
        super.onDestroy();
    }
}
