#include "canvas/render/skia_headless_backend.hpp"

#include "canvas/foundation/object_id.hpp"
#include "canvas/render/frame_plan.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/property_value.hpp"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstdint>
#include <limits>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

using canvas::RuntimeSceneRecord;
using canvas::foundation::ObjectId;
using canvas::foundation::SceneRevision;
using canvas::foundation::WorldPoint;
using canvas::foundation::WorldRect;
using canvas::render::BackendSubmissionCode;
using canvas::render::CameraGeneration;
using canvas::render::CameraState;
using canvas::render::ConnectorReferenceCommand;
using canvas::render::DabStrokeReferenceCommand;
using canvas::render::FrameId;
using canvas::render::FramePlan;
using canvas::render::FrameState;
using canvas::render::GroupReferenceCommand;
using canvas::render::HeadlessRasterConfig;
using canvas::render::HeadlessRasterObservation;
using canvas::render::HeadlessSubmissionIssue;
using canvas::render::ImageReferenceCommand;
using canvas::render::MetricsGeneration;
using canvas::render::ReferenceCommand;
using canvas::render::ReferenceDrawList;
using canvas::render::ReferenceTraversalDiagnostics;
using canvas::render::ReferenceTraversalEntry;
using canvas::render::RichTextReferenceCommand;
using canvas::render::ShapeReferenceCommand;
using canvas::render::SkiaHeadlessBackend;
using canvas::render::StickyReferenceCommand;
using canvas::render::SurfaceGeneration;
using canvas::render::SurfaceMetrics;
using canvas::render::VectorPathReferenceCommand;
using canvas::render::VectorStrokeReferenceCommand;
using canvas::render::ViewId;
using canvas::render::WorldToViewAffine;
using canvas::semantic::ColorValue;
using canvas::semantic::ObjectKind;
using canvas::semantic::SemanticGeneration;

constexpr std::uint32_t kWidth = 256U;
constexpr std::uint32_t kHeight = 256U;
using Rgba = std::array<std::uint8_t, 4>;

FrameState fixtureFrame(FrameId id = FrameId{901}) {
    return FrameState{
        .viewId = ViewId{801},
        .camera = CameraState{WorldPoint{0.0F, 0.0F}, 1.0F, 0.0F,
                              CameraGeneration{802}},
        .worldViewport = WorldRect{0.0F, 0.0F, 256.0F, 256.0F},
        .metrics = SurfaceMetrics{256.0F, 256.0F, kWidth, kHeight, 1.0F, 1.0F},
        .sceneGeneration = SemanticGeneration{803},
        .sceneReadToken = SceneRevision{804},
        .surfaceGeneration = SurfaceGeneration{805},
        .metricsGeneration = MetricsGeneration{806},
        .frameId = id,
    };
}

canvas::semantic::PropertyBag solidFill(ColorValue color) {
    return canvas::semantic::PropertyBag{{canvas::semantic::PropertyEntry{
        0x00000100U,
        canvas::semantic::FillStyleValue{canvas::semantic::SolidFill{color}}}}};
}

canvas::semantic::PropertyBag solidStroke(ColorValue color, double width) {
    return canvas::semantic::PropertyBag{{canvas::semantic::PropertyEntry{
        0x00000101U,
        canvas::semantic::StrokeStyleValue{canvas::semantic::SolidStroke{
            color,
            width,
            canvas::semantic::StrokeCap::kButt,
            canvas::semantic::MiterJoin{4.0},
            canvas::semantic::SolidDash{}}}}}};
}

canvas::semantic::VectorPathGeometry rectanglePath(double x, double y, double width,
                                                    double height) {
    return canvas::semantic::VectorPathGeometry{
        .fill_rule = canvas::semantic::FillRule::kNonZero,
        .commands = {
            canvas::semantic::MoveTo{{x, y}},
            canvas::semantic::LineTo{{x + width, y}},
            canvas::semantic::LineTo{{x + width, y + height}},
            canvas::semantic::LineTo{{x, y + height}},
            canvas::semantic::ClosePath{},
        },
    };
}

RuntimeSceneRecord baseRecord(std::uint64_t id, ObjectKind kind, WorldRect bounds) {
    RuntimeSceneRecord record;
    record.objectId = ObjectId::fromUint64(id);
    record.kind = kind;
    record.kindVersion = 1;
    record.placement.order_key = canvas::semantic::OrderKey(
        std::vector<std::uint8_t>{static_cast<std::uint8_t>(id - 500U)});
    record.visualBounds = bounds;
    record.worldBounds = bounds;
    record.geometryBounds = bounds;
    record.referenceGeometryDigest = "g3-05-fixture-" + std::to_string(id);
    return record;
}

