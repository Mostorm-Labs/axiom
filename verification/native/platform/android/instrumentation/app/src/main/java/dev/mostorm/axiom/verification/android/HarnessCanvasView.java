package dev.mostorm.axiom.verification.android;

import android.content.Context;
import android.view.MotionEvent;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

public final class HarnessCanvasView extends SurfaceView implements SurfaceHolder.Callback {
  static {
    System.loadLibrary("axiom_verification_android_jni");
  }

  private long nativeScope;

  public HarnessCanvasView(Context context) {
    super(context);
    getHolder().addCallback(this);
    setFocusable(true);
    nativeScope = nativeCreate();
  }

  @Override public void surfaceCreated(SurfaceHolder holder) {
    nativeSurfaceAvailable(holder.getSurface(), getWidth(), getHeight());
  }

  @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
    nativeMetricsChanged(width, height, getResources().getDisplayMetrics().density, getResources().getConfiguration().orientation);
  }

  @Override public void surfaceDestroyed(SurfaceHolder holder) {
    nativeSurfaceLost();
  }

  @Override public boolean onTouchEvent(MotionEvent event) {
    if (event.getActionMasked() == MotionEvent.ACTION_MOVE ||
        event.getActionMasked() == MotionEvent.ACTION_DOWN ||
        event.getActionMasked() == MotionEvent.ACTION_UP) {
      int history = event.getHistorySize();
      float[] samples = new float[(history + 1) * 7];
      for (int i = 0; i < history; i++) {
        int offset = i * 7;
        samples[offset] = event.getHistoricalX(0, i);
        samples[offset + 1] = event.getHistoricalY(0, i);
        samples[offset + 2] = event.getHistoricalPressure(0, i);
        samples[offset + 3] = event.getHistoricalAxisValue(MotionEvent.AXIS_TILT, 0, i);
        samples[offset + 4] = 0.0f;
        samples[offset + 5] = event.getHistoricalEventTime(i) * 1000000.0f;
        samples[offset + 6] = event.getToolType(0);
      }
      int offset = history * 7;
      samples[offset] = event.getX();
      samples[offset + 1] = event.getY();
      samples[offset + 2] = event.getPressure();
      samples[offset + 3] = event.getAxisValue(MotionEvent.AXIS_TILT);
      samples[offset + 4] = 0.0f;
      samples[offset + 5] = event.getEventTime() * 1000000.0f;
      samples[offset + 6] = event.getToolType(0);
      nativePointerBatch(samples, samples.length / 7);
    }
    return true;
  }

  public String runProbe() {
    nativeCreateCanvas();
    nativeAttachHost();
    nativeSurfaceAvailable(getHolder().getSurface(), getWidth(), getHeight());
    nativeAttachDocument();
    nativeBackground();
    nativeForeground();
    nativeSurfaceLost();
    nativeSurfaceAvailable(getHolder().getSurface(), getWidth(), getHeight());
    nativeDeviceLost();
    nativeDeviceRecover();
    long start = 1L;
    MotionEvent pointer = MotionEvent.obtain(
        start, start, MotionEvent.ACTION_MOVE, 10.0f, 20.0f, 0);
    pointer.addBatch(2L, 12.0f, 22.0f, 0.6f, 1.0f, 0);
    pointer.addBatch(3L, 16.0f, 26.0f, 0.9f, 1.0f, 0);
    onTouchEvent(pointer);
    pointer.recycle();
    return nativeSnapshotJson();
  }

  public void closeNative() {
    if (nativeScope != 0) {
      nativeDestroyCanvas();
      nativeDestroy();
      nativeScope = 0;
    }
  }

  private native long nativeCreate();
  private native void nativeDestroy();
  private native void nativeCreateCanvas();
  private native void nativeAttachHost();
  private native void nativeAttachDocument();
  private native void nativeBackground();
  private native void nativeForeground();
  private native void nativeSurfaceAvailable(Object surface, int width, int height);
  private native void nativeSurfaceLost();
  private native void nativeMetricsChanged(int width, int height, float density, int orientation);
  private native void nativeDeviceLost();
  private native void nativeDeviceRecover();
  private native void nativePointerBatch(float[] samples, int count);
  private native void nativeDestroyCanvas();
  private native String nativeSnapshotJson();
}
