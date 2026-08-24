#pragma once

#include <cstdint>
#include <string>
#include <vector>

#include <axiom/verification/platform_hooks.h>
#include <axiom/verification/platform_host_common.hpp>

namespace axiom::verification::platform {

struct WindowsVerificationProfile {
  std::string profile_id;
  std::string platform_family;
  std::string backend;
  std::vector<std::string> capabilities;
  bool arc_enabled;
};

WindowsVerificationProfile windows_verification_profile();

struct WindowsMetrics {
  std::uint32_t logical_width;
  std::uint32_t logical_height;
  std::uint32_t physical_width;
  std::uint32_t physical_height;
  float device_scale;
  bool visible;
  bool occluded;
};

struct NativePointerHistory {
  float x;
  float y;
  float pressure;
  float tilt_x;
  float tilt_y;
  std::uint64_t timestamp;
};

struct NormalizedPointerSample {
  float x;
  float y;
  float pressure;
  float tilt_x;
  float tilt_y;
  std::uint64_t timestamp;
};

struct PointerSampleBatch {
  std::string correlation_id;
  std::vector<NormalizedPointerSample> samples;
};

PointerSampleBatch normalize_windows_pointer_history(
    std::string correlation_id, const std::vector<NativePointerHistory>& history);

struct TargetOwnership {
  std::string canonical_owner;
  std::string preview_owner;
  bool distinct;
};

class WindowsHarnessAdapter {
 public:
  WindowsHarnessAdapter();
  ~WindowsHarnessAdapter();

  void create_canvas();
  void destroy_canvas();
  void attach_host();
  void attach_document();
  void update_metrics(const WindowsMetrics& metrics);
  void surface_lost(std::uint64_t generation);
  void rebind_surface();
  void device_lost(std::uint64_t generation);
  void device_recover();
  void hold_present_completions();

  [[nodiscard]] std::uint64_t semantic_revision() const noexcept;
  [[nodiscard]] std::uint64_t metrics_generation() const noexcept;
  [[nodiscard]] std::uint64_t surface_generation() const noexcept;
  [[nodiscard]] std::uint64_t device_generation() const noexcept;
  [[nodiscard]] bool document_attached() const noexcept;
  [[nodiscard]] TargetOwnership target_ownership() const;
  [[nodiscard]] bool native_surface_ready() const noexcept;
  [[nodiscard]] PublishDisposition release_held_after_destroy();
  [[nodiscard]] std::size_t events_after_destroy() const noexcept;

 private:
  enum class State { New, Created, HostAttached, DocumentAttached, Destroyed };
  State state_{State::New};
  bool present_held_{false};
  bool surface_lost_{false};
  bool device_lost_{false};
  bool document_attached_{false};
  std::uint64_t semantic_revision_{1};
  std::uint64_t metrics_generation_{1};
  std::uint64_t surface_generation_{1};
  std::uint64_t device_generation_{1};
  std::size_t events_after_destroy_{0};
  void* native_state_{nullptr};
  bool native_surface_ready_{false};
};

}  // namespace axiom::verification::platform