ReferenceTraversalEntry shapeEntry() {
    RuntimeSceneRecord record = baseRecord(501U, ObjectKind::kShape,
                                           WorldRect{8.0F, 8.0F, 32.0F, 32.0F});
    record.properties = solidFill(ColorValue{1.0F, 0.0F, 0.0F, 1.0F});
    const canvas::semantic::ShapeContent content{1U, 24.0, 24.0};
    record.content = content;
    record.eraseMasks = {{
        ObjectId::fromUint64(1501U),
        canvas::semantic::FilledPathMask{rectanglePath(16.0, 16.0, 8.0, 8.0)},
    }};
    return {.record = std::move(record), .command = ShapeReferenceCommand{content}};
}

ReferenceTraversalEntry imageEntry() {
    RuntimeSceneRecord record = baseRecord(502U, ObjectKind::kImage,
                                           WorldRect{40.0F, 8.0F, 64.0F, 32.0F});
    canvas::semantic::ImageContent content;
    content.resource_id.value = ObjectId::fromUint64(130U);
    content.intrinsic_width = 24.0;
    content.intrinsic_height = 24.0;
    content.content_mode = canvas::semantic::ImageContentMode::kStretch;
    content.width = 24.0;
    content.height = 24.0;
    record.content = content;
    return {.record = std::move(record), .command = ImageReferenceCommand{content}};
}

ReferenceTraversalEntry vectorPathEntry() {
    RuntimeSceneRecord record = baseRecord(503U, ObjectKind::kVectorPath,
                                           WorldRect{72.0F, 8.0F, 96.0F, 32.0F});
    record.properties = solidFill(ColorValue{0.0F, 0.0F, 1.0F, 1.0F});
    const canvas::semantic::VectorPathContent content{rectanglePath(72.0, 8.0, 24.0, 24.0)};
    record.content = content;
    return {.record = std::move(record), .command = VectorPathReferenceCommand{content}};
}

ReferenceTraversalEntry richTextEntry() {
    RuntimeSceneRecord record = baseRecord(504U, ObjectKind::kRichText,
                                           WorldRect{104.0F, 8.0F, 128.0F, 32.0F});
    canvas::semantic::TextStyle style;
    style.font_size = 12.0;
    style.weight = 400U;
    style.color = ColorValue{1.0F, 1.0F, 0.0F, 1.0F};
    canvas::semantic::Paragraph paragraph;
    paragraph.id = ObjectId::fromUint64(2504U);
    paragraph.style.alignment = canvas::semantic::ParagraphAlignment::kLeft;
    paragraph.style.line_height = 12.0;
    paragraph.runs = {{"A9", style}};
    canvas::semantic::RichTextContent content{{{paragraph}}};
    record.content = content;
    return {.record = std::move(record), .command = RichTextReferenceCommand{content}};
}

canvas::semantic::BrushDescriptor brush(ColorValue color, double size) {
    canvas::semantic::BrushDescriptor result;
    result.brush_family_id = 1U;
    result.brush_version = 1U;
    result.color = color;
    result.nominal_size = size;
    result.opacity = 1.0F;
    return result;
}

ReferenceTraversalEntry vectorStrokeEntry() {
    RuntimeSceneRecord record = baseRecord(505U, ObjectKind::kVectorStroke,
                                           WorldRect{8.0F, 46.0F, 32.0F, 50.0F});
    canvas::semantic::StrokeRecord stroke;
    stroke.brush = brush(ColorValue{1.0F, 0.0F, 1.0F, 1.0F}, 4.0);
    stroke.data = canvas::semantic::VectorStrokeData{{
        canvas::semantic::StrokeSample{{8.0, 48.0}, 1.0F, {}},
        canvas::semantic::StrokeSample{{32.0, 48.0}, 1.0F, {}},
    }};
    const canvas::semantic::VectorStrokeContent content{stroke};
    record.content = content;
    return {.record = std::move(record), .command = VectorStrokeReferenceCommand{content}};
}

