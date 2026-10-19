#pragma once

#include "canvas/render/frame_state.hpp"

#include <cstdint>
#include <optional>

namespace canvas::render {

enum class FrameRole : std::uint8_t { kCanonical, kPreview };

struct RuntimeFrameIdentity final {
  ViewId viewId{};
  FrameId frameId{};
  std::uint64_t documentRevision = 0;
  std::uint64_t sceneGeneration = 0;
  SurfaceGeneration surfaceGeneration{};
  MetricsGeneration metricsGeneration{};
  friend bool operator==(const RuntimeFrameIdentity&, const RuntimeFrameIdentity&) = default;
};

struct RuntimeFrameCapture final {
  RuntimeFrameIdentity identity{};
  SurfaceMetrics metrics{};
  FrameRole role = FrameRole::kCanonical;
};

enum class RuntimeFrameDisposition : std::uint8_t {
  kAccepted,
  kRetry,
  kStale,
  kRejected,
  kUnavailable,
};

enum class RuntimePresentEvidence : std::uint8_t {
  kNone,
  kApproximate,
  kPlatformQualified,
};

struct RuntimePresentFeedback final {
  RuntimeFrameIdentity identity{};
  FrameRole role = FrameRole::kCanonical;
  bool presented = false;
  RuntimePresentEvidence evidence = RuntimePresentEvidence::kNone;
};

class RuntimeFrameCoordinator final {
 public:
  [[nodiscard]] RuntimeFrameDisposition request(RuntimeFrameCapture capture) noexcept;
  [[nodiscard]] RuntimeFrameDisposition submit(
      FrameRole role, FrameId frameId, bool busy) noexcept;
  [[nodiscard]] RuntimeFrameDisposition receive(
      const RuntimePresentFeedback& feedback) noexcept;
  [[nodiscard]] bool retirePreview(
      const RuntimeFrameIdentity& canonical) const noexcept;
  [[nodiscard]] bool pending(FrameRole role) const noexcept;
  [[nodiscard]] bool presented(FrameRole role) const noexcept;
  [[nodiscard]] const RuntimeFrameCapture* capture(FrameRole role) const noexcept;
  void invalidate(FrameRole role) noexcept;
  void close() noexcept;

 private:
  struct Slot final {
    std::optional<RuntimeFrameCapture> capture;
    bool submitted = false;
    bool presented = false;
    bool qualified = false;
  };
  Slot& slot(FrameRole role) noexcept { return role == FrameRole::kCanonical ? canonical_ : preview_; }
  const Slot& slot(FrameRole role) const noexcept { return role == FrameRole::kCanonical ? canonical_ : preview_; }
  Slot canonical_{};
  Slot preview_{};
  bool closed_ = false;
};

}  // namespace canvas::render
