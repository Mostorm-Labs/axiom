package dev.mostorm.axiom.inkplayground;

import android.content.Context;
import android.graphics.Bitmap;
import android.graphics.Canvas;
import android.graphics.Color;
import android.graphics.Paint;
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

public final class InkPlaygroundView extends View {
    private static final class Point { final float x, y, pressure, size, opacity; final int representation, family; Point(float x, float y, float p, float s, float o, int r, int f) { this.x=x; this.y=y; this.pressure=p; this.size=s; this.opacity=o; this.representation=r; this.family=f; } }
    private static final class Stroke { final ArrayList<Point> points = new ArrayList<>(); }
    static { System.loadLibrary("axiom_ink_playground_android"); }

    private final Paint ink = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final Paint hud = new Paint(Paint.ANTI_ALIAS_FLAG);
    private final ArrayList<Stroke> strokes = new ArrayList<>();
    private final HashMap<Integer, Stroke> activeStrokes = new HashMap<>();
    private final HashMap<Integer, Long> pointerStrokeIds = new HashMap<>();
    private final File evidenceDir;
    private final StringBuilder trace = new StringBuilder("[\n");
    private long handle;
    private long sequence;
    private long strokeId;
    private boolean traceFirst = true;
    private String lastTool = "none";
    private float lastPressure;
    private int batchCount;
    private float viewportScale = 1f;
    private float viewportCenterX;
    private float viewportCenterY;
    private float pinchBaseline;
    private int activeBrushFamily = 1;
    private final StringBuilder brushEvidence = new StringBuilder("[\n");
    private boolean brushEvidenceFirst = true;
    private int selectedBrushFamily = 1;

    public InkPlaygroundView(Context context) {
        super(context);
        setBackgroundColor(Color.WHITE);
        ink.setColor(Color.rgb(26, 91, 255)); ink.setStyle(Paint.Style.STROKE); ink.setStrokeWidth(6f); ink.setStrokeCap(Paint.Cap.ROUND); ink.setStrokeJoin(Paint.Join.ROUND);
        hud.setColor(Color.rgb(37, 48, 74)); hud.setTextSize(28f);
        evidenceDir = new File(context.getFilesDir(), "g4-5-android");
        evidenceDir.mkdirs();
        setFocusable(true);
    }

    @Override protected void onSizeChanged(int w, int h, int oldw, int oldh) { if (handle != 0) nativeResize(handle, w, h); }

    @Override protected void onDraw(Canvas canvas) {
        super.onDraw(canvas);
        canvas.drawColor(Color.WHITE);
        for (Stroke stroke : strokes) {
            for (int i = 1; i < stroke.points.size(); ++i) {
                Point a = stroke.points.get(i - 1), b = stroke.points.get(i);
                if (a.family == 5) drawTexturedBrush(canvas, a, b);
                else drawBrushSegment(canvas, a, b);
            }
        }
        for (Stroke active : activeStrokes.values()) for (int i = 1; i < active.points.size(); ++i) {
            Point a = active.points.get(i - 1), b = active.points.get(i);
            if (a.family == 5) drawTexturedBrush(canvas, a, b);
            else drawBrushSegment(canvas, a, b);
        }
        hud.setStyle(Paint.Style.FILL);
        canvas.drawText(String.format(Locale.US, "Axiom Brush Lab  |  strokes %d  points %d  family %s  pressure %.2f", strokes.size(), pointCount(), brushFamilyName(activeBrushFamily), lastPressure), 24f, 42f, hud);
        canvas.drawText(String.format(Locale.US, "pointers %d  viewport %.2fx center %.0f,%.0f", activeStrokes.size(), viewportScale, viewportCenterX, viewportCenterY), 24f, 78f, hud);
    }

    private int pointCount() { int count = 0; for (Stroke active : activeStrokes.values()) count += active.points.size(); for (Stroke s : strokes) count += s.points.size(); return count; }

