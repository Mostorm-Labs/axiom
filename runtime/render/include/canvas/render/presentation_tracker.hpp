#pragma once

#include "canvas/render/frame_state.hpp"
#include "canvas/render/surface_lifecycle.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::render {

enum class PresentOutcome : std::uint8_t {
    kPresented,
    kDropped,
    kFailed,
    kUnknown,
};

enum class PresentEvidenceKind : std::uint8_t {
    kPlatformQualified,
    kApproximate,
    kNone,
};

enum class PresentationState : std::uint8_t {
    kPresentSubmitted,
    kPresented,
};

enum class PresentFeedbackDisposition : std::uint8_t {
    kSubmitted,
    kPresented,
    kDuplicate,
    kFeedbackConflict,
    kUnavailable,
    kWrongView,
    kUnknownFrame,
    kGenerationMismatch,
    kStaleGeneration,
    kMetricsMismatch,
    kUnqualifiedEvidence,
    kNonPresentedOutcome,
};

struct PresentedFeedback final {
    ViewId viewId{};
    FrameId frameId{};
    SurfaceGeneration surfaceGeneration{};
    MetricsGeneration metricsGeneration{};
    PresentOutcome outcome = PresentOutcome::kUnknown;
    PresentEvidenceKind evidence = PresentEvidenceKind::kNone;
    std::optional<std::uint64_t> presentedTimeNs;

    bool operator==(const PresentedFeedback&) const = default;
};

struct FramePresentationRecord final {
    FrameState frame;
    PresentationState state = PresentationState::kPresentSubmitted;
    PresentOutcome outcome = PresentOutcome::kUnknown;
    PresentEvidenceKind evidence = PresentEvidenceKind::kNone;
    std::optional<std::uint64_t> presentedTimeNs;

    bool operator==(const FramePresentationRecord&) const = default;
};

class PresentationTracker final {
  public:
    explicit PresentationTracker(const SurfaceLifecycle& lifecycle) noexcept
        : _lifecycle(lifecycle) {}

    [[nodiscard]] PresentFeedbackDisposition submit(const FrameState& frame);
    [[nodiscard]] PresentFeedbackDisposition receive(const PresentedFeedback& feedback);
    [[nodiscard]] std::optional<FramePresentationRecord> find(FrameId frameId) const;

  private:
    [[nodiscard]] FramePresentationRecord* findMutable(FrameId frameId) noexcept;

    const SurfaceLifecycle& _lifecycle;
    std::vector<FramePresentationRecord> _records;
};

} // namespace canvas::render
