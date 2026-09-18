#include "canvas/render/presentation_tracker.hpp"
#include "canvas/render/surface_lifecycle.hpp"

#include "canvas/foundation/revision.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/semantic_generation.hpp"

#include <cassert>
#include <cstdint>
#include <iterator>
#include <limits>
#include <optional>

namespace {

using canvas::foundation::SceneRevision;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::FrameId;
using canvas::render::FramePresentationRecord;
using canvas::render::FrameState;
using canvas::render::MetricsGeneration;
using canvas::render::PresentEvidenceKind;
using canvas::render::PresentFeedbackDisposition;
using canvas::render::PresentOutcome;
using canvas::render::PresentationState;
using canvas::render::PresentationTracker;
using canvas::render::PresentedFeedback;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceLifecycle;
using canvas::render::SurfaceLifecycleDisposition;
using canvas::render::SurfaceMetrics;
using canvas::render::SurfaceSnapshot;
using canvas::render::ViewId;
using canvas::semantic::SemanticGeneration;

constexpr ViewId kView{41};
constexpr ViewId kWrongView{42};

SurfaceMetrics metrics(std::uint32_t physicalWidth = 1600U,
                       std::uint32_t physicalHeight = 1200U,
                       float dpr = 2.0F) {
    return SurfaceMetrics{800.0F, 600.0F, physicalWidth, physicalHeight, dpr, 1.25F};
}

SurfaceSnapshot snapshot(SurfaceGeneration surface,
                         MetricsGeneration metricsGeneration,
                         SurfaceMetrics value = metrics(),
                         ViewId view = kView) {
    return SurfaceSnapshot{view, surface, metricsGeneration, value};
}

FrameState frame(FrameId id,
                 SurfaceGeneration surface,
                 MetricsGeneration metricsGeneration,
                 SurfaceMetrics value = metrics(),
                 ViewId view = kView) {
    return FrameState{
        .viewId = view,
        .camera = CameraState{WorldPoint{17.0F, -23.0F}, 1.75F, 0.25F,
                              CameraGeneration{43}},
        .worldViewport = WorldRect{-200.0F, -100.0F, 600.0F, 500.0F},
        .metrics = value,
        .sceneGeneration = SemanticGeneration{47},
        .sceneReadToken = SceneRevision{53},
        .surfaceGeneration = surface,
        .metricsGeneration = metricsGeneration,
        .frameId = id,
    };
}

PresentedFeedback feedback(FrameId id,
                           SurfaceGeneration surface,
                           MetricsGeneration metricsGeneration,
                           PresentOutcome outcome = PresentOutcome::kPresented,
                           PresentEvidenceKind evidence = PresentEvidenceKind::kPlatformQualified,
                           std::optional<std::uint64_t> time = 919U,
                           ViewId view = kView) {
    return PresentedFeedback{view, id, surface, metricsGeneration, outcome, evidence, time};
}

void independentGenerationAxesAndLossAreAtomic() {
    SurfaceLifecycle lifecycle{snapshot(SurfaceGeneration{10}, MetricsGeneration{20})};
    assert(lifecycle.acquire().disposition == SurfaceLifecycleDisposition::kAcquired);
    assert(lifecycle.acquire().snapshot ==
           snapshot(SurfaceGeneration{10}, MetricsGeneration{20}));

    const SurfaceMetrics resizedMetrics = metrics(1800U, 1350U, 2.25F);
    assert(lifecycle.replace(snapshot(SurfaceGeneration{10}, MetricsGeneration{21},
                                      resizedMetrics)) ==
           SurfaceLifecycleDisposition::kReplaced);
    assert(lifecycle.current() ==
           snapshot(SurfaceGeneration{10}, MetricsGeneration{21}, resizedMetrics));

    assert(lifecycle.replace(snapshot(SurfaceGeneration{11}, MetricsGeneration{21},
                                      resizedMetrics)) ==
           SurfaceLifecycleDisposition::kReplaced);
    assert(lifecycle.current() ==
           snapshot(SurfaceGeneration{11}, MetricsGeneration{21}, resizedMetrics));

    const SurfaceMetrics illegalSurfaceOnlyMetrics = metrics(1801U, 1350U, 2.25F);
    assert(lifecycle.replace(snapshot(SurfaceGeneration{12}, MetricsGeneration{21},
                                      illegalSurfaceOnlyMetrics)) ==
           SurfaceLifecycleDisposition::kMetricsMismatch);
    assert(lifecycle.current() ==
           snapshot(SurfaceGeneration{11}, MetricsGeneration{21}, resizedMetrics));

    const SurfaceMetrics combinedMetrics = metrics(2048U, 1536U, 2.5F);
    assert(lifecycle.replace(snapshot(SurfaceGeneration{12}, MetricsGeneration{22},
                                      combinedMetrics)) ==
           SurfaceLifecycleDisposition::kReplaced);
    const SurfaceSnapshot lastValid = lifecycle.current();

    const SurfaceSnapshot invalidCases[] = {
        snapshot(SurfaceGeneration{12}, MetricsGeneration{22}, combinedMetrics),
        snapshot(SurfaceGeneration{11}, MetricsGeneration{23}, combinedMetrics),
        snapshot(SurfaceGeneration{13}, MetricsGeneration{21}, combinedMetrics),
        snapshot(SurfaceGeneration{0}, MetricsGeneration{23}, combinedMetrics),
        snapshot(SurfaceGeneration{13}, MetricsGeneration{0}, combinedMetrics),
        snapshot(SurfaceGeneration{std::numeric_limits<std::uint64_t>::max()},
                 MetricsGeneration{23}, combinedMetrics),
        snapshot(SurfaceGeneration{13}, MetricsGeneration{23}, combinedMetrics, kWrongView),
        snapshot(SurfaceGeneration{13}, MetricsGeneration{23},
                 SurfaceMetrics{0.0F, 600.0F, 2048U, 1536U, 2.5F, 1.25F}),
        snapshot(SurfaceGeneration{13}, MetricsGeneration{23},
                 SurfaceMetrics{800.0F, 600.0F, 0U, 1536U, 2.5F, 1.25F}),
        snapshot(SurfaceGeneration{13}, MetricsGeneration{23},
                 SurfaceMetrics{800.0F, 600.0F, 2048U, 1536U,
                                std::numeric_limits<float>::infinity(), 1.25F}),
    };
    const SurfaceLifecycleDisposition expected[] = {
        SurfaceLifecycleDisposition::kNoGenerationAdvance,
        SurfaceLifecycleDisposition::kStaleGeneration,
        SurfaceLifecycleDisposition::kStaleGeneration,
        SurfaceLifecycleDisposition::kInvalidGeneration,
        SurfaceLifecycleDisposition::kInvalidGeneration,
        SurfaceLifecycleDisposition::kGenerationExhausted,
        SurfaceLifecycleDisposition::kWrongView,
        SurfaceLifecycleDisposition::kInvalidMetrics,
        SurfaceLifecycleDisposition::kInvalidMetrics,
        SurfaceLifecycleDisposition::kInvalidMetrics,
    };
    for (std::size_t index = 0; index < std::size(invalidCases); ++index) {
        assert(lifecycle.replace(invalidCases[index]) == expected[index]);
        assert(lifecycle.current() == lastValid);
    }

    assert(lifecycle.markLost(kWrongView) == SurfaceLifecycleDisposition::kWrongView);
    assert(lifecycle.available());
    assert(lifecycle.markLost(kView) == SurfaceLifecycleDisposition::kLost);
    assert(!lifecycle.available());
    assert(lifecycle.acquire().disposition == SurfaceLifecycleDisposition::kUnavailable);
    assert(!lifecycle.acquire().snapshot.has_value());
    assert(lifecycle.replace(snapshot(SurfaceGeneration{12}, MetricsGeneration{23},
                                      combinedMetrics)) ==
           SurfaceLifecycleDisposition::kStaleGeneration);
    assert(!lifecycle.available());
    assert(lifecycle.replace(snapshot(SurfaceGeneration{13}, MetricsGeneration{22},
                                      combinedMetrics)) ==
           SurfaceLifecycleDisposition::kReplaced);
    assert(lifecycle.available());
}

void submittedAndPresentedRemainDistinct() {
    SurfaceLifecycle lifecycle{snapshot(SurfaceGeneration{30}, MetricsGeneration{40})};
    PresentationTracker tracker{lifecycle};
    const FrameState submitted = frame(FrameId{50}, SurfaceGeneration{30},
                                       MetricsGeneration{40});

    assert(tracker.submit(submitted) == PresentFeedbackDisposition::kSubmitted);
    assert(tracker.find(FrameId{50}).has_value());
    assert(tracker.find(FrameId{50})->state == PresentationState::kPresentSubmitted);
    assert(!tracker.find(FrameId{50})->presentedTimeNs.has_value());

    assert(tracker.receive(feedback(FrameId{50}, SurfaceGeneration{30},
                                    MetricsGeneration{40})) ==
           PresentFeedbackDisposition::kPresented);
    const FramePresentationRecord record = *tracker.find(FrameId{50});
    assert(record.frame == submitted);
    assert(record.state == PresentationState::kPresented);
    assert(record.outcome == PresentOutcome::kPresented);
    assert(record.evidence == PresentEvidenceKind::kPlatformQualified);
    assert(record.presentedTimeNs == std::optional<std::uint64_t>{919U});
}

void staleGenerationAIsRejectedAndFreshBAlonePresents() {
    SurfaceLifecycle lifecycle{snapshot(SurfaceGeneration{60}, MetricsGeneration{70})};
    PresentationTracker tracker{lifecycle};
    assert(tracker.submit(frame(FrameId{80}, SurfaceGeneration{60}, MetricsGeneration{70})) ==
           PresentFeedbackDisposition::kSubmitted);

    const SurfaceMetrics nextMetrics = metrics(1900U, 1400U, 2.5F);
    assert(lifecycle.replace(snapshot(SurfaceGeneration{61}, MetricsGeneration{71},
                                      nextMetrics)) ==
           SurfaceLifecycleDisposition::kReplaced);
    const FramePresentationRecord beforeA = *tracker.find(FrameId{80});
    assert(tracker.receive(feedback(FrameId{80}, SurfaceGeneration{60},
                                    MetricsGeneration{70})) ==
           PresentFeedbackDisposition::kStaleGeneration);
    assert(*tracker.find(FrameId{80}) == beforeA);

    assert(tracker.submit(frame(FrameId{81}, SurfaceGeneration{61}, MetricsGeneration{71},
                                nextMetrics)) ==
           PresentFeedbackDisposition::kSubmitted);
    assert(tracker.receive(feedback(FrameId{81}, SurfaceGeneration{61},
                                    MetricsGeneration{71})) ==
           PresentFeedbackDisposition::kPresented);
    assert(tracker.find(FrameId{80})->state == PresentationState::kPresentSubmitted);
    assert(tracker.find(FrameId{81})->state == PresentationState::kPresented);

    assert(lifecycle.markLost(kView) == SurfaceLifecycleDisposition::kLost);
    const FramePresentationRecord beforeLost = *tracker.find(FrameId{81});
    assert(tracker.receive(feedback(FrameId{81}, SurfaceGeneration{61},
                                    MetricsGeneration{71})) ==
           PresentFeedbackDisposition::kStaleGeneration);
    assert(*tracker.find(FrameId{81}) == beforeLost);
}

void completeFeedbackNegativesAreNonMutatingAndDuplicatesIdempotent() {
    SurfaceLifecycle lifecycle{snapshot(SurfaceGeneration{100}, MetricsGeneration{200})};
    PresentationTracker tracker{lifecycle};
    assert(tracker.receive(feedback(FrameId{299}, SurfaceGeneration{100},
                                    MetricsGeneration{200})) ==
           PresentFeedbackDisposition::kUnknownFrame);

    const FrameState first = frame(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200});
    const FrameState second = frame(FrameId{301}, SurfaceGeneration{100}, MetricsGeneration{200});
    assert(tracker.submit(first) == PresentFeedbackDisposition::kSubmitted);
    assert(tracker.submit(second) == PresentFeedbackDisposition::kSubmitted);
    assert(tracker.submit(first) == PresentFeedbackDisposition::kDuplicate);

