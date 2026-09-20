package dev.mostorm.axiom.inkplayground;

import android.app.Activity;
import android.os.Bundle;
import android.view.Window;
import android.widget.ArrayAdapter;
import android.widget.LinearLayout;
import android.widget.Spinner;

public final class MainActivity extends Activity {
    private InkPlaygroundView view;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        view = new InkPlaygroundView(this);
        LinearLayout root = new LinearLayout(this);
        root.setOrientation(LinearLayout.VERTICAL);
        Spinner brushSelector = new Spinner(this);
        brushSelector.setTag("brushSelector");
        String[] families = {"Pen", "Pencil", "Chalk", "Marker", "Water Color Lite", "Highlighter", "Laser"};
        brushSelector.setPrompt("毛笔质感 / 选择笔型");
        brushSelector.setAdapter(new ArrayAdapter<>(this, android.R.layout.simple_spinner_dropdown_item, families));
        brushSelector.setSelection(view.selectedBrushFamily() - 1);
        brushSelector.setOnItemSelectedListener(new android.widget.AdapterView.OnItemSelectedListener() {
            @Override public void onItemSelected(android.widget.AdapterView<?> parent, android.view.View item, int position, long id) {
                view.selectBrushFamily(position + 1);
            }
            @Override public void onNothingSelected(android.widget.AdapterView<?> parent) { }
        });
        root.addView(view, new LinearLayout.LayoutParams(-1, 0, 1f));
        root.addView(brushSelector, new LinearLayout.LayoutParams(-1, 128));
        setContentView(root);
    }

    @Override protected void onDestroy() {
        if (view != null) view.close();
        super.onDestroy();
    }
}
