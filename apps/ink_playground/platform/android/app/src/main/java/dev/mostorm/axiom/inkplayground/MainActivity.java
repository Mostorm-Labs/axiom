package dev.mostorm.axiom.inkplayground;

import android.app.Activity;
import android.os.Bundle;
import android.graphics.Color;
import android.view.Gravity;
import android.view.Window;
import android.widget.Button;
import android.widget.FrameLayout;

public final class MainActivity extends Activity {
    private InkPlaygroundView view;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        view = new InkPlaygroundView(this);
        FrameLayout root = new FrameLayout(this);
        root.addView(view, new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.MATCH_PARENT));
        Button policy = new Button(this);
        policy.setText("双指模式：AutoIntent");
        policy.setTextColor(Color.WHITE);
        policy.setBackgroundColor(Color.rgb(35, 91, 210));
        policy.setOnClickListener(v ->
                policy.setText("双指模式：" + view.cycleMultiContactPolicy()));
        FrameLayout.LayoutParams policyParams = new FrameLayout.LayoutParams(
                FrameLayout.LayoutParams.MATCH_PARENT, FrameLayout.LayoutParams.WRAP_CONTENT,
                Gravity.BOTTOM);
        policyParams.setMargins(24, 0, 24, 24);
        root.addView(policy, policyParams);
        setContentView(root);
    }

    @Override protected void onDestroy() {
        if (view != null) view.close();
        super.onDestroy();
    }
}
