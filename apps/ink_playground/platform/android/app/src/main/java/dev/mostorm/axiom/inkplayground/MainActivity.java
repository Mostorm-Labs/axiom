package dev.mostorm.axiom.inkplayground;

import android.app.Activity;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.LinearLayout;
import android.widget.FrameLayout;
import android.widget.Spinner;

public final class MainActivity extends Activity {
    private InkPlaygroundView view;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        view = new InkPlaygroundView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        FrameLayout surfaceStack = new FrameLayout(this);
        surfaceStack.setBackgroundColor(Color.WHITE);
        InkRenderSurfaceView canonical = new InkRenderSurfaceView(view, false);
        InkRenderSurfaceView preview = new InkRenderSurfaceView(view, true);
        // Keep all three canvas/input layers in the exact same local coordinate
        // space.  The input View previously inherited the FrameLayout bounds,
        // while the SurfaceViews could be laid out with a different top inset
        // on edge-to-edge devices; that made MotionEvent coordinates and EGL
        // pixels disagree.  Explicit MATCH_PARENT params make the origin and
        // drawable size identical for Runtime and both providers.
        final FrameLayout.LayoutParams canvasParams =
                new FrameLayout.LayoutParams(-1, -1);
        surfaceStack.addView(canonical, new FrameLayout.LayoutParams(canvasParams));
        surfaceStack.addView(preview, new FrameLayout.LayoutParams(canvasParams));
        surfaceStack.addView(view, new FrameLayout.LayoutParams(canvasParams));

        Spinner brushSelector = new Spinner(this);
        brushSelector.setTag("brushSelector");
        String[] families = {"vector-solid-v1"};
        brushSelector.setPrompt("毛笔质感 / 选择笔型");
        brushSelector.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, families));
        brushSelector.setSelection(view.selectedBrushFamily() - 1);
        brushSelector.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, android.view.View item, int position, long id) {
                view.selectBrushFamily(position + 1);
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) { }
        });

        Button policy = new Button(this);
        policy.setText("双指模式：AutoIntent");
        policy.setTextColor(Color.WHITE);
        policy.setBackgroundColor(Color.rgb(35, 91, 210));
        policy.setOnClickListener(v ->
                policy.setText("双指模式：" + view.cycleMultiContactPolicy()));

        root.addView(surfaceStack, new LinearLayout.LayoutParams(-1, 0, 1f));
        root.addView(brushSelector, new LinearLayout.LayoutParams(-1, 128));
        root.addView(policy, new LinearLayout.LayoutParams(-1, -2));
        setContentView(root);
    }

    @Override protected void onDestroy() {
        if (view != null) view.close();
        super.onDestroy();
    }
}
