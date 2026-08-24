#include <axiom/verification/windows_harness_adapter.hpp>

namespace axiom::verification::platform {

WindowsVerificationProfile windows_verification_profile() {
  return {
      "windows-native-reference-v0.1", "WINDOWS", "D3D12",
      {"semantic.projection.capture", "surface.generation", "metrics.generation",
       "surface.loss.inject", "device.loss.inject", "presentation.feedback",
       "bridge.public_facade", "bridge.data_bridge", "bridge.callback_trace",
       "input.pointer_sample_batch", "arc.preview", "arc.preview.loss.inject",
       "platform.state.capture", "surface.ownership.capture", "fault.stale_generation.inject",
       "harness.completion_tokens", "harness.source_lease_registry",
       "harness.late_event_fence", "harness.source_attempt_trace"},
      true};
}

}  // namespace axiom::verification::platform
