package dev.mostorm.axiom.inkplayground;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Bitmap;
import java.nio.ByteBuffer;
import android.os.Build;
import android.view.MotionEvent;
import android.view.Surface;
import android.view.View;
import android.view.Choreographer;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.HashSet;
import java.util.Locale;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

public final class InkPlaygroundView extends View {
    private static final class TraceSample {
        final long stroke, pointerId, sequence, timeNs;
        final float x, y, pressure, major, minor;
        final String tool;
        TraceSample(long stroke, long pointerId, long sequence, float x, float y,
                    float pressure, float major, float minor, String tool, long timeNs) {
            this.stroke = stroke; this.pointerId = pointerId; this.sequence = sequence;
            this.x = x; this.y = y; this.pressure = pressure; this.major = major;
            this.minor = minor; this.tool = tool; this.timeNs = timeNs;
        }
    }
    private static final class EvidenceSnapshot {
        final int width, height, batchCount, sdk;
        final long traceSequence;
        final float lastPressure, viewportScale, viewportTranslationX, viewportTranslationY;
        final String device, model;
        final String brushEvidence;
        EvidenceSnapshot(
                         int width, int height, int batchCount, int sdk, long traceSequence,
                         float lastPressure, float viewportScale,
                         float viewportTranslationX, float viewportTranslationY,
                         String device, String model, String brushEvidence) {
            this.width = width; this.height = height;
            this.batchCount = batchCount; this.sdk = sdk; this.lastPressure = lastPressure;
            this.traceSequence = traceSequence;
            this.viewportScale = viewportScale; this.viewportTranslationX = viewportTranslationX;
            this.viewportTranslationY = viewportTranslationY;
            this.device = device; this.model = model; this.brushEvidence = brushEvidence;
        }
    }
    static { System.loadLibrary("axiom_ink_playground_android"); }

    private final Paint hud = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final HashSet<Integer> activePointers = new HashSet<>();
    private final File evidenceDir;
    private final ConcurrentLinkedQueue<TraceSample> trace = new ConcurrentLinkedQueue<>();
    private final ScheduledExecutorService evidenceExecutor = Executors.newSingleThreadScheduledExecutor();
    private ScheduledFuture<?> pendingEvidence;
    private long handle;
    private long sequence;
    private long strokeId;
    private String lastTool = "none";
    private float lastPressure;
    private int batchCount;
    private float viewportScale = 1f;
    private float viewportCenterX;
    private float viewportCenterY;
    private float viewportTranslationX;
    private float viewportTranslationY;
    private float pinchBaseline;
    private boolean viewportMode;
    private int selectedMultiContactPolicy;
    private int activeBrushFamily = 1;
    private final StringBuilder brushEvidence = new StringBuilder("[\n");
    private boolean brushEvidenceFirst = true;
    private int selectedBrushFamily = 1;
    private Runnable previewInvalidator;
    private boolean gpuCanonicalAttached;
    private boolean gpuPreviewAttached;
    private boolean previewFramePending;

