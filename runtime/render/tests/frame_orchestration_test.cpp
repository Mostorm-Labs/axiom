#include "canvas/render/frame_plan.hpp"
#include "canvas/render/render_backend.hpp"

#include "canvas/scene/scene_types.hpp"

#include <cassert>
#include <cstdint>
#include <optional>
#include <string>
#include <type_traits>
#include <utility>
#include <vector>

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
using canvas::render::ReferenceCommand;
using canvas::render::ReferenceTraversalDiagnostics;
using canvas::render::ReferenceTraversalEntry;
using canvas::render::ImageReferenceCommand;
using canvas::render::VectorPathReferenceCommand;
using canvas::render::RichTextReferenceCommand;
using canvas::render::VectorStrokeReferenceCommand;
using canvas::render::DabStrokeReferenceCommand;
using canvas::render::ConnectorReferenceCommand;
using canvas::render::StickyReferenceCommand;
using canvas::render::GroupReferenceCommand;
using canvas::semantic::ImageContent;
using canvas::semantic::VectorPathContent;
using canvas::semantic::RichTextContent;
using canvas::semantic::VectorStrokeContent;
using canvas::semantic::DabStrokeContent;
using canvas::semantic::ConnectorContent;
using canvas::semantic::StickyContent;
using canvas::semantic::GroupContent;
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

RuntimeSceneRecord distinguishableRecord(std::uint64_t id, ObjectKind kind,
                                          std::uint64_t index) {
    RuntimeSceneRecord record;
    record.objectId = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kindVersion = 1;
    record.placement.order_key = canvas::semantic::OrderKey(
        std::vector<std::uint8_t>{static_cast<std::uint8_t>(index + 1U), 1U});
    record.transform.tx = static_cast<double>(id);
    record.properties.entries = {{0x10U,
                                  canvas::semantic::PropertyValue{static_cast<float>(id)}}};
    record.eraseMasks = {{ObjectId::fromUint64(9000U + id),
                          canvas::semantic::FilledPathMask{}}};
    record.geometryBounds = WorldRect{static_cast<float>(1U + index * 13U),
                                      static_cast<float>(2U + index * 7U),
                                      static_cast<float>(11U + index * 13U),
                                      static_cast<float>(12U + index * 7U)};
    record.visualBounds = WorldRect{static_cast<float>(index * 13U),
                                    static_cast<float>(1U + index * 7U),
                                    static_cast<float>(12U + index * 13U),
                                    static_cast<float>(13U + index * 7U)};
    record.worldBounds = WorldRect{static_cast<float>(4U + index * 13U),
                                   static_cast<float>(5U + index * 7U),
                                   static_cast<float>(16U + index * 13U),
                                   static_cast<float>(17U + index * 7U)};
    record.referenceGeometryDigest = "g3-04-distinguishable-" + std::to_string(index);
    record.directDependencies = {ObjectId::fromUint64(7000U + index),
                                 ObjectId::fromUint64(8000U + index)};
    switch (kind) {
    case ObjectKind::kShape:
        record.content = canvas::semantic::ShapeContent{73U, 10.0 + index, 20.0 + index};
        break;
    case ObjectKind::kImage: record.content = ImageContent{}; break;
    case ObjectKind::kVectorPath: record.content = VectorPathContent{}; break;
    case ObjectKind::kRichText: record.content = RichTextContent{}; break;
    case ObjectKind::kVectorStroke: record.content = VectorStrokeContent{}; break;
    case ObjectKind::kDabStroke: record.content = DabStrokeContent{}; break;
    case ObjectKind::kConnector: record.content = ConnectorContent{}; break;
    case ObjectKind::kSticky: record.content = StickyContent{3.0 + index, 4.0 + index}; break;
    case ObjectKind::kGroup: record.content = GroupContent{}; break;
    }
    return record;
}

ReferenceDrawList distinguishableDrawList(const FrameState& frame) {
    const ObjectKind kinds[] = {ObjectKind::kShape, ObjectKind::kImage,
                                ObjectKind::kVectorPath, ObjectKind::kRichText,
                                ObjectKind::kVectorStroke, ObjectKind::kDabStroke,
                                ObjectKind::kConnector, ObjectKind::kSticky,
                                ObjectKind::kGroup};
    std::vector<ReferenceTraversalEntry> entries;
    entries.reserve(9U);
    for (std::uint64_t index = 0; index < 9U; ++index) {
        const ObjectKind kind = kinds[index];
        RuntimeSceneRecord record = distinguishableRecord(71U + index, kind, index);
        ReferenceCommand command;
        switch (kind) {
        case ObjectKind::kShape:
            command = ShapeReferenceCommand{std::get<canvas::semantic::ShapeContent>(record.content)};
            break;
        case ObjectKind::kImage: command = ImageReferenceCommand{std::get<ImageContent>(record.content)}; break;
        case ObjectKind::kVectorPath: command = VectorPathReferenceCommand{std::get<VectorPathContent>(record.content)}; break;
        case ObjectKind::kRichText: command = RichTextReferenceCommand{std::get<RichTextContent>(record.content)}; break;
        case ObjectKind::kVectorStroke: command = VectorStrokeReferenceCommand{std::get<VectorStrokeContent>(record.content)}; break;
        case ObjectKind::kDabStroke: command = DabStrokeReferenceCommand{std::get<DabStrokeContent>(record.content)}; break;
        case ObjectKind::kConnector: command = ConnectorReferenceCommand{std::get<ConnectorContent>(record.content)}; break;
        case ObjectKind::kSticky: command = StickyReferenceCommand{std::get<StickyContent>(record.content)}; break;
        case ObjectKind::kGroup: command = GroupReferenceCommand{}; break;
        }
        entries.push_back(ReferenceTraversalEntry{.record = std::move(record),
                                                   .contributesPixels = kind != ObjectKind::kGroup,
                                                   .command = std::move(command)});
    }
    return ReferenceDrawList{
        .frame = frame,
        .queryWorldRect = WorldRect{-190.0F, -90.0F, 590.0F, 490.0F},
        .candidatesExamined = 13,
        .visibleRecords = 9,
        .viewportClip = frame.worldViewport,
        .entries = std::move(entries),
        .diagnostics = ReferenceTraversalDiagnostics{
            .visibleIdsProcessed = 9,
            .runtimeSceneFindLookups = 9,
        },
        .canonicalBytes = {0x47U, 0x54U, 0x2dU, 0x47U, 0x33U, 0x2dU, 0x30U, 0x34U},
        .digest = "fnv1a64:g3-04-distinguishable",
    };
}

