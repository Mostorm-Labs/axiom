package dev.mostorm.axiom.inkplayground;

import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.view.View;
import java.nio.ByteBuffer;

/** Transparent presentation surface; it owns no input, brush, scene or viewport semantics. */
public final class InkPreviewOverlayView extends View {
    private final InkPlaygroundView source;
    private Bitmap bitmap;
    public InkPreviewOverlayView(InkPlaygroundView source) {
        super(source.getContext());
        this.source = source;
        setWillNotDraw(false);
        setClickable(false);
        setFocusable(false);
        setBackgroundColor(android.graphics.Color.TRANSPARENT);
    }
    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        final int width = getWidth(), height = getHeight();
        final byte[] pixels = source.previewPixels(width, height);
        if (pixels == null || pixels.length < width * height * 4) return;
        if (bitmap == null || bitmap.getWidth() != width || bitmap.getHeight() != height)
            bitmap = Bitmap.createBitmap(width, height, Bitmap.Config.ARGB_8888);
        bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(pixels));
        canvas.drawBitmap(bitmap, 0f, 0f, null);
        // The native preview provider is dirty-gated. Continuous invalidation
        // would repeatedly readback/present the overlay and hide lifecycle
        // bugs; input/resize/rebind explicitly invalidate this view.
    }
}