    @Override public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL) { if (handle != 0) nativeCancelAll(handle); activeStrokes.clear(); pointerStrokeIds.clear(); pinchBaseline = 0f; persistEvidence(); invalidate(); return true; }
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_POINTER_DOWN &&
            action != MotionEvent.ACTION_MOVE && action != MotionEvent.ACTION_UP &&
            action != MotionEvent.ACTION_POINTER_UP) return true;
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int pointerIndex = event.getActionIndex();
            final int pointerId = event.getPointerId(pointerIndex);
            if (handle == 0) handle = nativeCreate(getWidth(), getHeight());
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
        updateViewportGesture(event);
        invalidate(); return true;
    }

    private void updateViewportGesture(MotionEvent event) {
        if (event.getPointerCount() < 2) { pinchBaseline = 0f; return; }
        final float dx = event.getX(1) - event.getX(0);
        final float dy = event.getY(1) - event.getY(0);
        final float distance = (float)Math.hypot(dx, dy);
        if (pinchBaseline == 0f) pinchBaseline = distance;
        viewportScale = pinchBaseline == 0f ? 1f : distance / pinchBaseline;
        viewportCenterX = (event.getX(0) + event.getX(1)) * 0.5f;
        viewportCenterY = (event.getY(0) + event.getY(1)) * 0.5f;
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
            final float x = current ? event.getX(pointerIndex) : event.getHistoricalX(pointerIndex, i);
            final float y = current ? event.getY(pointerIndex) : event.getHistoricalY(pointerIndex, i);
            final float pressure = current ? event.getPressure(pointerIndex) : event.getHistoricalPressure(pointerIndex, i);
            final long timeNs = (current ? event.getEventTime() : event.getHistoricalEventTime(i)) * 1000000L;
            final int tool = current ? event.getToolType(pointerIndex) : event.getToolType(pointerIndex);
            final float major = current ? event.getTouchMajor(pointerIndex) : event.getHistoricalTouchMajor(pointerIndex, i);
            final float minor = current ? event.getTouchMinor(pointerIndex) : event.getHistoricalTouchMinor(pointerIndex, i);
            lastTool = tool == MotionEvent.TOOL_TYPE_STYLUS ? "stylus" : tool == MotionEvent.TOOL_TYPE_ERASER ? "eraser" : "touch";
            lastPressure = pressure;
            float brushSize = nativeBrushSize(handle, pointerId);
            float brushOpacity = nativeBrushOpacity(handle, pointerId);
            int brushRepresentation = nativeBrushRepresentation(handle, pointerId);
            active.points.add(new Point(x, y, pressure, brushSize, brushOpacity, brushRepresentation, activeBrushFamily));
            if (!traceFirst) trace.append(",\n"); traceFirst = false;
            trace.append(String.format(Locale.US, "  {\"stroke\":%d,\"pointer_id\":%d,\"sequence\":%d,\"x\":%.3f,\"y\":%.3f,\"pressure\":%.5f,\"touch_major\":%.3f,\"touch_minor\":%.3f,\"tool\":\"%s\",\"time_ns\":%d}", activeStrokeId, pointerId, ++sequence, x, y, pressure, major, minor, lastTool, timeNs));
            nativeMotion(handle, pointerId, sequence, timeNs, x, y, pressure, down && i == 0 ? 1 : 0, up && i == history ? 1 : 0, tool);
            nativeBrushSample(handle, pointerId, sequence, x, y, pressure);
        }
        batchCount = count;
        if (up) {
            nativeCommit(handle, pointerId, activeStrokeId);
            nativeBrushFinish(handle, pointerId);
            appendBrushEvidence(pointerId);
            strokes.add(active); activeStrokes.remove(pointerId); pointerStrokeIds.remove(pointerId); persistEvidence();
        }
    }
    private void drawBrushSegment(Canvas canvas, Point a, Point b) {
        ink.setAlpha(Math.max(20, Math.min(255, (int)(255f * a.opacity))));
        if (a.family == 7 || a.representation == 3) {
            ink.setColor(Color.rgb(255, 24, 64));
            ink.setStrokeWidth(Math.max(3f, a.size * 0.65f));
            ink.setStyle(Paint.Style.STROKE);
        } else if (a.family == 6) {
            ink.setColor(Color.rgb(255, 216, 32));
            ink.setStrokeWidth(Math.max(10f, a.size * 0.9f));
            ink.setStyle(Paint.Style.STROKE);
        } else if (a.family == 3) {
            ink.setColor(Color.rgb(120, 96, 72));
            ink.setStrokeWidth(Math.max(4f, a.size * 0.7f));
            ink.setStyle(Paint.Style.STROKE);
        } else if (a.family == 4) {
            ink.setColor(Color.rgb(32, 112, 240));
            ink.setStrokeWidth(Math.max(8f, a.size * 1.25f));
            ink.setStyle(Paint.Style.STROKE);
        } else if (a.family == 5) {
            ink.setColor(Color.rgb(38, 170, 210));
            ink.setStrokeWidth(Math.max(9f, a.size * 1.1f));
            ink.setStyle(Paint.Style.STROKE);
        } else if (a.family == 2) {
            ink.setColor(Color.rgb(65, 65, 65));
            ink.setStrokeWidth(Math.max(3f, a.size * 0.5f));
            ink.setStyle(Paint.Style.STROKE);
        } else {
            ink.setColor(Color.rgb(26, 91, 255));
            ink.setStrokeWidth(Math.max(3f, a.size * 0.65f));
            ink.setStyle(Paint.Style.STROKE);
        }
        canvas.drawLine(a.x, a.y, b.x, b.y, ink);
        ink.setAlpha(255);
    }
    private void drawTexturedBrush(Canvas canvas, Point a, Point b) {
        final float dx = b.x - a.x;
        final float dy = b.y - a.y;
        final float length = (float)Math.hypot(dx, dy);
        final float nx = length == 0f ? 0f : -dy / length;
        final float ny = length == 0f ? 1f : dx / length;
        final float pressureWidth = Math.max(10f, a.size * (0.72f + 0.55f * a.pressure));
        ink.setColor(Color.rgb(196, 76, 91));
        ink.setStyle(Paint.Style.STROKE);
        ink.setStrokeCap(Paint.Cap.ROUND);
        ink.setStrokeWidth(pressureWidth);
        ink.setAlpha(Math.max(35, Math.min(125, (int)(105f * a.opacity))));
        canvas.drawLine(a.x, a.y, b.x, b.y, ink);
        // Deterministic bristle strands create the dry-brush edge without changing canonical semantics.
        for (int i = -2; i <= 2; ++i) {
            final float jitter = i * pressureWidth * 0.19f;
            ink.setStrokeWidth(Math.max(1.5f, pressureWidth * (0.08f + 0.02f * (i + 3))));
            ink.setAlpha(Math.max(12, 58 - Math.abs(i) * 10));
            canvas.drawLine(a.x + nx * jitter, a.y + ny * jitter,
                    b.x + nx * jitter, b.y + ny * jitter, ink);
        }
        ink.setAlpha(255);
    }

    private void persistEvidence() {
        try {
            String json = trace + "\n]\n";
            write(new File(evidenceDir, "pointer-trace.json"), json.getBytes(StandardCharsets.UTF_8));
            write(new File(evidenceDir, "programmable-brush-evidence.json"),
                    (brushEvidence + "\n]\n").getBytes(StandardCharsets.UTF_8));
            Bitmap bitmap = Bitmap.createBitmap(Math.max(1, getWidth()), Math.max(1, getHeight()), Bitmap.Config.ARGB_8888);
            Canvas capture = new Canvas(bitmap); draw(capture);
            File captureFile = new File(evidenceDir, "ink-playground.png");
            try (FileOutputStream output = new FileOutputStream(captureFile)) { bitmap.compress(Bitmap.CompressFormat.PNG, 100, output); }
            String sha = sha256(captureFile);
            String record = String.format(Locale.US, "{\"platform\":\"android\",\"device\":\"%s\",\"model\":\"%s\",\"sdk\":%d,\"batch\":%d,\"pressure\":%.5f,\"viewport_scale\":%.5f,\"capture\":\"%s\",\"capture_sha256\":\"%s\"}\n", Build.DEVICE, Build.MODEL, Build.VERSION.SDK_INT, batchCount, lastPressure, viewportScale, captureFile.getAbsolutePath(), sha);
            write(new File(evidenceDir, "functional-smoke.json"), record.getBytes(StandardCharsets.UTF_8));
        } catch (Exception ignored) { }
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
            case 1: return "pen";
            case 2: return "pencil";
            case 3: return "chalk";
            case 4: return "marker";
            case 5: return "water_color_lite";
            case 6: return "highlighter";
            case 7: return "laser";
            default: return "unknown";
        }
    }
    public void selectBrushFamily(int family) {
        if (family >= 1 && family <= 7) {
            selectedBrushFamily = family;
            activeBrushFamily = family;
            invalidate();
        }
    }
    public int selectedBrushFamily() { return selectedBrushFamily; }
    private static void write(File file, byte[] bytes) throws Exception { try (FileOutputStream out = new FileOutputStream(file)) { out.write(bytes); } }
    private static String sha256(File file) throws Exception { MessageDigest digest = MessageDigest.getInstance("SHA-256"); byte[] data = java.nio.file.Files.readAllBytes(file.toPath()); byte[] hash = digest.digest(data); StringBuilder value = new StringBuilder(); for (byte b : hash) value.append(String.format("%02x", b)); return value.toString(); }
    public void close() { if (handle != 0) { nativeDestroy(handle); handle = 0; } }

    private static native long nativeCreate(int width, int height);
    private static native void nativeDestroy(long handle);
    private static native int nativeBegin(long handle, int pointerId, long strokeId);
    private static native int nativeMotion(long handle, int pointerId, long sequence, long timeNs, float x, float y, float pressure, int down, int up, int tool);
    private static native int nativeCommit(long handle, int pointerId, long strokeId);
    private static native int nativeResize(long handle, int width, int height);
    private static native int nativeSurfaceLost(long handle);
    private static native int nativeCancelAll(long handle);
    private static native int nativeBrushBegin(long handle, int pointerId, int family);
    private static native int nativeBrushSample(long handle, int pointerId, long sequence, float x, float y, float pressure);
    private static native int nativeBrushFinish(long handle, int pointerId);
    private static native long nativeBrushDigest(long handle);
    private static native long nativeBrushPrimitiveCount(long handle);
    private static native int nativeBrushFamily(long handle);
    private static native int nativeBrushCanonicalMutation(long handle);
    private static native float nativeBrushSize(long handle, int pointerId);
    private static native float nativeBrushOpacity(long handle, int pointerId);
    private static native int nativeBrushRepresentation(long handle, int pointerId);
}