void assertCompleteNineKindPlan(const FramePlan& plan) {
    const ReferenceDrawList& list = plan.referenceDrawList;
    const ObjectKind expectedKinds[] = {ObjectKind::kShape, ObjectKind::kImage,
                                        ObjectKind::kVectorPath, ObjectKind::kRichText,
                                        ObjectKind::kVectorStroke, ObjectKind::kDabStroke,
                                        ObjectKind::kConnector, ObjectKind::kSticky,
                                        ObjectKind::kGroup};
    assert(list.entries.size() == 9U);
    assert(list.visibleRecords == 9U);
    assert(list.diagnostics.visibleIdsProcessed == 9U);
    assert(list.diagnostics.runtimeSceneFindLookups == 9U);
    assert(list.canonicalBytes.size() == 8U);
    assert(list.digest == "fnv1a64:g3-04-distinguishable");
    for (std::size_t index = 0; index < 9U; ++index) {
        const ReferenceTraversalEntry& entry = list.entries[index];
        assert(entry.record.objectId == ObjectId::fromUint64(71U + index));
        assert(entry.record.kind == expectedKinds[index]);
        assert(entry.record.kindVersion == 1U);
        assert(entry.record.placement.order_key == canvas::semantic::OrderKey(
            std::vector<std::uint8_t>{static_cast<std::uint8_t>(index + 1U), 1U}));
        assert(entry.record.transform.tx == static_cast<double>(71U + index));
        assert(entry.record.properties.entries.size() == 1U);
        assert(entry.record.properties.entries.front().field_id == 0x10U);
        assert(std::get<float>(entry.record.properties.entries.front().value) ==
               static_cast<float>(71U + index));
        assert(entry.record.content.index() == index);
        assert(entry.record.eraseMasks.size() == 1U);
        assert(entry.record.eraseMasks.front().id == ObjectId::fromUint64(9071U + index));
        assert(entry.record.geometryBounds.left == static_cast<float>(1U + index * 13U));
        assert(entry.record.visualBounds.top == static_cast<float>(1U + index * 7U));
        assert(entry.record.worldBounds.right == static_cast<float>(16U + index * 13U));
        assert(entry.record.referenceGeometryDigest ==
               "g3-04-distinguishable-" + std::to_string(index));
        assert(entry.record.directDependencies.size() == 2U);
        assert(entry.record.directDependencies[0] == ObjectId::fromUint64(7000U + index));
        assert(entry.record.directDependencies[1] == ObjectId::fromUint64(8000U + index));
        assert(entry.contributesPixels == (expectedKinds[index] != ObjectKind::kGroup));
        assert(entry.command.index() == index);
        switch (expectedKinds[index]) {
        case ObjectKind::kShape:
            assert(std::get<ShapeReferenceCommand>(entry.command).content ==
                   std::get<canvas::semantic::ShapeContent>(entry.record.content));
            break;
        case ObjectKind::kImage:
            assert(std::get<ImageReferenceCommand>(entry.command).content ==
                   std::get<ImageContent>(entry.record.content));
            break;
        case ObjectKind::kVectorPath:
            assert(std::get<VectorPathReferenceCommand>(entry.command).content ==
                   std::get<VectorPathContent>(entry.record.content));
            break;
        case ObjectKind::kRichText:
            assert(std::get<RichTextReferenceCommand>(entry.command).content ==
                   std::get<RichTextContent>(entry.record.content));
            break;
        case ObjectKind::kVectorStroke:
            assert(std::get<VectorStrokeReferenceCommand>(entry.command).content ==
                   std::get<VectorStrokeContent>(entry.record.content));
            break;
        case ObjectKind::kDabStroke:
            assert(std::get<DabStrokeReferenceCommand>(entry.command).content ==
                   std::get<DabStrokeContent>(entry.record.content));
            break;
        case ObjectKind::kConnector:
            assert(std::get<ConnectorReferenceCommand>(entry.command).content ==
                   std::get<ConnectorContent>(entry.record.content));
            break;
        case ObjectKind::kSticky:
            assert(std::get<StickyReferenceCommand>(entry.command).content ==
                   std::get<StickyContent>(entry.record.content));
            break;
        case ObjectKind::kGroup:
            assert(std::holds_alternative<GroupReferenceCommand>(entry.command));
            break;
        }
    }
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
    assertCompleteNineKindPlan(expected);
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
    assertCompleteNineKindPlan(*backend.observedPlan);
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
    assertCompleteNineKindPlan(*backend.observedPlan);
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
