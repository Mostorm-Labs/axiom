#include <axiom/verification/apple_harness_adapter.hpp>

namespace axiom::verification::platform {

AppleVerificationProfile apple_verification_profile(AppleDeviceClass device_class) {
  const bool ipad = device_class == AppleDeviceClass::iPad;
  return {
      ipad ? "ipados-rn-objcxx-reference-v0-1" : "ios-rn-objcxx-reference-v0-1",
      "APPLE", "RN_FABRIC_OBJCXX", "UIVIEW_CAMETAL_LAYER", "METAL",
      ipad ? "IPAD" : "IPHONE",
      {"semantic.projection.capture", "surface.generation", "metrics.generation",
       "surface.loss.inject", "device.loss.inject", "presentation.feedback",
       "bridge.public_facade", "bridge.data_bridge", "bridge.callback_trace",
       "input.pointer_sample_batch", "arc.preview", "arc.preview.loss.inject",
       "platform.state.capture", "surface.ownership.capture", "fault.stale_generation.inject",
       "fault.present_completion_hold", "harness.completion_tokens",
       "harness.source_lease_registry", "harness.late_event_fence",
       "harness.source_attempt_trace"}, true};
}

}  // namespace axiom::verification::platform