ReferenceTraversalEntry dabStrokeEntry() {
    RuntimeSceneRecord record = baseRecord(506U, ObjectKind::kDabStroke,
                                           WorldRect{46.0F, 42.0F, 58.0F, 54.0F});
    canvas::semantic::StrokeRecord stroke;
    stroke.brush = brush(ColorValue{0.0F, 1.0F, 1.0F, 1.0F}, 12.0);
    stroke.data = canvas::semantic::DabStrokeData{{
        canvas::semantic::DabInstance{{52.0, 48.0}, 12.0, 0.0F, 1.0F},
    }};
    const canvas::semantic::DabStrokeContent content{stroke};
    record.content = content;
    return {.record = std::move(record), .command = DabStrokeReferenceCommand{content}};
}

ReferenceTraversalEntry connectorEntry() {
    RuntimeSceneRecord record = baseRecord(507U, ObjectKind::kConnector,
                                           WorldRect{72.0F, 46.0F, 96.0F, 50.0F});
    record.properties = solidStroke(ColorValue{1.0F, 128.0F / 255.0F, 0.0F, 1.0F}, 4.0);
    canvas::semantic::ConnectorContent content;
    content.start.value = canvas::semantic::FreePointEndpoint{{72.0, 48.0}};
    content.end.value = canvas::semantic::FreePointEndpoint{{96.0, 48.0}};
    content.routing = canvas::semantic::ConnectorRouting::kStraight;
    record.content = content;
    return {.record = std::move(record), .command = ConnectorReferenceCommand{content}};
}

ReferenceTraversalEntry stickyEntry() {
    RuntimeSceneRecord record = baseRecord(508U, ObjectKind::kSticky,
                                           WorldRect{104.0F, 40.0F, 128.0F, 60.0F});
    const canvas::semantic::StickyContent content{24.0, 20.0};
    record.content = content;
    return {.record = std::move(record), .command = StickyReferenceCommand{content}};
}

ReferenceTraversalEntry groupEntry() {
    RuntimeSceneRecord record = baseRecord(509U, ObjectKind::kGroup,
                                           WorldRect{160.0F, 160.0F, 184.0F, 184.0F});
    record.content = canvas::semantic::GroupContent{};
    return {.record = std::move(record),
            .contributesPixels = false,
            .command = GroupReferenceCommand{}};
}

ReferenceDrawList fixtureDrawList(const FrameState& frame, bool includeGroup = true) {
    std::vector<ReferenceTraversalEntry> entries{
        shapeEntry(), imageEntry(), vectorPathEntry(), richTextEntry(), vectorStrokeEntry(),
        dabStrokeEntry(), connectorEntry(), stickyEntry(),
    };
    if (includeGroup) entries.push_back(groupEntry());
    return ReferenceDrawList{
        .frame = frame,
        .queryWorldRect = frame.worldViewport,
        .candidatesExamined = entries.size(),
        .visibleRecords = entries.size(),
        .worldToView = WorldToViewAffine{},
        .viewportClip = frame.worldViewport,
        .entries = std::move(entries),
        .diagnostics = ReferenceTraversalDiagnostics{
            .visibleIdsProcessed = includeGroup ? 9U : 8U,
            .runtimeSceneFindLookups = includeGroup ? 9U : 8U,
        },
        .canonicalBytes = {0x47U, 0x33U, 0x2dU, 0x30U, 0x35U},
        .digest = "fnv1a64:g3-05-fixture",
    };
}

std::size_t offset(std::uint32_t x, std::uint32_t y) {
    return (static_cast<std::size_t>(y) * kWidth + x) * 4U;
}

void fill(std::vector<std::uint8_t>& pixels, std::uint32_t x, std::uint32_t y,
          std::uint32_t width, std::uint32_t height, Rgba color) {
    for (std::uint32_t row = y; row < y + height; ++row) {
        for (std::uint32_t column = x; column < x + width; ++column) {
            std::copy(color.begin(), color.end(), pixels.begin() +
                      static_cast<std::vector<std::uint8_t>::difference_type>(offset(column, row)));
        }
    }
}

