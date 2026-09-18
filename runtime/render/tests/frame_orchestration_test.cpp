#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"

#include "canvas/scene/scene_types.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <type_traits>
#include <utility>

namespace {

using canvas::RuntimeSceneRecord;
using canvas::foundation::ObjectId;
using canvas::foundation::SceneRevision;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::render::BackendSubmissionCode;
using canvas::render::BackendSubmissionResult;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::FrameId;
using canvas::render::FrameOrchestrator;
using canvas::render::FramePlan;
using canvas::render::FramePlanBuilder;
using canvas::render::FrameState;
using canvas::render::IRenderBackend;
using canvas::render::MetricsGeneration;
using canvas::render::ReferenceDrawList;
using canvas::render::ReferenceTraversalDiagnostics;
using canvas::render::ReferenceTraversalEntry;
using canvas::render::ShapeReferenceCommand;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceMetrics;
using canvas::render::ViewId;
using canvas::semantic::ObjectKind;
using canvas::semantic::SemanticGeneration;

FrameState distinguishableFrame(FrameId frameId = FrameId{67}) {
    return FrameState{
        .viewId = ViewId{41},
        .camera = CameraState{WorldPoint{17.0F, -23.0F}, 1.75F, 0.25F,
                              CameraGeneration{43}},
        .worldViewport = WorldRect{-200.0F, -100.0F, 600.0F, 500.0F},
        .metrics = SurfaceMetrics{800.0F, 600.0F, 1600U, 1200U, 2.0F, 1.25F},
        .sceneGeneration = SemanticGeneration{47},
        .sceneReadToken = SceneRevision{53},
        .surfaceGeneration = SurfaceGeneration{59},
        .metricsGeneration = MetricsGeneration{61},
        .frameId = frameId,
    };
}

ReferenceDrawList distinguishableDrawList(const FrameState& frame) {
    RuntimeSceneRecord record;
    record.objectId = ObjectId::fromUint64(71);
    record.kind = ObjectKind::kShape;
    record.kindVersion = 1;
    record.geometryBounds = WorldRect{1.0F, 2.0F, 11.0F, 12.0F};
    record.visualBounds = WorldRect{0.0F, 1.0F, 12.0F, 13.0F};
    record.worldBounds = WorldRect{4.0F, 5.0F, 16.0F, 17.0F};
    record.referenceGeometryDigest = "g3-04-distinguishable-shape";
    return ReferenceDrawList{
        .frame = frame,
        .queryWorldRect = WorldRect{-190.0F, -90.0F, 590.0F, 490.0F},
        .candidatesExamined = 13,
        .visibleRecords = 1,
        .viewportClip = frame.worldViewport,
        .entries = {ReferenceTraversalEntry{
            .record = record,
            .contributesPixels = true,
            .command = ShapeReferenceCommand{canvas::semantic::ShapeContent{73U, 10.0, 20.0}},
        }},
        .diagnostics = ReferenceTraversalDiagnostics{
            .visibleIdsProcessed = 1,
            .runtimeSceneFindLookups = 1,
        },
        .canonicalBytes = {0x47U, 0x54U, 0x2dU, 0x47U, 0x33U, 0x2dU, 0x30U, 0x34U},
        .digest = "fnv1a64:g3-04-distinguishable",
    };
}

class FakeBackend final : public IRenderBackend {
  public:
    explicit FakeBackend(BackendSubmissionResult next) : next_(std::move(next)) {}

    [[nodiscard]] BackendSubmissionResult submit(const FramePlan& plan) override {
        ++callCount;
        observedPlan.emplace(plan);
        return next_;
    }

    std::uint32_t callCount = 0;
    std::optional<FramePlan> observedPlan;

  private:
    BackendSubmissionResult next_;
};

void acceptedSubmissionPreservesExactImmutablePlan() {
    const FrameState frame = distinguishableFrame();
    const ReferenceDrawList drawList = distinguishableDrawList(frame);
    const FramePlan expected{.frame = frame, .referenceDrawList = drawList};
    const FrameState frameBefore = frame;
    const ReferenceDrawList drawListBefore = drawList;
    canvas::RuntimeScene runtimeScene;
    const auto runtimeGenerationBefore = runtimeScene.generation();
    const std::vector<RuntimeSceneRecord> runtimeRecordsBefore{
        runtimeScene.records().begin(), runtimeScene.records().end()};

    const auto plan = FramePlanBuilder::build(frame, drawList);
    assert(plan.hasValue());
    assert(plan.value() == expected);

    FakeBackend backend{BackendSubmissionResult::accepted()};
    const BackendSubmissionResult result = FrameOrchestrator::submit(backend, plan.value());

    assert(result.code == BackendSubmissionCode::kAccepted);
    assert(result.message.empty());
    assert(backend.callCount == 1U);
    assert(backend.observedPlan.has_value());
    assert(*backend.observedPlan == expected);
    assert(frame == frameBefore);
    assert(drawList == drawListBefore);
    assert(runtimeScene.generation() == runtimeGenerationBefore);
    assert(std::vector<RuntimeSceneRecord>(runtimeScene.records().begin(),
                                           runtimeScene.records().end()) ==
           runtimeRecordsBefore);
}

void rejectionIsTypedDeterministicAndPublishesNothing() {
    const FrameState frame = distinguishableFrame();
    const ReferenceDrawList drawList = distinguishableDrawList(frame);
    const auto plan = FramePlanBuilder::build(frame, drawList);
    assert(plan.hasValue());
    const FramePlan before = plan.value();
    canvas::RuntimeScene runtimeScene;
    const auto runtimeGenerationBefore = runtimeScene.generation();
    const std::vector<RuntimeSceneRecord> runtimeRecordsBefore{
        runtimeScene.records().begin(), runtimeScene.records().end()};

    FakeBackend backend{BackendSubmissionResult::rejected("fixture backend rejection")};
    const BackendSubmissionResult result = FrameOrchestrator::submit(backend, plan.value());

    assert(result.code == BackendSubmissionCode::kRejected);
    assert(result.message == "fixture backend rejection");
    assert(backend.callCount == 1U);
    assert(backend.observedPlan.has_value());
    assert(*backend.observedPlan == before);
    assert(plan.value() == before);
    assert(runtimeScene.generation() == runtimeGenerationBefore);
    assert(std::vector<RuntimeSceneRecord>(runtimeScene.records().begin(),
                                           runtimeScene.records().end()) ==
           runtimeRecordsBefore);
}

void inconsistentFrameIdentityIsRejectedBeforeBackendSubmission() {
    const FrameState frame = distinguishableFrame();
    const ReferenceDrawList drawList = distinguishableDrawList(distinguishableFrame(FrameId{101}));

    const auto plan = FramePlanBuilder::build(frame, drawList);
    assert(!plan.hasValue());
    assert(plan.error().code == canvas::foundation::ErrorCode::kInvalidArgument);
}

} // namespace

int main() {
    static_assert(std::is_same_v<decltype(std::declval<IRenderBackend&>().submit(
                                     std::declval<const FramePlan&>())),
                                 BackendSubmissionResult>);
    acceptedSubmissionPreservesExactImmutablePlan();
    rejectionIsTypedDeterministicAndPublishesNothing();
    inconsistentFrameIdentityIsRejectedBeforeBackendSubmission();
    return 0;
}
