#include "canvas/render/presentation_tracker.hpp"

#include <algorithm>

namespace canvas::render {

FramePresentationRecord* PresentationTracker::findMutable(FrameId frameId) noexcept {
    const auto iterator = std::find_if(
        _records.begin(), _records.end(),
        [frameId](const FramePresentationRecord& record) {
            return record.frame.frameId == frameId;
        });
    return iterator == _records.end() ? nullptr : &*iterator;
}

std::optional<FramePresentationRecord> PresentationTracker::find(FrameId frameId) const {
    const auto iterator = std::find_if(
        _records.begin(), _records.end(),
        [frameId](const FramePresentationRecord& record) {
            return record.frame.frameId == frameId;
        });
    if (iterator == _records.end()) {
        return std::nullopt;
    }
    return *iterator;
}

PresentFeedbackDisposition PresentationTracker::submit(const FrameState& frame) {
    if (!_lifecycle.available()) {
        return PresentFeedbackDisposition::kUnavailable;
    }
    const SurfaceSnapshot& live = _lifecycle.current();
    if (frame.viewId != live.viewId) {
        return PresentFeedbackDisposition::kWrongView;
    }
    if (frame.surfaceGeneration != live.surfaceGeneration ||
        frame.metricsGeneration != live.metricsGeneration) {
        return PresentFeedbackDisposition::kGenerationMismatch;
    }
    if (frame.metrics != live.metrics) {
        return PresentFeedbackDisposition::kMetricsMismatch;
    }
    if (findMutable(frame.frameId) != nullptr) {
        return PresentFeedbackDisposition::kDuplicate;
    }

    _records.push_back(FramePresentationRecord{
        .frame = frame,
        .state = PresentationState::kPresentSubmitted,
        .outcome = PresentOutcome::kUnknown,
        .evidence = PresentEvidenceKind::kNone,
        .presentedTimeNs = std::nullopt,
    });
    return PresentFeedbackDisposition::kSubmitted;
}

PresentFeedbackDisposition PresentationTracker::receive(const PresentedFeedback& feedback) {
    if (feedback.viewId != _lifecycle.current().viewId) {
        return PresentFeedbackDisposition::kWrongView;
    }
    FramePresentationRecord* record = findMutable(feedback.frameId);
    if (record == nullptr) {
        return PresentFeedbackDisposition::kUnknownFrame;
    }
    if (!_lifecycle.available()) {
        return PresentFeedbackDisposition::kStaleGeneration;
    }

    const SurfaceSnapshot& live = _lifecycle.current();
    if (record->frame.surfaceGeneration != live.surfaceGeneration ||
        record->frame.metricsGeneration != live.metricsGeneration) {
        return PresentFeedbackDisposition::kStaleGeneration;
    }
    if (feedback.surfaceGeneration.value() < live.surfaceGeneration.value() ||
        feedback.metricsGeneration.value() < live.metricsGeneration.value()) {
        return PresentFeedbackDisposition::kStaleGeneration;
    }
    if (feedback.surfaceGeneration != record->frame.surfaceGeneration ||
        feedback.metricsGeneration != record->frame.metricsGeneration) {
        return PresentFeedbackDisposition::kGenerationMismatch;
    }
    if (record->state == PresentationState::kPresented) {
        if (feedback.outcome == record->outcome && feedback.evidence == record->evidence &&
            feedback.presentedTimeNs == record->presentedTimeNs) {
            return PresentFeedbackDisposition::kDuplicate;
        }
        return PresentFeedbackDisposition::kFeedbackConflict;
    }
    if (feedback.outcome != PresentOutcome::kPresented) {
        return PresentFeedbackDisposition::kNonPresentedOutcome;
    }
    if (feedback.evidence != PresentEvidenceKind::kPlatformQualified) {
        return PresentFeedbackDisposition::kUnqualifiedEvidence;
    }

    record->state = PresentationState::kPresented;
    record->outcome = feedback.outcome;
    record->evidence = feedback.evidence;
    record->presentedTimeNs = feedback.presentedTimeNs;
    return PresentFeedbackDisposition::kPresented;
}

} // namespace canvas::render
