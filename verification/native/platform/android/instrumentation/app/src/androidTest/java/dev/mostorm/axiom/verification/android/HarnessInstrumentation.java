package dev.mostorm.axiom.verification.android;

import android.app.Activity;
import android.app.Instrumentation;
import android.content.Intent;
import android.os.Bundle;

public final class HarnessInstrumentation extends Instrumentation {
  @Override
  public void onCreate(Bundle arguments) {
    super.onCreate(arguments);
    start();
  }

  @Override
  public void onStart() {
    Bundle result = new Bundle();
    try {
      Intent intent = new Intent(getTargetContext(), HarnessActivity.class);
      intent.addFlags(Intent.FLAG_ACTIVITY_NEW_TASK);
      Activity activity = startActivitySync(intent);
      waitForIdleSync();
      if (!(activity instanceof HarnessActivity)) {
        throw new IllegalStateException("unexpected Activity type");
      }
      String snapshot = ((HarnessActivity) activity).runDeterministicProbe();
      if (!snapshot.contains("\"surfaceGeneration\":") ||
          snapshot.contains("\"surfaceGeneration\":1,") ||
          !snapshot.contains("\"deviceGeneration\":2") ||
          !snapshot.contains("\"documentAttached\":true") ||
          !snapshot.contains("\"pointerBatchCount\":3")) {
        throw new IllegalStateException("Android adapter snapshot mismatch: " + snapshot);
      }
      result.putString("status", "HARNESS_STARTED");
      result.putString("snapshot", snapshot);
      activity.finish();
      finish(Activity.RESULT_OK, result);
    } catch (Throwable failure) {
      result.putString("status", "HARNESS_FAILED");
      result.putString("error", failure.toString());
      finish(Activity.RESULT_CANCELED, result);
    }
  }
}