std::vector<std::uint8_t> independentExpectedPixels() {
    std::vector<std::uint8_t> pixels(kWidth * kHeight * 4U, 0U);
    fill(pixels, 8U, 8U, 24U, 24U, {255U, 0U, 0U, 255U});
    fill(pixels, 16U, 16U, 8U, 8U, {0U, 0U, 0U, 0U});
    for (std::uint32_t y = 8U; y < 32U; y += 4U) {
        for (std::uint32_t x = 40U; x < 64U; x += 4U) {
            const bool bright = (((x - 40U) / 4U) + ((y - 8U) / 4U) + 130U) % 2U == 0U;
            fill(pixels, x, y, 4U, 4U,
                 bright ? Rgba{0U, 255U, 0U, 255U} : Rgba{0U, 128U, 0U, 255U});
        }
    }
    fill(pixels, 72U, 8U, 24U, 24U, {0U, 0U, 255U, 255U});
    fill(pixels, 104U, 8U, 8U, 12U, {255U, 255U, 0U, 255U});
    fill(pixels, 114U, 8U, 8U, 12U, {255U, 255U, 0U, 255U});
    fill(pixels, 8U, 46U, 24U, 4U, {255U, 0U, 255U, 255U});
    fill(pixels, 46U, 42U, 12U, 12U, {0U, 255U, 255U, 255U});
    fill(pixels, 72U, 46U, 24U, 4U, {255U, 128U, 0U, 255U});
    fill(pixels, 104U, 40U, 24U, 20U, {255U, 255U, 128U, 255U});
    return pixels;
}

std::string independentDigest(const std::vector<std::uint8_t>& bytes) {
    std::uint64_t value = 0xcbf29ce484222325ULL;
    for (std::uint8_t byte : bytes) {
        value ^= byte;
        value *= 0x100000001b3ULL;
    }
    constexpr char digits[] = "0123456789abcdef";
    std::string result = "fnv1a64:";
    for (int shift = 60; shift >= 0; shift -= 4) {
        result.push_back(digits[(value >> shift) & 0x0fU]);
    }
    return result;
}

Rgba pixel(const HeadlessRasterObservation& observation, std::uint32_t x,
           std::uint32_t y) {
    const auto start = observation.rgba.begin() +
        static_cast<std::vector<std::uint8_t>::difference_type>(offset(x, y));
    return {start[0], start[1], start[2], start[3]};
}

void exactNineKindGoldenIsIndependentAndDeterministic() {
    const FrameState frame = fixtureFrame();
    const ReferenceDrawList list = fixtureDrawList(frame);
    const FramePlan plan{frame, list};
    const auto expected = independentExpectedPixels();
    const std::string expectedDigest = independentDigest(expected);

    SkiaHeadlessBackend backend(HeadlessRasterConfig{kWidth, kHeight});
    assert(backend.submit(plan).code == BackendSubmissionCode::kAccepted);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kNone);
    assert(backend.observation().has_value());
    const HeadlessRasterObservation first = *backend.observation();
    assert(first.width == kWidth);
    assert(first.height == kHeight);
    assert(first.rgba == expected);
    assert(first.digest == expectedDigest);
    assert(first.sourcePlanDigest == "fnv1a64:g3-05-fixture");
    assert(first.traversedKinds == std::vector<ObjectKind>({
        ObjectKind::kShape, ObjectKind::kImage, ObjectKind::kVectorPath,
        ObjectKind::kRichText, ObjectKind::kVectorStroke, ObjectKind::kDabStroke,
        ObjectKind::kConnector, ObjectKind::kSticky, ObjectKind::kGroup,
    }));
    assert((plan == FramePlan{frame, list}));

    assert(pixel(first, 8U, 8U) == (Rgba{255U, 0U, 0U, 255U}));
    assert(pixel(first, 16U, 16U) == (Rgba{0U, 0U, 0U, 0U}));
    assert(pixel(first, 40U, 8U) == (Rgba{0U, 255U, 0U, 255U}));
    assert(pixel(first, 72U, 8U) == (Rgba{0U, 0U, 255U, 255U}));
    assert(pixel(first, 104U, 8U) == (Rgba{255U, 255U, 0U, 255U}));
    assert(pixel(first, 8U, 46U) == (Rgba{255U, 0U, 255U, 255U}));
    assert(pixel(first, 46U, 42U) == (Rgba{0U, 255U, 255U, 255U}));
    assert(pixel(first, 72U, 46U) == (Rgba{255U, 128U, 0U, 255U}));
    assert(pixel(first, 104U, 40U) == (Rgba{255U, 255U, 128U, 255U}));

    assert(backend.submit(plan).code == BackendSubmissionCode::kAccepted);
    assert(*backend.observation() == first);
    assert((plan == FramePlan{frame, list}));

    const ReferenceDrawList noGroupList = fixtureDrawList(frame, false);
    const FramePlan noGroupPlan{frame, noGroupList};
    SkiaHeadlessBackend noGroupBackend(HeadlessRasterConfig{kWidth, kHeight});
    assert(noGroupBackend.submit(noGroupPlan).code == BackendSubmissionCode::kAccepted);
    assert(noGroupBackend.observation()->rgba == first.rgba);
}