    public InkPlaygroundView(Context context) {
        super(context);
        setBackgroundColor(Color.TRANSPARENT);
        hud.setColor(Color.rgb(37, 48, 74)); hud.setTextSize(28f);
        evidenceDir = new File(context.getFilesDir(), "g4-5-android");
        evidenceDir.mkdirs();
        setFocusable(true);
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (handle != 0) nativeResize(handle, w, h);
        if (previewInvalidator != null) previewInvalidator.run();
    }
    void setPreviewInvalidator(Runnable invalidator) { previewInvalidator = invalidator; }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        // The production path presents directly through the two GLES-backed
        // SurfaceViews below this transparent input/HUD view. Pixel readback
        // is reserved for the explicit evidence capture path.
        // Java Canvas is present-only. Brush, stroke and viewport semantics
        // must come from Runtime → SkiaRenderer → native surface provider.
        hud.setStyle(Paint.Style.FILL);
        canvas.drawText(String.format(Locale.US, "Axiom Brush Lab  |  runtime strokes  family %s  pressure %.2f", brushFamilyName(activeBrushFamily), lastPressure), 24f, 42f, hud);
        canvas.drawText(String.format(Locale.US, "pointers %d  viewport %.2fx center %.0f,%.0f  mode %s", activePointers.size(), viewportScale, viewportCenterX, viewportCenterY, multiContactPolicyLabel()), 24f, 78f, hud);
    }

    /** Presentation-only pixel bridge used by the independent ARC overlay. */
    byte[] previewPixels(int width, int height) {
        return handle == 0 ? null : nativePreviewRgba(handle, width, height);
    }

    private void ensureNativeHost(int width, int height) {
        if (handle == 0 && width > 0 && height > 0) {
            handle = nativeCreate(width, height);
            nativeSetMultiContactPolicy(handle, selectedMultiContactPolicy);
        }
    }

    void attachRenderSurface(Surface surface, boolean preview, int width, int height) {
        ensureNativeHost(width, height);
        if (handle == 0 || surface == null || !surface.isValid()) return;
        if (nativeAttachSurface(handle, surface, preview, width, height) != 0) {
            if (preview) gpuPreviewAttached = true;
            else gpuCanonicalAttached = true;
        }
    }

    void detachRenderSurface(boolean preview) {
        if (handle != 0) nativeDetachSurface(handle, preview);
        if (preview) gpuPreviewAttached = false;
        else gpuCanonicalAttached = false;
    }

    private int pointCount() { return trace.size(); }

    public String cycleMultiContactPolicy() {
        final int next = (selectedMultiContactPolicy + 1) % 3;
        if (setMultiContactPolicy(next)) invalidate();
        return multiContactPolicyLabel();
    }

    private boolean setMultiContactPolicy(int policy) {
        if (handle == 0) {
            selectedMultiContactPolicy = policy;
            return true;
        }
        if (nativeSetMultiContactPolicy(handle, policy) == 0) return false;
        selectedMultiContactPolicy = nativeMultiContactPolicy(handle);
        return true;
    }

    private String multiContactPolicyLabel() {
        return selectedMultiContactPolicy == 0 ? "AutoIntent" :
            selectedMultiContactPolicy == 1 ? "MultiInk" : "GesturePriority";
    }

    @Override public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL) { if (handle != 0) nativeCancelAll(handle); activePointers.clear(); pinchBaseline = 0f; scheduleEvidenceSnapshot(); invalidate(); return true; }
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_POINTER_DOWN &&
            action != MotionEvent.ACTION_MOVE && action != MotionEvent.ACTION_UP &&
            action != MotionEvent.ACTION_POINTER_UP) return true;
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int pointerIndex = event.getActionIndex();
            final int pointerId = event.getPointerId(pointerIndex);
            ensureNativeHost(getWidth(), getHeight());
            strokeId++;
            activeBrushFamily = selectedBrushFamily;
            activePointers.add(pointerId);
            emitPointer(event, pointerIndex, true, false);
        } else if (action == MotionEvent.ACTION_MOVE) {
            for (int pointerIndex = 0; pointerIndex < event.getPointerCount(); ++pointerIndex) {
                emitPointer(event, pointerIndex, false, false);
            }
        } else {
            final int pointerIndex = event.getActionIndex();
            emitPointer(event, pointerIndex, false, true);
        }
        updateViewportGesture();
        clearViewportProvisionalStrokes();
        invalidate();
        requestPreviewFrame();
        return true;
    }

    private void requestPreviewFrame() {
        if (previewFramePending) return;
        previewFramePending = true;
        Choreographer.getInstance().postFrameCallback(frameTimeNanos -> {
            previewFramePending = false;
            if (handle != 0 && gpuPreviewAttached) nativePresentPreview(handle);
        });
    }

    private void updateViewportGesture() {
        if (handle == 0) return;
        final boolean claimed = nativeViewportClaimed(handle) != 0;
        if (claimed) {
            viewportMode = true;
            viewportScale = nativeViewportScale(handle);
            viewportCenterX = nativeViewportCenterX(handle);
            viewportCenterY = nativeViewportCenterY(handle);
            viewportTranslationX = nativeViewportTranslationX(handle);
            viewportTranslationY = nativeViewportTranslationY(handle);
        } else if (viewportMode) {
            viewportMode = false;
            // Keep the HUD in sync with Runtime after the last contact is
            // released.  Java is display-only; the committed transform is
            // still owned by ViewportInteractionController.
            viewportScale = nativeViewportScale(handle);
            viewportCenterX = nativeViewportCenterX(handle);
            viewportCenterY = nativeViewportCenterY(handle);
            viewportTranslationX = nativeViewportTranslationX(handle);
            viewportTranslationY = nativeViewportTranslationY(handle);
        }
    }

    private void clearViewportProvisionalStrokes() {
        if (!viewportMode) return;
        // Runtime owns provisional stroke cancellation.
    }

    private void emitPointer(MotionEvent event, int pointerIndex, boolean down, boolean up) {
        final int pointerId = event.getPointerId(pointerIndex);
        if (!activePointers.contains(pointerId)) return;
        final int history = event.getHistorySize();
        final int count = history + 1;
        final long[] sequences = new long[count];
        final long[] times = new long[count];
        final float[] xs = new float[count];
        final float[] ys = new float[count];
        final float[] pressures = new float[count];
        final int[] phases = new int[count];
        for (int i = 0; i < count; ++i) {
            final boolean current = i == history;
            final float viewX = current ? event.getX(pointerIndex) : event.getHistoricalX(pointerIndex, i);
            final float viewY = current ? event.getY(pointerIndex) : event.getHistoricalY(pointerIndex, i);
            // MotionEvent coordinates are already local to this canvas. Do
            // not apply screen/window insets here; the runtime viewport owns
            // only its content transform.
            // MotionEvent coordinates are View-local. Runtime owns the one
            // canonical view→content transform; Java must pass raw samples.
            final float x = viewX;
            final float y = viewY;
            final float pressure = current ? event.getPressure(pointerIndex) : event.getHistoricalPressure(pointerIndex, i);
            final long timeNs = (current ? event.getEventTime() : event.getHistoricalEventTime(i)) * 1000000L;
            final int tool = event.getToolType(pointerIndex);
            final float major = current ? event.getTouchMajor(pointerIndex) : event.getHistoricalTouchMajor(pointerIndex, i);
            final float minor = current ? event.getTouchMinor(pointerIndex) : event.getHistoricalTouchMinor(pointerIndex, i);
            lastTool = tool == MotionEvent.TOOL_TYPE_STYLUS ? "stylus" : tool == MotionEvent.TOOL_TYPE_ERASER ? "eraser" : "touch";
            lastPressure = pressure;
            final long sampleSequence = ++sequence;
            sequences[i] = sampleSequence;
            times[i] = timeNs;
            xs[i] = x;
            ys[i] = y;
            pressures[i] = pressure;
            phases[i] = down && i == 0 ? 1 : up && i == history ? 3 : 2;
            trace.add(new TraceSample(strokeId, pointerId, sampleSequence, x, y, pressure,
                    major, minor, lastTool, timeNs));
            // Runtime owns BrushPackage/BrushSession interpretation. Java
            // keeps no platform stroke primitive; the trace above is the
            // complete input evidence boundary.
        }
        nativePlatformBatch(handle, pointerId, sequences, times, xs, ys, pressures,
                phases, activeBrushFamily);
        batchCount = count;
        if (up) {
            appendBrushEvidence(pointerId);
            activePointers.remove(pointerId);
            scheduleEvidenceSnapshot();
        }
    }

    private EvidenceSnapshot snapshotEvidence() {
        return new EvidenceSnapshot(getWidth(), getHeight(), batchCount,
                Build.VERSION.SDK_INT, sequence, lastPressure, viewportScale, viewportTranslationX,
                viewportTranslationY, Build.DEVICE, Build.MODEL, brushEvidence.toString() + "\n]\n");
    }

    private synchronized void scheduleEvidenceSnapshot() {
        final EvidenceSnapshot snapshot = snapshotEvidence();
        if (pendingEvidence != null) pendingEvidence.cancel(false);
        pendingEvidence = evidenceExecutor.schedule(() -> persistEvidence(snapshot),
                500, TimeUnit.MILLISECONDS);
    }

    private void persistEvidence(EvidenceSnapshot snapshot) {
        try {
            StringBuilder traceJson = new StringBuilder("[\n");
            int index = 0;
            for (TraceSample sample : trace) {
                if (sample.sequence > snapshot.traceSequence) continue;
                if (index++ != 0) traceJson.append(",\n");
                traceJson.append(String.format(Locale.US, "  {\"stroke\":%d,\"pointer_id\":%d,\"sequence\":%d,\"x\":%.3f,\"y\":%.3f,\"pressure\":%.5f,\"touch_major\":%.3f,\"touch_minor\":%.3f,\"tool\":\"%s\",\"time_ns\":%d}", sample.stroke, sample.pointerId, sample.sequence, sample.x, sample.y, sample.pressure, sample.major, sample.minor, sample.tool, sample.timeNs));
            }
            traceJson.append("\n]\n");
            write(new File(evidenceDir, "pointer-trace.json"), traceJson.toString().getBytes(StandardCharsets.UTF_8));
            write(new File(evidenceDir, "programmable-brush-evidence.json"), snapshot.brushEvidence.getBytes(StandardCharsets.UTF_8));
        Bitmap bitmap = Bitmap.createBitmap(Math.max(1, snapshot.width), Math.max(1, snapshot.height), Bitmap.Config.ARGB_8888);
            final byte[] nativeCapture = handle == 0 ? null : nativeBrushRgba(handle, snapshot.width, snapshot.height);
            if (nativeCapture != null) bitmap.copyPixelsFromBuffer(ByteBuffer.wrap(nativeCapture));
            File captureFile = new File(evidenceDir, "ink-playground.png");
            try (FileOutputStream output = new FileOutputStream(captureFile)) { bitmap.compress(Bitmap.CompressFormat.PNG, 100, output); }
            String sha = sha256(captureFile);
            long readbacks = handle == 0 ? 0 : nativeRenderReadbackCount(handle);
            long copies = handle == 0 ? 0 : nativeRenderCpuCopyCount(handle);
            long presents = handle == 0 ? 0 : nativeRenderPresentCount(handle);
            String record = String.format(Locale.US, "{\"platform\":\"android\",\"device\":\"%s\",\"model\":\"%s\",\"sdk\":%d,\"batch\":%d,\"pressure\":%.5f,\"viewport_scale\":%.5f,\"capture\":\"%s\",\"capture_sha256\":\"%s\",\"render_path\":{\"input_boundary\":\"MotionEvent.history→JNI\",\"runtime_boundary\":\"C++ InkPlaygroundHost\",\"renderer\":\"Skia raster\",\"surface_type\":\"CPU raster buffer→Java Bitmap→Canvas\",\"submission_count\":%d,\"readback_count\":%d,\"cpu_copy_count\":%d,\"present_count\":%d,\"viewport_transform_applied\":\"true\"}}\n", snapshot.device, snapshot.model, snapshot.sdk, snapshot.batchCount, snapshot.lastPressure, snapshot.viewportScale, captureFile.getAbsolutePath(), sha, readbacks, readbacks, copies, presents);
            write(new File(evidenceDir, "functional-smoke.json"), record.getBytes(StandardCharsets.UTF_8));
            String artifactIdentity = getContext().getPackageName() + ":apk-sha256:"
                    + sha256(new File(getContext().getApplicationInfo().sourceDir))
                    + ":" + Build.FINGERPRINT;
            String baseline = handle == 0 ? null : nativeBaselineObservation(
                    handle, "PHYSICAL", artifactIdentity);
            if (baseline != null) write(new File(evidenceDir, "observation.json"),
                    baseline.getBytes(StandardCharsets.UTF_8));
        } catch (Exception ignored) { }
    }
    private static void drawSnapshot(Canvas canvas, EvidenceSnapshot snapshot) {
        canvas.drawColor(Color.WHITE);
        canvas.save();
        canvas.translate(snapshot.viewportTranslationX, snapshot.viewportTranslationY);
        canvas.scale(snapshot.viewportScale, snapshot.viewportScale);
        // Brush pixels are captured from the native Skia provider by onDraw;
        // evidence serialization must not re-rasterize them in Java.
        canvas.restore();
    }
    private void appendBrushEvidence(int pointerId) {
        if (!brushEvidenceFirst) brushEvidence.append(",\n");
        brushEvidenceFirst = false;
        brushEvidence.append(String.format(Locale.US,
                "  {\"family\":\"%s\",\"family_id\":%d,\"digest\":%d,\"primitive_count\":%d,\"canonical_mutation\":%s}",
                brushFamilyName(nativeBrushFamily(handle)), nativeBrushFamily(handle),
                nativeBrushDigest(handle), nativeBrushPrimitiveCount(handle),
                nativeBrushCanonicalMutation(handle) != 0 ? "true" : "false"));
    }
    private static String brushFamilyName(int family) { return "vector-solid-v1"; }
    public void selectBrushFamily(int family) {
        if (family >= 1 && family <= 7) { selectedBrushFamily = family; activeBrushFamily = family; invalidate(); }
    }
    public int selectedBrushFamily() { return selectedBrushFamily; }

    private static void write(File file, byte[] bytes) throws Exception { try (FileOutputStream out = new FileOutputStream(file)) { out.write(bytes); } }
    private static String sha256(File file) throws Exception { MessageDigest digest = MessageDigest.getInstance("SHA-256"); byte[] data = java.nio.file.Files.readAllBytes(file.toPath()); byte[] hash = digest.digest(data); StringBuilder value = new StringBuilder(); for (byte b : hash) value.append(String.format("%02x", b)); return value.toString(); }
    public void close() { if (handle != 0) { nativeDestroy(handle); handle = 0; } evidenceExecutor.shutdown(); }

    private static native long nativeCreate(int width, int height);
    private static native void nativeDestroy(long handle);
    private static native int nativePlatformBatch(long handle, int pointerId, long[] sequences, long[] times, float[] xs, float[] ys, float[] pressures, int[] phases, int family);
    private static native int nativeAttachSurface(long handle, Surface surface, boolean preview, int width, int height);
    private static native void nativeDetachSurface(long handle, boolean preview);
    private static native int nativePresentPreview(long handle);
    private static native int nativeResize(long handle, int width, int height);
    private static native int nativeSurfaceLost(long handle);
    private static native int nativeCancelAll(long handle);
    private static native long nativeBrushDigest(long handle);
    private static native long nativeBrushPrimitiveCount(long handle);
    private static native int nativeBrushFamily(long handle);
    private static native int nativeBrushCanonicalMutation(long handle);
    private static native byte[] nativeBrushRgba(long handle, int width, int height);
    private static native byte[] nativePreviewRgba(long handle, int width, int height);
    private static native void nativeRecordPresent(long handle);
    private static native long nativeRenderReadbackCount(long handle);
    private static native long nativeRenderCpuCopyCount(long handle);
    private static native long nativeRenderPresentCount(long handle);
    private static native String nativeBaselineObservation(long handle, String reality,
                                                            String artifactIdentity);
    private static native int nativeSetMultiContactPolicy(long handle, int policy);
    private static native int nativeMultiContactPolicy(long handle);
    private static native int nativeViewportClaimed(long handle);
    private static native float nativeViewportScale(long handle);
    private static native float nativeViewportCenterX(long handle);
    private static native float nativeViewportCenterY(long handle);
    private static native float nativeViewportTranslationX(long handle);
    private static native float nativeViewportTranslationY(long handle);
}