    const PresentedFeedback negativeCases[] = {
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200},
                 PresentOutcome::kPresented, PresentEvidenceKind::kPlatformQualified, 919U,
                 kWrongView),
        feedback(FrameId{999}, SurfaceGeneration{100}, MetricsGeneration{200}),
        feedback(FrameId{300}, SurfaceGeneration{99}, MetricsGeneration{200}),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{199}),
        feedback(FrameId{300}, SurfaceGeneration{101}, MetricsGeneration{200}),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{201}),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200},
                 PresentOutcome::kPresented, PresentEvidenceKind::kApproximate),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200},
                 PresentOutcome::kPresented, PresentEvidenceKind::kNone),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200},
                 PresentOutcome::kDropped),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200},
                 PresentOutcome::kFailed),
        feedback(FrameId{300}, SurfaceGeneration{100}, MetricsGeneration{200},
                 PresentOutcome::kUnknown),
    };
    const PresentFeedbackDisposition expected[] = {
        PresentFeedbackDisposition::kWrongView,
        PresentFeedbackDisposition::kUnknownFrame,
        PresentFeedbackDisposition::kStaleGeneration,
        PresentFeedbackDisposition::kStaleGeneration,
        PresentFeedbackDisposition::kGenerationMismatch,
        PresentFeedbackDisposition::kGenerationMismatch,
        PresentFeedbackDisposition::kUnqualifiedEvidence,
        PresentFeedbackDisposition::kUnqualifiedEvidence,
        PresentFeedbackDisposition::kNonPresentedOutcome,
        PresentFeedbackDisposition::kNonPresentedOutcome,
        PresentFeedbackDisposition::kNonPresentedOutcome,
    };
    for (std::size_t index = 0; index < std::size(negativeCases); ++index) {
        const FramePresentationRecord firstBefore = *tracker.find(FrameId{300});
        const FramePresentationRecord secondBefore = *tracker.find(FrameId{301});
        assert(tracker.receive(negativeCases[index]) == expected[index]);
        assert(*tracker.find(FrameId{300}) == firstBefore);
        assert(*tracker.find(FrameId{301}) == secondBefore);
    }

    assert(tracker.receive(feedback(FrameId{300}, SurfaceGeneration{100},
                                    MetricsGeneration{200})) ==
           PresentFeedbackDisposition::kPresented);
    const FramePresentationRecord presented = *tracker.find(FrameId{300});
    assert(tracker.find(FrameId{301})->state == PresentationState::kPresentSubmitted);
    assert(tracker.receive(feedback(FrameId{300}, SurfaceGeneration{100},
                                    MetricsGeneration{200})) ==
           PresentFeedbackDisposition::kDuplicate);
    assert(*tracker.find(FrameId{300}) == presented);
    assert(tracker.find(FrameId{301})->state == PresentationState::kPresentSubmitted);
    assert(tracker.receive(feedback(FrameId{300}, SurfaceGeneration{100},
                                    MetricsGeneration{200}, PresentOutcome::kPresented,
                                    PresentEvidenceKind::kPlatformQualified, 920U)) ==
           PresentFeedbackDisposition::kFeedbackConflict);
    assert(*tracker.find(FrameId{300}) == presented);
    assert(tracker.find(FrameId{301})->state == PresentationState::kPresentSubmitted);
}