void transformClipAndEraseAreObservable() {
    const FrameState frame = fixtureFrame(FrameId{911});
    ReferenceDrawList list = fixtureDrawList(frame, false);
    list.entries = {shapeEntry()};
    list.visibleRecords = 1U;
    list.worldToView = WorldToViewAffine{2.0, 0.0, 0.0, 1.0, -8.0, 0.0};
    list.entries[0].record.transform.tx = 3.0;
    list.viewportClip = WorldRect{11.0F, 8.0F, 40.0F, 20.0F};
    list.entries[0].record.eraseMasks.clear();
    const FramePlan plan{frame, list};

    SkiaHeadlessBackend backend(HeadlessRasterConfig{kWidth, kHeight});
    assert(backend.submit(plan).code == BackendSubmissionCode::kAccepted);
    const auto& observation = *backend.observation();
    // worldToView first maps x=8 to 8, then record transform moves it to x=11.
    assert(pixel(observation, 10U, 8U) == (Rgba{0U, 0U, 0U, 0U}));
    assert(pixel(observation, 11U, 8U) == (Rgba{255U, 0U, 0U, 255U}));
    assert(pixel(observation, 39U, 19U) == (Rgba{255U, 0U, 0U, 255U}));
    assert(pixel(observation, 40U, 19U) == (Rgba{0U, 0U, 0U, 0U}));
    assert(pixel(observation, 11U, 20U) == (Rgba{0U, 0U, 0U, 0U}));
}

void orthogonalConnectorUsesDeterministicElbow() {
    const FrameState frame = fixtureFrame(FrameId{916});
    ReferenceTraversalEntry entry = connectorEntry();
    auto command = std::get<ConnectorReferenceCommand>(entry.command);
    command.content.end.value = canvas::semantic::FreePointEndpoint{{96.0, 64.0}};
    command.content.routing = canvas::semantic::ConnectorRouting::kOrthogonal;
    entry.record.content = command.content;
    entry.command = command;
    entry.record.visualBounds = WorldRect{72.0F, 46.0F, 98.0F, 66.0F};

    ReferenceDrawList list = fixtureDrawList(frame, false);
    list.entries = {std::move(entry)};
    list.candidatesExamined = 1U;
    list.visibleRecords = 1U;
    list.diagnostics = {.visibleIdsProcessed = 1U, .runtimeSceneFindLookups = 1U};
    const FramePlan plan{frame, list};

    SkiaHeadlessBackend backend(HeadlessRasterConfig{kWidth, kHeight});
    assert(backend.submit(plan).code == BackendSubmissionCode::kAccepted);
    const auto& observation = *backend.observation();
    assert(pixel(observation, 80U, 48U) == (Rgba{255U, 128U, 0U, 255U}));
    assert(pixel(observation, 94U, 56U) == (Rgba{255U, 128U, 0U, 255U}));
    assert(pixel(observation, 80U, 56U) == (Rgba{0U, 0U, 0U, 0U}));
}

