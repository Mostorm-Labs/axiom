#include <axiom/verification/android_harness_adapter.hpp>

#include <cstdlib>
#include <string>
#include <vector>

namespace avp = axiom::verification::platform;

namespace {
void require(bool condition) {
  if (!condition) std::abort();
}
}  // namespace

int main() {
  const auto profile = avp::android_verification_profile();
  require(profile.profile_id == "android-instrumentation-reference-v0.1");
  require(profile.platform_family == "ANDROID");
  require(profile.host == "ACTIVITY_VIEW_JNI");
  require(profile.arc_enabled);

  avp::AndroidHarnessAdapter adapter;
  const auto semantic_before = adapter.semantic_revision();
  adapter.create_canvas();
  adapter.attach_host();
  adapter.surface_available(101);
  adapter.attach_document();
  adapter.update_metrics({800, 600, 2400, 1800, 3.0F, 0, true, false});
  require(adapter.metrics_generation() == 2);
  require(adapter.semantic_revision() == semantic_before);

  adapter.app_background();
  require(adapter.backgrounded());
  require(adapter.document_attached());
  require(adapter.semantic_revision() == semantic_before);
  adapter.app_foreground();
  require(!adapter.backgrounded());

  const auto surface_before = adapter.surface_generation();
  adapter.surface_lost(surface_before);
  require(!adapter.surface_ready());
  require(adapter.document_attached());
  adapter.rebind_surface(202);
  require(adapter.surface_ready());
  require(adapter.native_window_token() == 202);
  require(adapter.surface_generation() == surface_before + 1);
  require(adapter.semantic_revision() == semantic_before);

  const auto device_before = adapter.device_generation();
  adapter.device_lost(device_before);
  adapter.device_recover();
  require(adapter.device_generation() == device_before + 1);
  require(adapter.semantic_revision() == semantic_before);

  const std::vector<avp::AndroidMotionSample> history{
      {10.0F, 20.0F, 0.2F, 0.0F, 0.0F, 1, 2},
      {12.0F, 22.0F, 0.6F, 4.0F, -2.0F, 2, 2},
  };
  const auto batch = avp::normalize_android_motion_history(
      "batch:android:1", history,
      {16.0F, 26.0F, 0.9F, 8.0F, -4.0F, 3, 2});
  require(batch.correlation_id == "batch:android:1");
  require(batch.samples.size() == 3);
  require(batch.samples[0].timestamp_ns == 1);
  require(batch.samples[2].pressure == 0.9F);

  const auto ownership = adapter.target_ownership();
  require(ownership.canonical_owner == "AXIOM");
  require(ownership.preview_owner == "ARC");
  require(ownership.distinct);

  adapter.hold_present_completions();
  adapter.destroy_canvas();
  require(adapter.release_held_after_destroy() ==
          avp::PublishDisposition::DroppedStaleScope);
  require(adapter.events_after_destroy() == 0);
  return 0;
}