void mismatchedSubmissionCannotEnterTracker() {
    SurfaceLifecycle lifecycle{snapshot(SurfaceGeneration{400}, MetricsGeneration{500})};
    PresentationTracker tracker{lifecycle};
    assert(tracker.submit(frame(FrameId{600}, SurfaceGeneration{400}, MetricsGeneration{500},
                                metrics(), kWrongView)) ==
           PresentFeedbackDisposition::kWrongView);
    assert(tracker.submit(frame(FrameId{601}, SurfaceGeneration{399}, MetricsGeneration{500})) ==
           PresentFeedbackDisposition::kGenerationMismatch);
    assert(tracker.submit(frame(FrameId{602}, SurfaceGeneration{400}, MetricsGeneration{499})) ==
           PresentFeedbackDisposition::kGenerationMismatch);
    assert(tracker.submit(frame(FrameId{603}, SurfaceGeneration{400}, MetricsGeneration{500},
                                metrics(1700U, 1200U, 2.0F))) ==
           PresentFeedbackDisposition::kMetricsMismatch);
    assert(!tracker.find(FrameId{600}).has_value());
    assert(!tracker.find(FrameId{601}).has_value());
    assert(!tracker.find(FrameId{602}).has_value());
    assert(!tracker.find(FrameId{603}).has_value());
}

} // namespace

int main() {
    independentGenerationAxesAndLossAreAtomic();
    submittedAndPresentedRemainDistinct();
    staleGenerationAIsRejectedAndFreshBAlonePresents();
    completeFeedbackNegativesAreNonMutatingAndDuplicatesIdempotent();
    mismatchedSubmissionCannotEnterTracker();
    return 0;
}
