#pragma once

#include "canvas/ink/brush_preview_delta.hpp"
#include "canvas/semantic/canonical_commit_record.hpp"
#include "canvas/render/frame_state.hpp"

#include <cstdint>

namespace canvas::ink {

struct PreviewIdentity final {
  std::uint64_t documentEpoch = 0;
  std::uint64_t session = 0;
  std::uint64_t sessionGeneration = 0;
  bool operator==(const PreviewIdentity&) const = default;
};

enum class PreviewSubmitResult : std::uint8_t {
  kAccepted,
  kCanonicalOnly,
  kRejected,
};

class ArcPreviewSink {
 public:
  virtual ~ArcPreviewSink() = default;
  virtual PreviewSubmitResult begin(const PreviewIdentity&,
                                    const BrushPreviewDelta&) noexcept = 0;
  virtual PreviewSubmitResult update(const PreviewIdentity&,
                                     const BrushPreviewDelta&) noexcept = 0;
  virtual PreviewSubmitResult cancel(const PreviewIdentity&) noexcept = 0;
};

struct CanonicalHandoffIdentity final {
  std::uint64_t documentEpoch = 0;
  std::uint64_t session = 0;
  std::uint64_t sessionGeneration = 0;
  semantic::OperationId operationId{};
  semantic::CanonicalCommitStamp commit{};
  render::SurfaceGeneration surfaceGeneration{};
  bool operator==(const CanonicalHandoffIdentity&) const = default;
};

enum class HandoffResult : std::uint8_t {
  kAccepted,
  kIgnored,
  kRejected,
};

class CanonicalVisibilitySink {
 public:
  virtual ~CanonicalVisibilitySink() = default;
  virtual HandoffResult canonicalCommitted(
      const CanonicalHandoffIdentity&,
      const semantic::CanonicalCommitRecord&) noexcept = 0;
  virtual HandoffResult canonicalVisible(
      const CanonicalHandoffIdentity&,
      const render::FrameState&) noexcept = 0;
};

}  // namespace canvas::ink
