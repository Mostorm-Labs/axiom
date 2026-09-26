package dev.mostorm.axiom.inkplayground;

import android.graphics.PixelFormat;
import android.view.SurfaceHolder;
import android.view.SurfaceView;

/** Native presentation target only; all geometry is produced by Runtime and Skia. */
public final class InkRenderSurfaceView extends SurfaceView implements SurfaceHolder.Callback {
    private final InkPlaygroundView runtimeView;
    private final boolean preview;

    public InkRenderSurfaceView(InkPlaygroundView runtimeView, boolean preview) {
        super(runtimeView.getContext());
        this.runtimeView = runtimeView;
        this.preview = preview;
        getHolder().addCallback(this);
        if (preview) {
            getHolder().setFormat(PixelFormat.TRANSLUCENT);
            setZOrderMediaOverlay(true);
        } else {
            getHolder().setFormat(PixelFormat.OPAQUE);
        }
    }

    @Override public void surfaceCreated(SurfaceHolder holder) {
        // Dimensions are authoritative in surfaceChanged.
    }

    @Override public void surfaceChanged(SurfaceHolder holder, int format, int width, int height) {
        runtimeView.attachRenderSurface(holder.getSurface(), preview, width, height);
    }

    @Override public void surfaceDestroyed(SurfaceHolder holder) {
        runtimeView.detachRenderSurface(preview);
    }
}
