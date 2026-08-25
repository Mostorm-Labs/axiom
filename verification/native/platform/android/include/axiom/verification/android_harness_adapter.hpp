#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <axiom/verification/platform_hooks.h>

namespace axiom::verification::platform {

struct AndroidVerificationProfile {
  std::string profile_id;
  std::string platform_family;
  std::string host;
  std::string surface;
  std::string backend;
  std::vector<std::string> capabilities;
  bool arc_enabled;
};

AndroidVerificationProfile android_verification_profile();

struct AndroidMetrics {
  std::uint32_t logical_width;
  std::uint32_t logical_height;
  std::uint32_t physical_width;
  std::uint32_t physical_height;
  float device_scale;
  std::uint32_t orientation;
  bool visible;
  bool occluded;
};

struct AndroidMotionSample {
  float x;
  float y;
  float pressure;
  float tilt_x;
  float tilt_y;
  std::uint64_t timestamp_ns;
  std::int32_t tool_type;
};

struct AndroidPointerSampleBatch {
  std::string correlation_id;
  std::vector<AndroidMotionSample> samples;
};

AndroidPointerSampleBatch normalize_android_motion_history(
    std::string correlation_id,
    const std::vector<AndroidMotionSample>& historical_samples,
    AndroidMotionSample current_sample);

struct AndroidTargetOwnership {
  std::string canonical_owner;
  std::string preview_owner;
  bool distinct;
};

class AndroidHarnessAdapter {
 public:
  void create_canvas();
  void destroy_canvas();
  void attach_host();
  void detach_host();
  void attach_document();
  void app_background();
  void app_foreground();
  void update_metrics(const AndroidMetrics& metrics);
  void surface_available(std::uintptr_t native_window_token);
  void surface_lost(std::uint64_t generation);
  void rebind_surface(std::uintptr_t native_window_token);
  void device_lost(std::uint64_t generation);
  void device_recover();
  void hold_present_completions();

  [[nodiscard]] std::uint64_t semantic_revision() const noexcept;
  [[nodiscard]] std::uint64_t metrics_generation() const noexcept;
  [[nodiscard]] std::uint64_t surface_generation() const noexcept;
  [[nodiscard]] std::uint64_t device_generation() const noexcept;
  [[nodiscard]] bool document_attached() const noexcept;
  [[nodiscard]] bool backgrounded() const noexcept;
  [[nodiscard]] bool surface_ready() const noexcept;
  [[nodiscard]] std::uintptr_t native_window_token() const noexcept;
  [[nodiscard]] AndroidTargetOwnership target_ownership() const;
  [[nodiscard]] PublishDisposition release_held_after_destroy();
  [[nodiscard]] std::size_t events_after_destroy() const noexcept;

 private:
  enum class State { New, Created, HostAttached, DocumentAttached, Destroyed };
  State state_{State::New};
  bool present_held_{false};
  bool backgrounded_{false};
  bool surface_ready_{false};
  bool surface_lost_{false};
  bool device_lost_{false};
  bool document_attached_{false};
  std::uint64_t semantic_revision_{1};
  std::uint64_t metrics_generation_{1};
  std::uint64_t surface_generation_{1};
  std::uint64_t device_generation_{1};
  std::size_t events_after_destroy_{0};
  std::uintptr_t native_window_token_{0};
};

}  // namespace axiom::verification::platform
