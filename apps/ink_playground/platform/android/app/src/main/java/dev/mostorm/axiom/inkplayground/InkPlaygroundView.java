package dev.mostorm.axiom.inkplayground;

import android.content.Context;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
import android.graphics.Bitmap;
import java.nio.ByteBuffer;
import android.os.Build;
import android.view.MotionEvent;
import android.view.View;

import java.io.File;
import java.io.FileOutputStream;
import java.nio.charset.StandardCharsets;
import java.security.MessageDigest;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.Locale;
import java.util.concurrent.ConcurrentLinkedQueue;
import java.util.concurrent.Executors;
import java.util.concurrent.ScheduledExecutorService;
import java.util.concurrent.ScheduledFuture;
import java.util.concurrent.TimeUnit;

public final class InkPlaygroundView extends View {
    private static final class Point { final float x, y, pressure, size, opacity; final int representation, family; Point(float x, float y, float p, float s, float o, int r, int f) { this.x=x; this.y=y; this.pressure=p; this.size=s; this.opacity=o; this.representation=r; this.family=f; } }
    private static final class Stroke { final ArrayList<Point> points = new ArrayList<>(); }
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
        final ArrayList<Stroke> strokes;
        final int width, height, batchCount, sdk;
        final long traceSequence;
        final float lastPressure, viewportScale, viewportTranslationX, viewportTranslationY;
        final String device, model;
        final String brushEvidence;
        EvidenceSnapshot(ArrayList<Stroke> strokes,
                         int width, int height, int batchCount, int sdk, long traceSequence,
                         float lastPressure, float viewportScale,
                         float viewportTranslationX, float viewportTranslationY,
                         String device, String model, String brushEvidence) {
            this.strokes = strokes; this.width = width; this.height = height;
            this.batchCount = batchCount; this.sdk = sdk; this.lastPressure = lastPressure;
            this.traceSequence = traceSequence;
            this.viewportScale = viewportScale; this.viewportTranslationX = viewportTranslationX;
            this.viewportTranslationY = viewportTranslationY;
            this.device = device; this.model = model; this.brushEvidence = brushEvidence;
        }
    }
    static { System.loadLibrary("axiom_ink_playground_android"); }

    private final Paint ink = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint hud = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<Stroke> strokes = new ArrayList<>();
    private final HashMap<Integer, Stroke> activeStrokes = new HashMap<>();
    private final HashMap<Integer, Long> pointerStrokeIds = new HashMap<>();
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
    // The committed Skia frame is immutable while a stroke is in flight.  A
    // cached bitmap keeps JNI/readback/upload work off the input hot path.
    private Bitmap committedBitmap;
    private boolean committedBitmapValid;

    public InkPlaygroundView(Context context) {
        super(context);
        setBackgroundColor(Color.WHITE);
        ink.setColor(Color.rgb(26, 91, 255)); ink.setStyle(Paint.Style.STROKE); ink.setStrokeWidth(6f); ink.setStrokeCap(Paint.Cap.ROUND); ink.setStrokeJoin(Paint.Join.ROUND);
        hud.setColor(Color.rgb(37, 48, 74)); hud.setTextSize(28f);
        evidenceDir = new File(context.getFilesDir(), "g4-5-android");
        evidenceDir.mkdirs();
        setFocusable(true);
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) {
        if (committedBitmap != null) {
            committedBitmap.recycle();
            committedBitmap = null;
        }
        committedBitmapValid = false;
        if (handle != 0) nativeResize(handle, w, h);
    }

    private void refreshCommittedBitmapIfNeeded() {
        // A viewport gesture needs a fresh transformed frame even though its
        // pointers are active; ordinary ink input stays on the cheap overlay.
        if ((!activeStrokes.isEmpty() && !viewportMode) || handle == 0 ||
                getWidth() <= 0 || getHeight() <= 0) return;
        if (committedBitmapValid && committedBitmap != null) return;
        final byte[] rgba = nativeBrushRgba(handle, getWidth(), getHeight());
        if (rgba == null) return;
        if (committedBitmap == null || committedBitmap.getWidth() != getWidth() ||
                committedBitmap.getHeight() != getHeight()) {
            if (committedBitmap != null) committedBitmap.recycle();
            committedBitmap = Bitmap.createBitmap(getWidth(), getHeight(), Bitmap.Config.ARGB_8888);
        }
        committedBitmap.copyPixelsFromBuffer(ByteBuffer.wrap(rgba));
        committedBitmapValid = true;
    }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawColor(Color.WHITE);
        refreshCommittedBitmapIfNeeded();
        if (committedBitmapValid && committedBitmap != null) {
            canvas.drawBitmap(committedBitmap, 0f, 0f, null);
        } else {
            canvas.save();
            canvas.translate(viewportTranslationX, viewportTranslationY);
            canvas.scale(viewportScale, viewportScale);
            for (Stroke stroke : strokes) {
                for (int i = 1; i < stroke.points.size(); ++i) {
                    Point a = stroke.points.get(i - 1), b = stroke.points.get(i);
                    if (a.family >= 2 && a.family <= 5) drawTexturedBrush(canvas, ink, a, b);
                    else drawBrushSegment(canvas, ink, a, b);
                }
            }
            canvas.restore();
        }
        // Native Skia owns committed programmable-DAB pixels.  The Java
        // activity list owns the in-flight stroke, so always overlay it for
        // low-latency preview, including when Skia already has a prior dab.
        if (!viewportMode) {
            canvas.save();
            canvas.translate(viewportTranslationX, viewportTranslationY);
            canvas.scale(viewportScale, viewportScale);
            for (Stroke active : activeStrokes.values()) for (int i = 1; i < active.points.size(); ++i) {
                Point a = active.points.get(i - 1), b = active.points.get(i);
                if (a.family >= 2 && a.family <= 5) drawTexturedBrush(canvas, ink, a, b);
                else drawBrushSegment(canvas, ink, a, b);
            }
            canvas.restore();
        }
        hud.setStyle(Paint.Style.FILL);
        canvas.drawText(String.format(Locale.US, "Axiom Brush Lab  |  strokes %d  points %d  family %s  pressure %.2f", strokes.size(), pointCount(), brushFamilyName(activeBrushFamily), lastPressure), 24f, 42f, hud);
        canvas.drawText(String.format(Locale.US, "pointers %d  viewport %.2fx center %.0f,%.0f  mode %s", activeStrokes.size(), viewportScale, viewportCenterX, viewportCenterY, multiContactPolicyLabel()), 24f, 78f, hud);
    }

    private static boolean hasInkPixels(byte[] rgba) {
        for (int i = 0; i + 3 < rgba.length; i += 4) {
            final int r = rgba[i] & 0xff, g = rgba[i + 1] & 0xff,
                    b = rgba[i + 2] & 0xff, a = rgba[i + 3] & 0xff;
            if (a > 8 && (r < 245 || g < 245 || b < 245)) return true;
        }
        return false;
    }

    private int pointCount() { int count = 0; for (Stroke active : activeStrokes.values()) count += active.points.size(); for (Stroke s : strokes) count += s.points.size(); return count; }

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
        if (action == MotionEvent.ACTION_CANCEL) { if (handle != 0) nativeCancelAll(handle); activeStrokes.clear(); pointerStrokeIds.clear(); pinchBaseline = 0f; scheduleEvidenceSnapshot(); invalidate(); return true; }
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_POINTER_DOWN &&
            action != MotionEvent.ACTION_MOVE && action != MotionEvent.ACTION_UP &&
            action != MotionEvent.ACTION_POINTER_UP) return true;
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int pointerIndex = event.getActionIndex();
            final int pointerId = event.getPointerId(pointerIndex);
            if (handle == 0) {
                handle = nativeCreate(getWidth(), getHeight());
                nativeSetMultiContactPolicy(handle, selectedMultiContactPolicy);
            }
            strokeId++;
            activeBrushFamily = selectedBrushFamily;
            Stroke active = new Stroke(); activeStrokes.put(pointerId, active);
            pointerStrokeIds.put(pointerId, strokeId);
            nativeBegin(handle, pointerId, strokeId);
            nativeBrushBegin(handle, pointerId, activeBrushFamily);
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
        invalidate(); return true;
    }

    private void updateViewportGesture() {
        if (handle == 0) return;
        final boolean claimed = nativeViewportClaimed(handle) != 0;
        if (claimed) {
            final float nextScale = nativeViewportScale(handle);
            final float nextTranslationX = nativeViewportTranslationX(handle);
            final float nextTranslationY = nativeViewportTranslationY(handle);
            if (nextScale != viewportScale || nextTranslationX != viewportTranslationX ||
                    nextTranslationY != viewportTranslationY) committedBitmapValid = false;
            viewportMode = true;
            viewportScale = nextScale;
            viewportCenterX = nativeViewportCenterX(handle);
            viewportCenterY = nativeViewportCenterY(handle);
            viewportTranslationX = nextTranslationX;
            viewportTranslationY = nextTranslationY;
        } else if (viewportMode) {
            viewportMode = false;
            committedBitmapValid = false;
        }
    }

    private void clearViewportProvisionalStrokes() {
        if (!viewportMode) return;
        for (Integer pointerId : new ArrayList<>(activeStrokes.keySet())) {
            if (handle != 0) nativeBrushCancel(handle, pointerId);
            activeStrokes.get(pointerId).points.clear();
        }
    }

    private void emitPointer(MotionEvent event, int pointerIndex, boolean down, boolean up) {
        final int pointerId = event.getPointerId(pointerIndex);
        final Stroke active = activeStrokes.get(pointerId);
        final Long activeStrokeId = pointerStrokeIds.get(pointerId);
        if (active == null || activeStrokeId == null) return;
        final int history = event.getHistorySize();
        final int count = history + 1;
        for (int i = 0; i < count; ++i) {
            final boolean current = i == history;
            final float viewX = current ? event.getX(pointerIndex) : event.getHistoricalX(pointerIndex, i);
            final float viewY = current ? event.getY(pointerIndex) : event.getHistoricalY(pointerIndex, i);
            final float x = (viewX - viewportTranslationX) / viewportScale;
            final float y = (viewY - viewportTranslationY) / viewportScale;
            final float pressure = current ? event.getPressure(pointerIndex) : event.getHistoricalPressure(pointerIndex, i);
            final long timeNs = (current ? event.getEventTime() : event.getHistoricalEventTime(i)) * 1000000L;
            final int tool = event.getToolType(pointerIndex);
            final float major = current ? event.getTouchMajor(pointerIndex) : event.getHistoricalTouchMajor(pointerIndex, i);
            final float minor = current ? event.getTouchMinor(pointerIndex) : event.getHistoricalTouchMinor(pointerIndex, i);
            lastTool = tool == MotionEvent.TOOL_TYPE_STYLUS ? "stylus" : tool == MotionEvent.TOOL_TYPE_ERASER ? "eraser" : "touch";
            lastPressure = pressure;
            final long sampleSequence = ++sequence;
            trace.add(new TraceSample(activeStrokeId, pointerId, sampleSequence, x, y, pressure,
                    major, minor, lastTool, timeNs));
            nativeMotion(handle, pointerId, sampleSequence, timeNs, viewX, viewY, pressure,
                    down && i == 0 ? 1 : 0, up && i == history ? 1 : 0, tool);
            final boolean claimedByViewport = nativeViewportClaimed(handle) != 0;
            if (!claimedByViewport) {
                nativeBrushSample(handle, pointerId, sampleSequence, x, y, pressure);
                float brushSize = nativeBrushSize(handle, pointerId);
                float brushOpacity = nativeBrushOpacity(handle, pointerId);
                int brushRepresentation = nativeBrushRepresentation(handle, pointerId);
                active.points.add(new Point(x, y, pressure, brushSize, brushOpacity,
                        brushRepresentation, activeBrushFamily));
            }
        }
        batchCount = count;
        if (up) {
            final boolean viewportClaimedAtEnd = nativeViewportClaimed(handle) != 0 || viewportMode;
            final boolean committed = !viewportClaimedAtEnd &&
                    nativeCommit(handle, pointerId, activeStrokeId) != 0;
            final boolean brushFinished = !viewportClaimedAtEnd &&
                    nativeBrushFinish(handle, pointerId) != 0;
            if (viewportClaimedAtEnd) {
                nativeBrushCancel(handle, pointerId);
                nativeReleasePointer(handle, pointerId);
            }
            if (committed && brushFinished) appendBrushEvidence(pointerId);
            if (committed) strokes.add(active);
            if (committed) committedBitmapValid = false;
            activeStrokes.remove(pointerId);
            pointerStrokeIds.remove(pointerId);
            scheduleEvidenceSnapshot();
        }
    }

    private static void drawBrushSegment(Canvas canvas, Paint paint, Point a, Point b) {
        paint.setAlpha(Math.max(20, Math.min(255, (int)(255f * a.opacity))));
        if (a.family == 7 || a.representation == 3) { paint.setColor(Color.rgb(255, 24, 64)); paint.setStrokeWidth(Math.max(3f, a.size * 0.65f)); }
        else if (a.family == 6) { paint.setColor(Color.rgb(255, 216, 32)); paint.setStrokeWidth(Math.max(10f, a.size * 0.9f)); }
        else if (a.family == 3) { paint.setColor(Color.rgb(120, 96, 72)); paint.setStrokeWidth(Math.max(4f, a.size * 0.7f)); }
        else if (a.family == 4) { paint.setColor(Color.rgb(32, 112, 240)); paint.setStrokeWidth(Math.max(8f, a.size * 1.25f)); }
        else if (a.family == 5) { paint.setColor(Color.rgb(38, 170, 210)); paint.setStrokeWidth(Math.max(9f, a.size * 1.1f)); }
        else if (a.family == 2) { paint.setColor(Color.rgb(65, 65, 65)); paint.setStrokeWidth(Math.max(3f, a.size * 0.5f)); }
        else { paint.setColor(Color.rgb(26, 91, 255)); paint.setStrokeWidth(Math.max(3f, a.size * 0.65f)); }
        paint.setStyle(Paint.Style.STROKE);
        canvas.drawLine(a.x, a.y, b.x, b.y, paint);
        paint.setAlpha(255);
    }

    private static void drawTexturedBrush(Canvas canvas, Paint paint, Point a, Point b) {
        final float dx = b.x - a.x, dy = b.y - a.y;
        final float length = (float)Math.hypot(dx, dy);
        final float nx = length == 0f ? 0f : -dy / length;
        final float ny = length == 0f ? 1f : dx / length;
        final float pressureWidth = Math.max(4f, a.size * (a.family == 2 ? 0.42f :
                a.family == 3 ? 0.95f : a.family == 4 ? 1.35f : 1.05f) *
                (0.72f + 0.55f * a.pressure));
        if (a.family == 2) paint.setColor(Color.rgb(55, 55, 55));
        else if (a.family == 3) paint.setColor(Color.rgb(135, 94, 61));
        else if (a.family == 4) paint.setColor(Color.rgb(35, 111, 236));
        else paint.setColor(Color.rgb(38, 170, 210));
        paint.setStyle(Paint.Style.STROKE); paint.setStrokeCap(Paint.Cap.ROUND);
        paint.setStrokeWidth(pressureWidth);
        final int baseAlpha = a.family == 2 ? 190 : a.family == 3 ? 105 : a.family == 4 ? 175 : 105;
        paint.setAlpha(Math.max(25, Math.min(220, (int)(baseAlpha * a.opacity))));
        canvas.drawLine(a.x, a.y, b.x, b.y, paint);
        final int strands = a.family == 2 ? 1 : a.family == 3 ? 4 : a.family == 4 ? 2 : 5;
        for (int i = -strands; i <= strands; ++i) {
            final float jitter = i * pressureWidth * (a.family == 3 ? 0.28f : 0.19f);
            paint.setStrokeWidth(Math.max(1.5f, pressureWidth * (0.08f + 0.02f * (i + 3))));
            paint.setAlpha(Math.max(10, (a.family == 2 ? 70 : 58) - Math.abs(i) * 10));
            canvas.drawLine(a.x + nx * jitter, a.y + ny * jitter, b.x + nx * jitter, b.y + ny * jitter, paint);
        }
        paint.setAlpha(255);
    }

    private EvidenceSnapshot snapshotEvidence() {
        ArrayList<Stroke> strokesCopy = new ArrayList<>(strokes);
        return new EvidenceSnapshot(strokesCopy, getWidth(), getHeight(), batchCount,
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
            Canvas capture = new Canvas(bitmap); drawSnapshot(capture, snapshot);
            File captureFile = new File(evidenceDir, "ink-playground.png");
            try (FileOutputStream output = new FileOutputStream(captureFile)) { bitmap.compress(Bitmap.CompressFormat.PNG, 100, output); }
            String sha = sha256(captureFile);
            String record = String.format(Locale.US, "{\"platform\":\"android\",\"device\":\"%s\",\"model\":\"%s\",\"sdk\":%d,\"batch\":%d,\"pressure\":%.5f,\"viewport_scale\":%.5f,\"capture\":\"%s\",\"capture_sha256\":\"%s\"}\n", snapshot.device, snapshot.model, snapshot.sdk, snapshot.batchCount, snapshot.lastPressure, snapshot.viewportScale, captureFile.getAbsolutePath(), sha);
            write(new File(evidenceDir, "functional-smoke.json"), record.getBytes(StandardCharsets.UTF_8));
        } catch (Exception ignored) { }
    }
    private static void drawSnapshot(Canvas canvas, EvidenceSnapshot snapshot) {
        canvas.drawColor(Color.WHITE);
        canvas.save();
        canvas.translate(snapshot.viewportTranslationX, snapshot.viewportTranslationY);
        canvas.scale(snapshot.viewportScale, snapshot.viewportScale);
        Paint capturePaint = new Paint(Paint.ANTI_ALIAS_FLAG);
        capturePaint.setColor(Color.rgb(26, 91, 255));
        capturePaint.setStyle(Paint.Style.STROKE);
        capturePaint.setStrokeWidth(6f);
        capturePaint.setStrokeCap(Paint.Cap.ROUND);
        capturePaint.setStrokeJoin(Paint.Join.ROUND);
        for (Stroke stroke : snapshot.strokes) {
            for (int i = 1; i < stroke.points.size(); ++i) {
                Point a = stroke.points.get(i - 1), b = stroke.points.get(i);
                if (a.family >= 2 && a.family <= 5) drawTexturedBrush(canvas, capturePaint, a, b);
                else drawBrushSegment(canvas, capturePaint, a, b);
            }
        }
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
    private static String brushFamilyName(int family) {
        switch (family) {
            case 1: return "pen"; case 2: return "pencil"; case 3: return "chalk";
            case 4: return "marker"; case 5: return "water_color_lite";
            case 6: return "highlighter"; case 7: return "laser"; default: return "unknown";
        }
    }
    public void selectBrushFamily(int family) {
        if (family >= 1 && family <= 7) { selectedBrushFamily = family; activeBrushFamily = family; invalidate(); }
    }
    public int selectedBrushFamily() { return selectedBrushFamily; }

    private static void write(File file, byte[] bytes) throws Exception { try (FileOutputStream out = new FileOutputStream(file)) { out.write(bytes); } }
    private static String sha256(File file) throws Exception { MessageDigest digest = MessageDigest.getInstance("SHA-256"); byte[] data = java.nio.file.Files.readAllBytes(file.toPath()); byte[] hash = digest.digest(data); StringBuilder value = new StringBuilder(); for (byte b : hash) value.append(String.format("%02x", b)); return value.toString(); }
    public void close() { if (handle != 0) { nativeDestroy(handle); handle = 0; } evidenceExecutor.shutdown(); }

    private static native long nativeCreate(int width, int height);
    private static native void nativeDestroy(long handle);
    private static native int nativeBegin(long handle, int pointerId, long strokeId);
    private static native int nativeMotion(long handle, int pointerId, long sequence, long timeNs, float x, float y, float pressure, int down, int up, int tool);
    private static native int nativeCommit(long handle, int pointerId, long strokeId);
    private static native int nativeReleasePointer(long handle, int pointerId);
    private static native int nativeResize(long handle, int width, int height);
    private static native int nativeSurfaceLost(long handle);
    private static native int nativeCancelAll(long handle);
    private static native int nativeBrushBegin(long handle, int pointerId, int family);
    private static native int nativeBrushSample(long handle, int pointerId, long sequence, float x, float y, float pressure);
    private static native int nativeBrushFinish(long handle, int pointerId);
    private static native int nativeBrushCancel(long handle, int pointerId);
    private static native long nativeBrushDigest(long handle);
    private static native long nativeBrushPrimitiveCount(long handle);
    private static native int nativeBrushFamily(long handle);
    private static native int nativeBrushCanonicalMutation(long handle);
    private static native float nativeBrushSize(long handle, int pointerId);
    private static native float nativeBrushOpacity(long handle, int pointerId);
    private static native int nativeBrushRepresentation(long handle, int pointerId);
    private static native byte[] nativeBrushRgba(long handle, int width, int height);
    private static native int nativeSetMultiContactPolicy(long handle, int policy);
    private static native int nativeMultiContactPolicy(long handle);
    private static native int nativeViewportClaimed(long handle);
    private static native float nativeViewportScale(long handle);
    private static native float nativeViewportCenterX(long handle);
    private static native float nativeViewportCenterY(long handle);
    private static native float nativeViewportTranslationX(long handle);
    private static native float nativeViewportTranslationY(long handle);
}
