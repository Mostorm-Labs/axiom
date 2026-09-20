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
    private static final class Point { final float x, y, pressure; Point(float x, float y, float p) { this.x=x; this.y=y; this.pressure=p; } }
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

    public InkPlaygroundView(Context context) {
        super(context);
        setBackgroundColor(Color.WHITE);
        ink.setColor(Color.rgb(26, 91, 255)); ink.setStyle(Paint.Style.STROKE); ink.setStrokeWidth(6f); ink.setStrokeCap(Paint.Cap.ROUND); ink.setStrokeJoin(Paint.Join.ROUND);
        hud.setColor(Color.rgb(37, 48, 74)); hud.setTextSize(28f);
        evidenceDir = new File(context.getFilesDir(), "g4-12-android");
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
                canvas.drawLine(a.x, a.y, b.x, b.y, ink);
            }
        }
        for (Stroke active : activeStrokes.values()) for (int i = 1; i < active.points.size(); ++i) {
            Point a = active.points.get(i - 1), b = active.points.get(i);
            canvas.drawLine(a.x, a.y, b.x, b.y, ink);
        }
        hud.setStyle(Paint.Style.FILL);
        canvas.drawText(String.format(Locale.US, "Axiom Ink  |  strokes %d  points %d  tool %s  pressure %.2f", strokes.size(), pointCount(), lastTool, lastPressure), 24f, 42f, hud);
        canvas.drawText("Touch or stylus input  |  functional smoke", 24f, 78f, hud);
    }

    private int pointCount() { int count = 0; for (Stroke active : activeStrokes.values()) count += active.points.size(); for (Stroke s : strokes) count += s.points.size(); return count; }

    @Override public boolean onTouchEvent(MotionEvent event) {
        final int action = event.getActionMasked();
        if (action == MotionEvent.ACTION_CANCEL) { if (handle != 0) nativeSurfaceLost(handle); activeStrokes.clear(); pointerStrokeIds.clear(); invalidate(); return true; }
        if (action != MotionEvent.ACTION_DOWN && action != MotionEvent.ACTION_POINTER_DOWN &&
            action != MotionEvent.ACTION_MOVE && action != MotionEvent.ACTION_UP &&
            action != MotionEvent.ACTION_POINTER_UP) return true;
        if (action == MotionEvent.ACTION_DOWN || action == MotionEvent.ACTION_POINTER_DOWN) {
            final int pointerIndex = event.getActionIndex();
            final int pointerId = event.getPointerId(pointerIndex);
            if (handle == 0) handle = nativeCreate(getWidth(), getHeight());
            strokeId++;
            Stroke active = new Stroke(); activeStrokes.put(pointerId, active);
            pointerStrokeIds.put(pointerId, strokeId);
            nativeBegin(handle, pointerId, strokeId);
            emitPointer(event, pointerIndex, true, false);
        } else if (action == MotionEvent.ACTION_MOVE) {
            for (int pointerIndex = 0; pointerIndex < event.getPointerCount(); ++pointerIndex) {
                emitPointer(event, pointerIndex, false, false);
            }
        } else {
            final int pointerIndex = event.getActionIndex();
            emitPointer(event, pointerIndex, false, true);
        }
        invalidate(); return true;
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
            lastTool = tool == MotionEvent.TOOL_TYPE_STYLUS ? "stylus" : tool == MotionEvent.TOOL_TYPE_ERASER ? "eraser" : "touch";
            lastPressure = pressure; active.points.add(new Point(x, y, pressure));
            if (!traceFirst) trace.append(",\n"); traceFirst = false;
            trace.append(String.format(Locale.US, "  {\"stroke\":%d,\"pointer_id\":%d,\"sequence\":%d,\"x\":%.3f,\"y\":%.3f,\"pressure\":%.5f,\"tool\":\"%s\",\"time_ns\":%d}", activeStrokeId, pointerId, ++sequence, x, y, pressure, lastTool, timeNs));
            nativeMotion(handle, pointerId, sequence, timeNs, x, y, pressure, down && i == 0 ? 1 : 0, up && i == history ? 1 : 0, tool);
        }
        batchCount = count;
        if (up) { nativeCommit(handle, pointerId, activeStrokeId); strokes.add(active); activeStrokes.remove(pointerId); pointerStrokeIds.remove(pointerId); persistEvidence(); }
    }

    private void persistEvidence() {
        try {
            String json = trace + "\n]\n";
            write(new File(evidenceDir, "pointer-trace.json"), json.getBytes(StandardCharsets.UTF_8));
            Bitmap bitmap = Bitmap.createBitmap(Math.max(1, getWidth()), Math.max(1, getHeight()), Bitmap.Config.ARGB_8888);
            Canvas capture = new Canvas(bitmap); draw(capture);
            File captureFile = new File(evidenceDir, "ink-playground.png");
            try (FileOutputStream output = new FileOutputStream(captureFile)) { bitmap.compress(Bitmap.CompressFormat.PNG, 100, output); }
            String sha = sha256(captureFile);
            String record = String.format(Locale.US, "{\"platform\":\"android\",\"device\":\"%s\",\"model\":\"%s\",\"sdk\":%d,\"batch\":%d,\"pressure\":%.5f,\"capture\":\"%s\",\"capture_sha256\":\"%s\"}\n", Build.DEVICE, Build.MODEL, Build.VERSION.SDK_INT, batchCount, lastPressure, captureFile.getAbsolutePath(), sha);
            write(new File(evidenceDir, "functional-smoke.json"), record.getBytes(StandardCharsets.UTF_8));
        } catch (Exception ignored) { }
    }
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
}
