package dev.mostorm.axiom.verification.android;

import android.app.Activity;
import android.os.Bundle;

public final class HarnessActivity extends Activity {
  private HarnessCanvasView canvas;

  @Override protected void onCreate(Bundle state) {
    super.onCreate(state);
    canvas = new HarnessCanvasView(this);
    setContentView(canvas);
  }

  public String runDeterministicProbe() {
    return canvas.runProbe();
  }

  @Override protected void onDestroy() {
    if (canvas != null) canvas.closeNative();
    super.onDestroy();
  }
}
