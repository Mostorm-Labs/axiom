package dev.mostorm.axiom.inkplayground;

import android.app.Activity;
import android.os.Bundle;
import android.view.Window;

public final class MainActivity extends Activity {
    private InkPlaygroundView view;

    @Override public void onCreate(Bundle state) {
        super.onCreate(state);
        requestWindowFeature(Window.FEATURE_NO_TITLE);
        view = new InkPlaygroundView(this);
        setContentView(view);
    }

    @Override protected void onDestroy() {
        if (view != null) view.close();
        super.onDestroy();
    }
}
