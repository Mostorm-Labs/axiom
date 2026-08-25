#include <axiom/verification/apple_harness_adapter.hpp>

#include <cstdlib>
#include <vector>

namespace avp = axiom::verification::platform;

namespace {
void require(bool condition) { if (!condition) std::abort(); }
}

int main() {
  const auto ios = avp::apple_verification_profile(avp::AppleDeviceClass::iPhone);
  const auto ipados = avp::apple_verification_profile(avp::AppleDeviceClass::iPad);
  require(ios.profile_id == "ios-rn-objcxx-reference-v0-1");
  require(ipados.profile_id == "ipados-rn-objcxx-reference-v0-1");
  require(ios.profile_id != ipados.profile_id);
  require(ios.backend == "METAL");

  avp::AppleHarnessAdapter adapter(avp::AppleDeviceClass::iPhone);
  const auto semantic = adapter.semantic_revision();
  adapter.create_canvas();
  adapter.attach_host();
  adapter.surface_available(101);
  adapter.attach_document();
  adapter.update_metrics({390, 844, 1170, 2532, 3.0F, 1, true, false});
  require(adapter.metrics_generation() == 2);
  require(adapter.document_attached());
  require(adapter.semantic_revision() == semantic);
  adapter.app_background();
  require(adapter.backgrounded() && adapter.document_attached());
  adapter.app_foreground();
  adapter.surface_lost(adapter.surface_generation());
  adapter.rebind_surface(202);
  require(adapter.surface_generation() == 2);
  require(adapter.document_attached());
  adapter.device_lost(adapter.device_generation());
  adapter.device_recover();
  require(adapter.device_generation() == 2);

  const auto batch = avp::normalize_apple_pointer_history(
      "pencil-01", {{1, 2, 0.2F, 0, 1, 10, avp::AppleToolType::Pencil}},
      {3, 4, 0.8F, 2, 3, 20, avp::AppleToolType::Pencil});
  require(batch.correlation_id == "pencil-01");
  require(batch.samples.size() == 2);
  require(batch.samples[1].pressure == 0.8F);
  const auto ownership = adapter.target_ownership();
  require(ownership.distinct && ownership.canonical_owner == "AXIOM" && ownership.preview_owner == "ARC");
  adapter.hold_present_completions();
  adapter.destroy_canvas();
  require(adapter.release_held_after_destroy() == avp::PublishDisposition::DroppedStaleScope);
  return 0;
}