void deterministicRejectionsPreservePriorObservation() {
    const FrameState frame = fixtureFrame(FrameId{921});
    const ReferenceDrawList list = fixtureDrawList(frame);
    const FramePlan plan{frame, list};
    const FramePlan planBefore = plan;
    SkiaHeadlessBackend backend(HeadlessRasterConfig{kWidth, kHeight});
    assert(backend.submit(plan).code == BackendSubmissionCode::kAccepted);
    const HeadlessRasterObservation stable = *backend.observation();

    const FrameState otherFrame = fixtureFrame(FrameId{922});
    const FramePlan mismatched{otherFrame, list};
    const auto mismatch = backend.submit(mismatched);
    assert(mismatch.code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kPlanIdentityMismatch);
    assert(plan == planBefore);
    assert((mismatched == FramePlan{otherFrame, list}));
    assert(*backend.observation() == stable);

    ReferenceDrawList unsupportedList = list;
    unsupportedList.entries.front().record.eraseMasks = {{
        ObjectId::fromUint64(1999U), canvas::semantic::SweptCircleMask{},
    }};
    const FramePlan unsupported{frame, unsupportedList};
    const auto unsupportedResult = backend.submit(unsupported);
    assert(unsupportedResult.code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kUnsupportedEraseMask);
    assert(plan == planBefore);
    assert((unsupported == FramePlan{frame, unsupportedList}));
    assert(*backend.observation() == stable);

    ReferenceDrawList fractionalList = list;
    fractionalList.entries[0].record.visualBounds.left = 8.5F;
    const FramePlan fractional{frame, fractionalList};
    assert(backend.submit(fractional).code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kUnsupportedGeometry);
    assert(plan == planBefore);
    assert((fractional == FramePlan{frame, fractionalList}));
    assert(*backend.observation() == stable);

    ReferenceDrawList emptyList = list;
    emptyList.entries.clear();
    emptyList.visibleRecords = 0U;
    emptyList.candidatesExamined = 0U;
    emptyList.diagnostics = {};
    const FramePlan empty{frame, emptyList};
    const FramePlan emptyBefore = empty;
    assert(backend.submit(empty).code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kEmptyPlan);
    assert(empty == emptyBefore);
    assert(*backend.observation() == stable);

    SkiaHeadlessBackend invalid(HeadlessRasterConfig{0U, kHeight});
    assert(invalid.submit(plan).code == BackendSubmissionCode::kRejected);
    assert(invalid.lastIssue() == HeadlessSubmissionIssue::kInvalidConfiguration);
    assert(plan == planBefore);
    assert(!invalid.observation().has_value());

    SkiaHeadlessBackend nonFixed(HeadlessRasterConfig{kWidth / 2U, kHeight});
    assert(nonFixed.submit(plan).code == BackendSubmissionCode::kRejected);
    assert(nonFixed.lastIssue() == HeadlessSubmissionIssue::kInvalidConfiguration);
    assert(plan == planBefore);
    assert(!nonFixed.observation().has_value());

    SkiaHeadlessBackend overflow(HeadlessRasterConfig{
        std::numeric_limits<std::uint32_t>::max(),
        std::numeric_limits<std::uint32_t>::max()});
    assert(overflow.submit(plan).code == BackendSubmissionCode::kRejected);
    assert(overflow.lastIssue() == HeadlessSubmissionIssue::kOverflow);
    assert(plan == planBefore);
    assert(!overflow.observation().has_value());

    ReferenceDrawList coordinateOverflowList = list;
    coordinateOverflowList.entries[0].record.transform.tx =
        std::numeric_limits<double>::max();
    const FramePlan coordinateOverflow{frame, coordinateOverflowList};
    assert(backend.submit(coordinateOverflow).code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kOverflow);
    assert((coordinateOverflow == FramePlan{frame, coordinateOverflowList}));
    assert(*backend.observation() == stable);

    ReferenceDrawList attachedConnectorList = list;
    auto attachedConnector = std::get<ConnectorReferenceCommand>(
        attachedConnectorList.entries[6].command);
    attachedConnector.content.start.value = canvas::semantic::AttachedEndpoint{
        ObjectId::fromUint64(501U), canvas::semantic::AutoPerimeterAnchor{}};
    attachedConnectorList.entries[6].command = attachedConnector;
    const FramePlan attachedConnectorPlan{frame, attachedConnectorList};
    assert(backend.submit(attachedConnectorPlan).code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kUnsupportedGeometry);
    assert((attachedConnectorPlan == FramePlan{frame, attachedConnectorList}));
    assert(*backend.observation() == stable);

    ReferenceDrawList fractionalDabList = list;
    auto fractionalDab = std::get<DabStrokeReferenceCommand>(
        fractionalDabList.entries[5].command);
    auto* dabData = std::get_if<canvas::semantic::DabStrokeData>(
        &fractionalDab.content.stroke.data);
    assert(dabData != nullptr);
    dabData->dabs[0].center.x = 52.5;
    fractionalDabList.entries[5].command = fractionalDab;
    const FramePlan fractionalDabPlan{frame, fractionalDabList};
    assert(backend.submit(fractionalDabPlan).code == BackendSubmissionCode::kRejected);
    assert(backend.lastIssue() == HeadlessSubmissionIssue::kUnsupportedGeometry);
    assert((fractionalDabPlan == FramePlan{frame, fractionalDabList}));
    assert(*backend.observation() == stable);
}

} // namespace

int main() {
    exactNineKindGoldenIsIndependentAndDeterministic();
    transformClipAndEraseAreObservable();
    orthogonalConnectorUsesDeterministicElbow();
    deterministicRejectionsPreservePriorObservation();
    return 0;
}
