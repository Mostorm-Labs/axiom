#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <axiom/verification/platform_hooks.h>

namespace axiom::verification::platform {

enum class AppleDeviceClass { iPhone, iPad };
enum class AppleToolType { Unknown, Finger, Pencil, Stylus };

struct AppleVerificationProfile {
  std::string profile_id;
  std::string platform_family;
  std::string host;
  std::string surface;
  std::string backend;
  std::string device_class;
  std::vector<std::string> capabilities;
  bool arc_enabled;
};

AppleVerificationProfile apple_verification_profile(AppleDeviceClass device_class);

struct AppleMetrics {
  std::uint32_t logical_width;
  std::uint32_t logical_height;
  std::uint32_t physical_width;
  std::uint32_t physical_height;
  float device_scale;
  std::uint32_t orientation;
  bool visible;
  bool occluded;
};

struct AppleMotionSample {
  float x;
  float y;
  float pressure;
  float tilt_x;
  float tilt_y;
  std::uint64_t timestamp_ns;
  AppleToolType tool_type;
};

struct ApplePointerSampleBatch {
  std::string correlation_id;
  std::vector<AppleMotionSample> samples;
};

ApplePointerSampleBatch normalize_apple_pointer_history(
    std::string correlation_id,
    const std::vector<AppleMotionSample>& historical_samples,
    AppleMotionSample current_sample);

struct AppleTargetOwnership {
  std::string canonical_owner;
  std::string preview_owner;
  bool distinct;
};

class AppleHarnessAdapter {
 public:
  explicit AppleHarnessAdapter(AppleDeviceClass device_class);

  void create_canvas();
  void destroy_canvas();
  void attach_host();
  void detach_host();
  void attach_document();
  void app_background();
  void app_foreground();
  void update_metrics(const AppleMetrics& metrics);
  void surface_available(std::uintptr_t drawable_token);
  void surface_lost(std::uint64_t generation);
  void rebind_surface(std::uintptr_t drawable_token);
  void device_lost(std::uint64_t generation);
  void device_recover();
  void hold_present_completions();

  [[nodiscard]] const AppleVerificationProfile& profile() const noexcept;
  [[nodiscard]] std::uint64_t semantic_revision() const noexcept;
  [[nodiscard]] std::uint64_t metrics_generation() const noexcept;
  [[nodiscard]] std::uint64_t surface_generation() const noexcept;
  [[nodiscard]] std::uint64_t device_generation() const noexcept;
  [[nodiscard]] bool document_attached() const noexcept;
  [[nodiscard]] bool backgrounded() const noexcept;
  [[nodiscard]] bool surface_ready() const noexcept;
  [[nodiscard]] std::uintptr_t drawable_token() const noexcept;
  [[nodiscard]] AppleTargetOwnership target_ownership() const;
  [[nodiscard]] PublishDisposition release_held_after_destroy();

 private:
  enum class State { New, Created, HostAttached, DocumentAttached, Destroyed };
  AppleVerificationProfile profile_;
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
  std::uintptr_t drawable_token_{0};
};

// The ObjC++ surface seam keeps CAMetalLayer/MTLDrawable ownership outside the
// shared adapter. It is intentionally opaque to the C++ verification contract.
bool apple_native_surface_seam_available() noexcept;

}  // namespace axiom::verification::platform
