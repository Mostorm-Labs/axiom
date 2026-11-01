#include "canvas/runtime/internal/runtime_composition_core.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <array>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <thread>

#include "canvas/runtime/internal/multi_selection_port.hpp"
#include "canvas/semantic/normalizer.hpp"
#if AXIOM_MSE_HAS_DEBUG_UI
#include "canvas/debug_ui/builtin_panels.hpp"
#include "canvas/debug_ui/snapshot_assembler.hpp"
#include "canvas/debug_ui/ui_session_state.hpp"
#include "imgui.h"
#include "imgui_internal.h"
#endif

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
 public:
  static void select(interaction::CanvasEditingRuntime& runtime,
                     std::vector<foundation::ObjectId> ids) {
    ASSERT_TRUE(runtime.selection_.applyAcquisition(
        ids, interaction::SelectionCombineMode::kReplace, runtime.selection_.revision()));
  }
  static bool enterGroupScope(interaction::CanvasEditingRuntime& runtime, foundation::ObjectId id) {
    return runtime.selection_.enterGroupScope(id, runtime.semanticObjects_);
  }
  static std::size_t historySize(const interaction::CanvasEditingRuntime& runtime) {
    return runtime.history_.size();
  }
  static std::optional<semantic::Transform2D> world(
      const interaction::CanvasEditingRuntime& runtime, foundation::ObjectId object) {
    const auto* value = runtime.sceneCoordinator_->runtimeScene().find(object);
    return value ? std::optional<semantic::Transform2D>{value->transform} : std::nullopt;
  }
  static bool publishConnectorRoute(interaction::CanvasEditingRuntime& runtime,
                                    foundation::ObjectId object) {
    const auto* value = runtime.semanticObjects_.find(object);
    if (!value) return false;
    const auto geometry = runtime.previewConnectorGeometry(*value);
    const std::array routes{std::pair{object, geometry}};
    return static_cast<bool>(runtime.sceneCoordinator_->publishConnectorRoutes(routes, runtime.semanticGeneration_.current()));
  }
  static std::optional<scene::ConnectorGeometry> previewConnector(
      interaction::CanvasEditingRuntime& runtime, foundation::ObjectId object) {
    const auto* value = runtime.semanticObjects_.find(object);
    return value ? std::optional<scene::ConnectorGeometry>{runtime.previewConnectorGeometry(*value)} : std::nullopt;
  }
  static std::optional<semantic::Transform2D> preview(
      const interaction::CanvasEditingRuntime& runtime, foundation::ObjectId object) {
    const auto* value = runtime.transformOverrides_.find(object);
    return value ? std::optional<semantic::Transform2D>{*value} : std::nullopt;
  }
};
}

namespace {
using namespace canvas;
using Access = ink_playground::InkPlaygroundHistoryTestAccess;
foundation::ObjectId id(std::uint64_t value) { return foundation::ObjectId::fromUint64(value); }

class MultiSelectionMSE : public testing::Test {
 protected:
  runtime::RuntimeCompositionCore composition;
  void SetUp() override {
    ASSERT_TRUE(composition.open());
    ASSERT_TRUE(composition.bindSurface(800, 600));
    ASSERT_TRUE(composition.multiSelectionPort().setPermissionMode(
        runtime::MultiSelectionPermissionMode::kLocalSingleUser));
  }
  void TearDown() override { EXPECT_TRUE(composition.close()); }

  void preparePartialFillFixture() {
    semantic::InsertObjectsOp insert;
    for (std::uint64_t i = 1; i <= 3; ++i) {
      semantic::ObjectRecord record;
      record.id = id(i);
      record.kind = semantic::ObjectKind::kShape;
      record.kind_version = 1;
      record.content = semantic::ShapeContent{1, 40, 30};
      record.transform.tx = static_cast<double>(i * 60);
      record.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(i)});
      insert.objects.push_back(record);
    }
    semantic::Operation operation{semantic::OperationId{id(100)}, semantic::DocumentId{id(1)},
                                  1, 1, std::move(insert)};
    ASSERT_EQ(composition.applyExternalOperation(operation, semantic::ApplySource::kRemoteSync).disposition,
              semantic::ApplyDisposition::kApplied);
    const std::vector<std::uint8_t> png = {
      137,80,78,71,13,10,26,10,0,0,0,13,73,72,68,82,0,0,0,1,0,0,0,1,
      8,4,0,0,0,181,28,12,2,0,0,0,11,73,68,65,84,120,218,99,100,248,15,
      0,1,5,1,1,39,24,227,102,0,0,0,0,73,69,78,68,174,66,96,130};
    for (std::uint64_t i = 4; i <= 5; ++i) {
      runtime::ImageImportRequest request;
      request.objectId = id(i);
      request.bytes = png;
      request.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(i)});
      ASSERT_EQ(composition.controlFacade().importImage(request).state,
                runtime::ImageCommandState::kApplied);
    }
    Access::select(composition.qualificationRuntime(), {id(1), id(2), id(3), id(4), id(5)});
    ASSERT_EQ(composition.controlFacade().readSelectionSummary().count, 5U);
  }

  template <class Port>
  void verifyPartialFillCommit(Port& port) {
    const auto initial = port.read();
    ASSERT_TRUE(initial.available);
    const auto& fill = initial.properties.at(0x100U);
    ASSERT_EQ(fill.eligible, (std::vector<foundation::ObjectId>{id(1), id(2), id(3)}));
    const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
    const auto beforeD = *composition.qualificationRuntime().qualificationObjectRecord(id(4));
    const auto beforeE = *composition.qualificationRuntime().qualificationObjectRecord(id(5));
    (void)beforeHistory;
    (void)beforeD;
    (void)beforeE;
    const semantic::PropertyValue red = semantic::FillStyleValue{
        semantic::SolidFill{semantic::ColorValue{1, 0, 0, 1}}};
    if constexpr (requires { port.submitProperty(0x100U, red, initial.stamp, fill.eligible, 7001U, 1U); }) {
      const auto submitted = port.submitProperty(0x100U, red, initial.stamp, fill.eligible, 7001U, 1U);
      ASSERT_TRUE(submitted.queued);
      const auto pending = port.receipt(7001U, 1U);
      ASSERT_TRUE(pending);
      EXPECT_FALSE(pending->terminal);
      EXPECT_TRUE(pending->changed.empty());
      ASSERT_TRUE(composition.ownerPump());
      const auto outcome = port.receipt(7001U, 1U);
      ASSERT_TRUE(outcome);
      ASSERT_TRUE(outcome->applied);
      EXPECT_EQ(outcome->changed, (std::vector<foundation::ObjectId>{id(1), id(2), id(3)}));
      ASSERT_EQ(outcome->skipped.size(), 2U);
      EXPECT_EQ(outcome->skipped[0].object, id(4));
      EXPECT_EQ(outcome->skipped[1].object, id(5));
      EXPECT_EQ(outcome->skipped[0].reason, runtime::MultiSelectionExclusion::kNotApplicable);
      EXPECT_EQ(outcome->skipped[1].reason, runtime::MultiSelectionExclusion::kNotApplicable);
      EXPECT_TRUE(outcome->unchanged.empty());
      EXPECT_EQ(*composition.qualificationRuntime().qualificationObjectRecord(id(4)), beforeD);
      EXPECT_EQ(*composition.qualificationRuntime().qualificationObjectRecord(id(5)), beforeE);
      for (const auto objectId : {id(1), id(2), id(3)}) {
        const auto* object = composition.qualificationRuntime().qualificationObjectRecord(objectId);
        ASSERT_NE(object, nullptr);
        const auto entry = std::find_if(object->properties.entries.begin(), object->properties.entries.end(),
                                        [](const auto& item) { return item.field_id == 0x100U; });
        ASSERT_NE(entry, object->properties.entries.end());
        EXPECT_EQ(entry->value, red);
      }
      EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory + 1U);
      auto& facade = composition.controlFacade();
      ASSERT_EQ(facade.undo(100U, facade.readRuntimeState().identity.runtimeGeneration, 7001U).state,
                runtime::ProductControlState::kQueued);
      ASSERT_TRUE(composition.ownerPump());
      for (const auto objectId : {id(1), id(2), id(3)}) {
        const auto* object = composition.qualificationRuntime().qualificationObjectRecord(objectId);
        ASSERT_NE(object, nullptr);
        EXPECT_TRUE(std::none_of(object->properties.entries.begin(), object->properties.entries.end(),
                                 [](const auto& item) { return item.field_id == 0x100U; }));
      }
    } else {
      ADD_FAILURE() << "Typed multi-selection submission and per-root receipt are not implemented";
    }
  }
};

TEST_F(MultiSelectionMSE, MSE_07) {
  preparePartialFillFixture();
  const auto before = composition.readback();
  const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
  const auto projection = composition.multiSelectionPort().read();
  ASSERT_TRUE(projection.available);
  const auto& fill = projection.properties.at(0x100U);
  EXPECT_EQ(fill.total, 5U);
  EXPECT_EQ(fill.eligible, (std::vector<foundation::ObjectId>{id(1), id(2), id(3)}));
  ASSERT_EQ(fill.excluded.size(), 2U);
  EXPECT_EQ(fill.excluded[0].object, id(4));
  EXPECT_EQ(fill.excluded[1].object, id(5));
  EXPECT_EQ(fill.excluded[0].reason, runtime::MultiSelectionExclusion::kNotApplicable);
  EXPECT_EQ(fill.excluded[1].reason, runtime::MultiSelectionExclusion::kNotApplicable);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);
}

#if defined(AXIOM_MSE_FAULT_CTEST_PATH) && defined(AXIOM_MSE_FAULT_BUILD_DIR)
TEST_F(MultiSelectionMSE, MSE_22) {
  for (const auto* name : {"FC04MSEHistoryFaultUndo", "FC04MSEHistoryFaultRedo"}) {
    SCOPED_TRACE(name);
    const std::string command = std::string("\"") + AXIOM_MSE_FAULT_CTEST_PATH +
        "\" --test-dir \"" + AXIOM_MSE_FAULT_BUILD_DIR +
        "\" -R \"^" + name + "$\" --output-on-failure --no-tests=error --timeout 20";
#if defined(_WIN32)
    const auto invocation = std::string("\"") + command + "\"";
#else
    const auto& invocation = command;
#endif
    EXPECT_EQ(std::system(invocation.c_str()), 0) << command;
  }
}
#endif

#if defined(AXIOM_MSE_FAULT_CTEST_PATH) && defined(AXIOM_MSE_FAULT_BUILD_DIR)
TEST_F(MultiSelectionMSE, MSE_15) {
  for (const auto* name : {"FC04MSEMultiIntent", "FC04MSEMultiIntentFault"}) {
    SCOPED_TRACE(name);
    const std::string command = std::string("\"") + AXIOM_MSE_FAULT_CTEST_PATH +
        "\" --test-dir \"" + AXIOM_MSE_FAULT_BUILD_DIR +
        "\" -R \"^" + name + "$\" --output-on-failure --no-tests=error --timeout 20";
#if defined(_WIN32)
    const auto invocation = std::string("\"") + command + "\"";
#else
    const auto& invocation = command;
#endif
    EXPECT_EQ(std::system(invocation.c_str()), 0) << command;
  }
}
#endif

TEST_F(MultiSelectionMSE, MSE_21) {
  preparePartialFillFixture();
  semantic::PatchPropertiesOp patch;
  for (std::uint64_t i = 1; i <= 3; ++i) {
    const semantic::ColorValue color = i == 3 ? semantic::ColorValue{0, 0, 1, 1}
                                               : semantic::ColorValue{1, 0, 0, 1};
    patch.patches.push_back({id(i), 0x100U, semantic::PropertyPatchAction::kSet,
                             semantic::PropertyValue{semantic::FillStyleValue{semantic::SolidFill{color}}}});
  }
  const semantic::Operation operation{semantic::OperationId{id(101)}, semantic::DocumentId{id(1)},
                                     1, 1, std::move(patch)};
  ASSERT_EQ(composition.applyExternalOperation(operation, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  const auto before = composition.readback();
  const auto projection = composition.multiSelectionPort().read();
  ASSERT_TRUE(projection.available);
  const auto& fill = projection.properties.at(0x100U);
  EXPECT_EQ(fill.total, 5U);
  EXPECT_EQ(fill.eligible, (std::vector<foundation::ObjectId>{id(1), id(2), id(3)}));
  EXPECT_EQ(fill.excluded.size(), 2U);
  EXPECT_TRUE(fill.partial);
  EXPECT_TRUE(fill.mixed);
  EXPECT_FALSE(fill.uniformValue);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
}

TEST_F(MultiSelectionMSE, MSE_17) {
  preparePartialFillFixture();
  semantic::ObjectRecord connector;
  connector.id = id(6); connector.kind = semantic::ObjectKind::kConnector; connector.kind_version = 1;
  connector.placement.order_key = semantic::OrderKey({6});
  connector.content = semantic::ConnectorContent{
      {semantic::FreePointEndpoint{{50, 50}}}, {semantic::FreePointEndpoint{{100, 50}}},
      semantic::ConnectorRouting::kStraight};
  connector.properties.entries = {
      {0x200U, semantic::ConnectorDecorationValue::kNone},
      {0x201U, semantic::ConnectorDecorationValue::kArrow}};
  const semantic::Operation insert{semantic::OperationId{id(110)}, semantic::DocumentId{id(1)}, 1, 1,
                                    semantic::InsertObjectsOp{{connector}}};
  ASSERT_EQ(composition.applyExternalOperation(insert, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  Access::select(composition.qualificationRuntime(), {id(1), id(6)});
  const auto beforeShape = *composition.qualificationRuntime().qualificationObjectRecord(id(1));
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto& end = projection.properties.at(0x201U);
  ASSERT_EQ(end.total, 2U);
  ASSERT_EQ(end.eligible, (std::vector<foundation::ObjectId>{id(6)}));
  ASSERT_EQ(end.excluded.size(), 1U);
  EXPECT_EQ(end.excluded.front().object, id(1));
  EXPECT_EQ(end.excluded.front().reason, runtime::MultiSelectionExclusion::kNotApplicable);
  ASSERT_TRUE(port.submitProperty(0x201U, semantic::ConnectorDecorationValue::kNone,
                                  projection.stamp, end.eligible, 7017U, 1U).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto outcome = port.receipt(7017U, 1U);
  ASSERT_TRUE(outcome && outcome->applied);
  EXPECT_EQ(*composition.qualificationRuntime().qualificationObjectRecord(id(1)), beforeShape);
  const auto* after = composition.qualificationRuntime().qualificationObjectRecord(id(6));
  ASSERT_NE(after, nullptr);
  const auto field = [&](std::uint32_t idValue) -> const semantic::PropertyValue* {
    const auto found = std::find_if(after->properties.entries.begin(), after->properties.entries.end(),
        [&](const auto& entry) { return entry.field_id == idValue; });
    return found == after->properties.entries.end() ? nullptr : &found->value;
  };
  const auto effective = [&](std::uint32_t idValue) -> std::optional<semantic::PropertyValue> {
    const auto* stored = field(idValue);
    return stored ? std::optional<semantic::PropertyValue>{*stored} : semantic::propertyDefaultValue(idValue);
  };
  EXPECT_EQ(effective(0x200U), std::optional<semantic::PropertyValue>{semantic::ConnectorDecorationValue::kNone});
  EXPECT_EQ(effective(0x201U), std::optional<semantic::PropertyValue>{semantic::ConnectorDecorationValue::kNone});
}

#if defined(AXIOM_MSE_REGRESSION_BUILD_DIR) && defined(AXIOM_MSE_CTEST_PATH)
TEST_F(MultiSelectionMSE, MSE_18) {
  // Invoke each frozen producer's existing real fixture separately so an
  // absent test or executable cannot be hidden by another matching test.
  for (const auto* name : {"FC01SelectedActions", "FC08SectionConsumer",
                           "FC03ImageFacade", "FC05ConnectorFacade",
                           "FC05ConnectorDragPreview"}) {
    SCOPED_TRACE(name);
    const std::string command = std::string("\"") + AXIOM_MSE_CTEST_PATH +
        "\" --test-dir \"" + AXIOM_MSE_REGRESSION_BUILD_DIR +
        "\" -R \"^" + name + "$\" --output-on-failure --no-tests=error";
#if defined(_WIN32)
    // cmd.exe needs an outer quote pair when the executable and arguments
    // themselves contain quoted paths.
    const auto invocation = std::string("\"") + command + "\"";
#else
    const auto& invocation = command;
#endif
    EXPECT_EQ(std::system(invocation.c_str()), 0) << command;
  }
}
#endif

TEST_F(MultiSelectionMSE, MSE_19) {
  preparePartialFillFixture();
  verifyPartialFillCommit(composition.multiSelectionPort());
}

TEST_F(MultiSelectionMSE, MSE_09) {
  preparePartialFillFixture();
  semantic::PatchPropertiesOp lock{{{id(2), 2U, semantic::PropertyPatchAction::kSet,
                                    semantic::PropertyValue{true}}}};
  const semantic::Operation operation{semantic::OperationId{id(102)}, semantic::DocumentId{id(1)},
                                     1, 1, std::move(lock)};
  ASSERT_EQ(composition.applyExternalOperation(operation, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto& fill = projection.properties.at(0x100U);
  EXPECT_EQ(fill.eligible.size(), 3U);
  EXPECT_FALSE(fill.editAvailable);
  const auto before = composition.readback();
  const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  EXPECT_FALSE(port.submitProperty(0x100U, red, projection.stamp, fill.eligible, 7002U, 1U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);

  const semantic::Operation unlock{semantic::OperationId{id(103)}, semantic::DocumentId{id(1)}, 1, 1,
      semantic::PatchPropertiesOp{{{id(2), 2U, semantic::PropertyPatchAction::kSet, semantic::PropertyValue{false}}}}};
  ASSERT_EQ(composition.applyExternalOperation(unlock, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  const auto displayed = port.read(); ASSERT_TRUE(displayed.available);
  const semantic::Operation changed{semantic::OperationId{id(104)}, semantic::DocumentId{id(1)}, 1, 1,
      semantic::SetTransformsOp{{{id(3), {1,0,0,1,240,0}}}}};
  ASSERT_EQ(composition.applyExternalOperation(changed, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  const auto staleBefore = composition.readback();
  EXPECT_FALSE(port.submitProperty(0x100U, red, displayed.stamp, displayed.properties.at(0x100U).eligible, 7002U, 2U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, staleBefore.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, staleBefore.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);

  semantic::ObjectRecord group; group.id = id(7); group.kind = semantic::ObjectKind::kGroup;
  group.kind_version = 1; group.content = semantic::GroupContent{}; group.placement.order_key = semantic::OrderKey({7});
  semantic::ObjectRecord child; child.id = id(8); child.kind = semantic::ObjectKind::kShape;
  child.kind_version = 1; child.content = semantic::ShapeContent{1,40,30};
  child.placement.parent_id = group.id; child.placement.order_key = semantic::OrderKey({1});
  const semantic::Operation insert{semantic::OperationId{id(105)}, semantic::DocumentId{id(1)}, 1, 1,
                                    semantic::InsertObjectsOp{{group, child}}};
  ASSERT_EQ(composition.applyExternalOperation(insert, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  ASSERT_TRUE(Access::enterGroupScope(composition.qualificationRuntime(), group.id));
  Access::select(composition.qualificationRuntime(), {child.id, id(1)});
  const auto scopeBefore = composition.readback();
  const auto scopedProjection = port.read();
  EXPECT_FALSE(scopedProjection.available);
  EXPECT_FALSE(port.submitProperty(0x100U, red, scopedProjection.stamp, {child.id}, 7002U, 3U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, scopeBefore.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, scopeBefore.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);
}

TEST_F(MultiSelectionMSE, MSE_08) {
  const auto fontPath = std::filesystem::path(MSE_REPO_ROOT) /
      "assets/fonts/noto-sans-cjk-sc/NotoSansCJKsc-Regular.otf";
  std::ifstream fontFile(fontPath, std::ios::binary);
  ASSERT_TRUE(fontFile);
  std::vector<std::uint8_t> bytes{std::istreambuf_iterator<char>(fontFile), {}};
  ASSERT_FALSE(bytes.empty());
  semantic::ResourceManifestEntry font;
  font.resource_id = semantic::ResourceId{id(0xFC0202)};
  font.resource_kind = semantic::ResourceKind::kFont;
  font.resource_revision = 1;
  font.content_hash = "sha256:2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b";
  font.byte_length = bytes.size(); font.decode_version = 1; font.layout_version = 1;
  font.metadata = {8, 1, 16, 0, 24, 1};
  semantic::ResourceManifest manifest; manifest.entries = {font};
  ASSERT_TRUE(composition.bindManifest(manifest, 1));
  ASSERT_TRUE(composition.publishResource(font.resource_id.value, std::move(bytes)));
  ASSERT_TRUE(composition.ownerPump());
  semantic::ObjectRecord shape;
  shape.id = id(1); shape.kind = semantic::ObjectKind::kShape; shape.kind_version = 1;
  shape.content = semantic::ShapeContent{1, 40, 30}; shape.placement.order_key = semantic::OrderKey({1});
  semantic::SolidStroke black; black.color = {0,0,0,1}; black.width = 2; black.cap = semantic::StrokeCap::kButt;
  black.join = semantic::MiterJoin{4}; black.dash = semantic::SolidDash{};
  shape.properties.entries = {{0x100U, semantic::FillStyleValue{semantic::SolidFill{{1,0,0,1}}}},
                              {0x101U, semantic::StrokeStyleValue{black}}};
  semantic::TextStyle textStyle; textStyle.font_resource_id = font.resource_id;
  textStyle.font_size = 16; textStyle.weight = 400; textStyle.color = {0,1,0,1};
  semantic::RichTextContent text;
  text.document.paragraphs = {{id(31), {semantic::ParagraphAlignment::kLeft, 1, 0, 0}, {{"Text", textStyle}}}};
  semantic::ObjectRecord rich;
  rich.id = id(2); rich.kind = semantic::ObjectKind::kRichText; rich.kind_version = 1;
  rich.placement.order_key = semantic::OrderKey({2}); rich.transform.tx = 80; rich.content = text;
  semantic::StrokeRecord stroke;
  stroke.brush.brush_family_id = 1; stroke.brush.brush_version = 1;
  stroke.brush.color = {0,0,1,1}; stroke.brush.nominal_size = 2; stroke.brush.opacity = 1;
  stroke.data = semantic::VectorStrokeData{{semantic::StrokeSample{{10,80},1,{0,0}},
                                             semantic::StrokeSample{{40,80},1,{0,0}}}};
  semantic::ObjectRecord brush;
  brush.id = id(3); brush.kind = semantic::ObjectKind::kVectorStroke; brush.kind_version = 1;
  brush.placement.order_key = semantic::OrderKey({3}); brush.content = semantic::VectorStrokeContent{stroke};
  const semantic::Operation insert{semantic::OperationId{id(100)}, semantic::DocumentId{id(1)}, 1, 1,
                                    semantic::InsertObjectsOp{{shape, rich, brush}}};
  ASSERT_EQ(composition.applyExternalOperation(insert, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  Access::select(composition.qualificationRuntime(), {id(1), id(2), id(3)});
  const auto textBefore = *composition.qualificationRuntime().qualificationObjectRecord(id(2));
  const auto brushBefore = *composition.qualificationRuntime().qualificationObjectRecord(id(3));
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto& fill = projection.properties.at(0x100U);
  ASSERT_EQ(fill.eligible, (std::vector<foundation::ObjectId>{id(1)}));
  ASSERT_EQ(fill.excluded.size(), 2U);
  const semantic::PropertyValue yellow = semantic::FillStyleValue{semantic::SolidFill{{1,1,0,1}}};
  ASSERT_TRUE(port.submitProperty(0x100U, yellow, projection.stamp, fill.eligible, 7008U, 1U).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto outcome = port.receipt(7008U, 1U); ASSERT_TRUE(outcome && outcome->applied);
  EXPECT_EQ(*composition.qualificationRuntime().qualificationObjectRecord(id(2)), textBefore);
  EXPECT_EQ(*composition.qualificationRuntime().qualificationObjectRecord(id(3)), brushBefore);
  const auto* after = composition.qualificationRuntime().qualificationObjectRecord(id(1));
  ASSERT_NE(after, nullptr);
  const auto find = [&](std::uint32_t field) {
    return std::find_if(after->properties.entries.begin(), after->properties.entries.end(),
                        [&](const auto& entry) { return entry.field_id == field; });
  };
  ASSERT_NE(find(0x100U), after->properties.entries.end());
  ASSERT_NE(find(0x101U), after->properties.entries.end());
  EXPECT_EQ(find(0x100U)->value, yellow);
  EXPECT_EQ(find(0x101U)->value, semantic::PropertyValue{semantic::StrokeStyleValue{black}});
}

TEST_F(MultiSelectionMSE, MSE_10) {
  preparePartialFillFixture();
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto opacity = projection.properties.at(3U);
  ASSERT_EQ(opacity.eligible.size(), 5U);
  const auto before = composition.readback();
  const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
  auto& facade = composition.controlFacade();
  const auto beforeControl = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
  const semantic::PropertyValue invalidOpacity{2.0F};
  ASSERT_TRUE(port.submitProperty(3U, invalidOpacity, projection.stamp, opacity.eligible, 7004U, 1U).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto outcome = port.receipt(7004U, 1U);
  ASSERT_TRUE(outcome);
  EXPECT_TRUE(outcome->terminal);
  EXPECT_FALSE(outcome->applied);
  EXPECT_TRUE(outcome->changed.empty());
  EXPECT_EQ(outcome->error, runtime::CanvasControlError::kInvalidValue);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);
  const auto afterControl = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
  EXPECT_EQ(afterControl.canUndo, beforeControl.canUndo);
  EXPECT_EQ(afterControl.canRedo, beforeControl.canRedo);
  auto invalidTargets = opacity.eligible;
  invalidTargets.push_back(id(99));
  EXPECT_FALSE(port.submitProperty(3U, semantic::PropertyValue{0.5F}, projection.stamp,
                                   invalidTargets, 7004U, 2U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);
}

#if AXIOM_MSE_HAS_DEBUG_UI
TEST_F(MultiSelectionMSE, MSE_11) {
  preparePartialFillFixture();
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  auto& facade = composition.controlFacade();
  const auto before = composition.readback();
  const auto history = Access::historySize(composition.qualificationRuntime());
  const auto historyState = facade.readRuntimeState().history;
  struct ImGuiOwner { ImGuiOwner() { ImGui::CreateContext(); } ~ImGuiOwner() { ImGui::DestroyContext(); } } imgui;
  auto& io = ImGui::GetIO(); io.IniFilename = nullptr; io.DisplaySize = {1000,1800}; io.DeltaTime = 1.0F/60;
  unsigned char* pixels = nullptr; int w = 0, h = 0; io.Fonts->GetTexDataAsRGBA32(&pixels, &w, &h);
  ASSERT_NE(pixels, nullptr);
  debug_ui::PanelRegistry panels; debug_ui::registerBuiltinPanels(panels);
  debug_ui::DebugUiSessionState ui;
  debug_ui::DebugControlRouter router(&facade, nullptr, nullptr, nullptr);
  debug_ui::DebugSnapshotAssembler assembler;
  const auto render = [&](bool cancel) {
    const auto snapshot = assembler.capture({&facade}); router.beginFrame(snapshot); ui.beginFrame();
    ImGui::NewFrame(); ImGui::SetNextWindowSize({1000,1800}, ImGuiCond_Always); ImGui::Begin("MSE cancel");
    auto& g = *ImGui::GetCurrentContext();
    if (cancel) { ImGui::PushID(3); g.NavActivateId = g.NavActivateDownId = ImGui::GetID("Cancel");
      ImGui::PopID(); g.NavInputSource = ImGuiInputSource_Keyboard; }
    debug_ui::DebugPanelContext context{snapshot, router, ui};
    for (const auto& panel : panels.contributions(debug_ui::WorkspaceId::kControl))
      if (panel.slot == debug_ui::PanelSlot::kTools) panel.render(context);
    ImGui::End(); ImGui::Render(); g.NavActivateId = g.NavActivateDownId = 0; ImGui::ClearActiveID();
  };
  render(false); ASSERT_TRUE(ui.multiSelectionDraft().initialized);
  ui.multiSelectionDraft().opacity = 0.25F;
  render(true);
  EXPECT_FALSE(ui.multiSelectionDraft().initialized);
  render(false);
  EXPECT_FLOAT_EQ(ui.multiSelectionDraft().opacity, 1.0F);
  EXPECT_FALSE(router.hasMultiSelectionAdmission());
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
  EXPECT_EQ(facade.readRuntimeState().history.canUndo, historyState.canUndo);
  EXPECT_EQ(facade.readRuntimeState().history.canRedo, historyState.canRedo);
}
#endif

#if AXIOM_MSE_HAS_DEBUG_UI
TEST_F(MultiSelectionMSE, MSE_24) {
  std::ifstream file(AXIOM_MSE_HOST_FONT, std::ios::binary);
  ASSERT_TRUE(file);
  std::vector<std::uint8_t> font{std::istreambuf_iterator<char>(file), {}};
  ASSERT_EQ(font.size(), 16437364U);
  const semantic::ResourceId fontId{id(0xFC0202U)};
  semantic::ResourceManifest manifest;
  manifest.entries.push_back({fontId, semantic::ResourceKind::kFont, 1,
      "sha256:2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b",
      font.size(), 1, 1, {8, 1, 16, 0, 24, 1}});
  ASSERT_TRUE(composition.bindManifest(manifest, 1));
  ASSERT_TRUE(composition.publishResource(fontId.value, std::move(font)));
  ASSERT_TRUE(composition.ownerPump());
  semantic::InsertObjectsOp insert;
  for (std::uint64_t i = 1; i <= 2; ++i) {
    semantic::ObjectRecord record;
    record.id = id(i); record.kind = semantic::ObjectKind::kShape; record.kind_version = 3;
    semantic::TextStyle style{fontId, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.document.paragraphs = {{id(100 + i), {semantic::ParagraphAlignment::kLeft, 1, 0, 0}, {{"Text", style}}}};
    label.layout = semantic::RichTextLayout{100};
    semantic::ShapeContent shape{1, 100, 60}; shape.label = std::move(label);
    record.content = std::move(shape);
    record.transform.tx = static_cast<double>(i * 150);
    record.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(i)});
    insert.objects.push_back(std::move(record));
  }
  const semantic::Operation seed{semantic::OperationId{id(1000)}, semantic::DocumentId{id(1)},
                                  1, 1, std::move(insert)};
  const auto seeded = composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync);
  ASSERT_EQ(seeded.disposition, semantic::ApplyDisposition::kApplied)
      << "semantic issue " << static_cast<int>(seeded.error.issue);
  ASSERT_TRUE(composition.qualificationRuntime().setSelectionMode(true));
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  auto& facade = composition.controlFacade();
  ASSERT_EQ(facade.readSelectionSummary().count, 2U);
  ASSERT_EQ(facade.readSelectionSummary().primary, id(1));
  runtime::TextIntent edit;
  edit.target = facade.mountedCanvasTargets().front();
  edit.clientId = 7024U; edit.requestId = 1U;
  edit.kind = runtime::TextIntentKind::kEditAtPoint;
  edit.viewX = 350.0F; edit.viewY = 30.0F;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto receipt = facade.textIntentReceipt(edit.clientId, edit.requestId);
  ASSERT_TRUE(receipt);
  ASSERT_EQ(receipt->status, runtime::TextStatus::kApplied);
  EXPECT_EQ(facade.readTextEditor().token.object, id(2));
  EXPECT_EQ(facade.readSelectionSummary().roots, std::vector<foundation::ObjectId>{id(2)});
  edit.kind = runtime::TextIntentKind::kFinish; edit.requestId++;
  edit.token = facade.readTextEditor().token;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  ASSERT_EQ(facade.textIntentReceipt(edit.clientId, edit.requestId)->status, runtime::TextStatus::kApplied);
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  ASSERT_EQ(facade.readSelectionSummary().primary, id(1));
  ASSERT_TRUE(composition.registerSource(7024U));
  std::uint64_t sequence = 0;
  for (const auto phase : {input::PointerPhase::kDown, input::PointerPhase::kUp,
                           input::PointerPhase::kDown, input::PointerPhase::kUp}) {
    input::PlatformPointerBatch batch;
    batch.samples.push_back({7024U, 1U, ++sequence, sequence * 1000U, 350.0F, 30.0F,
        0, 0, 0, {}, {}, input::SampleProvenance::kConfirmedCurrent, phase});
    ASSERT_TRUE(composition.submit(batch));
    EXPECT_EQ(facade.readTextEditor().phase, runtime::TextPhase::kClosed)
        << "pointer sequence " << sequence << " textTool " << composition.qualificationRuntime().textTool();
  }
  // Web emits dblclick after two pointer ups; the host translates that event
  // into the point intent. Win32 uses its distinct WM_LBUTTONDBLCLK path.
  edit.kind = runtime::TextIntentKind::kEditAtPoint; edit.requestId++;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  ASSERT_EQ(facade.textIntentReceipt(edit.clientId, edit.requestId)->status, runtime::TextStatus::kApplied);
  EXPECT_EQ(facade.readTextEditor().token.object, id(2));
  EXPECT_EQ(facade.readSelectionSummary().roots, std::vector<foundation::ObjectId>{id(2)});
}
TEST_F(MultiSelectionMSE, MSE_25) {
  std::ifstream file(AXIOM_MSE_HOST_FONT, std::ios::binary);
  ASSERT_TRUE(file);
  std::vector<std::uint8_t> font{std::istreambuf_iterator<char>(file), {}};
  ASSERT_EQ(font.size(), 16437364U);
  const semantic::ResourceId fontId{id(0xFC0202U)};
  semantic::ResourceManifest manifest;
  manifest.entries.push_back({fontId, semantic::ResourceKind::kFont, 1,
      "sha256:2c76254f6fc379fddfce0a7e84fb5385bb135d3e399294f6eeb6680d0365b74b",
      font.size(), 1, 1, {8, 1, 16, 0, 24, 1}});
  ASSERT_TRUE(composition.bindManifest(manifest, 1));
  ASSERT_TRUE(composition.publishResource(fontId.value, std::move(font)));
  ASSERT_TRUE(composition.ownerPump());
  semantic::InsertObjectsOp insert;
  semantic::ObjectRecord group;
  group.id = id(3); group.kind = semantic::ObjectKind::kGroup; group.kind_version = 1;
  group.content = semantic::GroupContent{};
  group.placement.order_key = semantic::OrderKey({1});
  insert.objects.push_back(group);
  for (std::uint64_t i = 1; i <= 2; ++i) {
    semantic::ObjectRecord record;
    record.id = id(i); record.kind = semantic::ObjectKind::kShape; record.kind_version = 3;
    semantic::TextStyle style{fontId, 16, 400, false, false, {0, 0, 0, 1}};
    semantic::RichTextContent label;
    label.document.paragraphs = {{id(100 + i), {semantic::ParagraphAlignment::kLeft, 1, 0, 0}, {{"Text", style}}}};
    label.layout = semantic::RichTextLayout{100};
    semantic::ShapeContent shape{1, 100, 60}; shape.label = std::move(label);
    record.content = std::move(shape);
    record.transform.tx = static_cast<double>(i * 150);
    record.placement.parent_id = id(3);
    record.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(i)});
    insert.objects.push_back(std::move(record));
  }
  const semantic::Operation seed{semantic::OperationId{id(1000)}, semantic::DocumentId{id(1)},
                                  1, 1, std::move(insert)};
  const auto seeded = composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync);
  ASSERT_EQ(seeded.disposition, semantic::ApplyDisposition::kApplied)
      << "semantic issue " << static_cast<int>(seeded.error.issue);
  ASSERT_TRUE(composition.qualificationRuntime().setSelectionMode(true));
  ASSERT_TRUE(Access::enterGroupScope(composition.qualificationRuntime(), id(3)));
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  auto& facade = composition.controlFacade();
  ASSERT_EQ(facade.readSelectionSummary().count, 2U);
  ASSERT_EQ(facade.readSelectionSummary().primary, id(1));
  runtime::TextIntent edit;
  edit.target = facade.mountedCanvasTargets().front();
  edit.clientId = 7025U; edit.requestId = 1U;
  edit.kind = runtime::TextIntentKind::kEditAtPoint;
  edit.viewX = 350.0F; edit.viewY = 30.0F;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto receipt = facade.textIntentReceipt(edit.clientId, edit.requestId);
  ASSERT_TRUE(receipt);
  ASSERT_EQ(receipt->status, runtime::TextStatus::kApplied);
  ASSERT_EQ(facade.readTextEditor().token.object, id(2));
  const auto send = [&](runtime::TextInputKind kind, std::u16string value, std::uint64_t comp) {
    const auto state = facade.readTextEditor();
    runtime::TextInputEvent event;
    event.token = state.token; event.sequence = state.nextInputSequence;
    event.kind = kind; event.composition = comp; event.text = std::move(value);
    event.range = state.selection;
    EXPECT_EQ(facade.submitTextInput(event).status, runtime::TextStatus::kQueued);
    EXPECT_TRUE(composition.ownerPump());
    const auto result = facade.textInputReceipt(event.token, event.sequence);
    EXPECT_TRUE(result);
    return result ? result->status : runtime::TextStatus::kUnsupported;
  };
  ASSERT_EQ(send(runtime::TextInputKind::kBegin, {}, 1), runtime::TextStatus::kObserved);
  ASSERT_EQ(send(runtime::TextInputKind::kUpdate, u"\u5019\u9009", 1), runtime::TextStatus::kObserved);
  ASSERT_EQ(facade.readTextEditor().phase, runtime::TextPhase::kComposing);
  edit.kind = runtime::TextIntentKind::kEscape; edit.requestId++;
  edit.token = facade.readTextEditor().token;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  ASSERT_EQ(facade.textIntentReceipt(edit.clientId, edit.requestId)->status, runtime::TextStatus::kObserved);
  EXPECT_EQ(facade.readTextEditor().phase, runtime::TextPhase::kEditing);
  EXPECT_TRUE(facade.readTextEditor().preedit.empty());
  EXPECT_EQ(facade.readTextEditor().projection, u"Text");
  EXPECT_EQ(facade.readSelectionSummary().groupScope, id(3));
  EXPECT_EQ(facade.readSelectionSummary().roots, std::vector<foundation::ObjectId>{id(2)});
  edit.requestId++; edit.token = facade.readTextEditor().token;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  EXPECT_EQ(facade.readTextEditor().phase, runtime::TextPhase::kClosed);
  EXPECT_EQ(facade.readSelectionSummary().roots, std::vector<foundation::ObjectId>{id(2)});
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  edit.kind = runtime::TextIntentKind::kEditAtPoint; edit.requestId++;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  ASSERT_EQ(facade.textIntentReceipt(edit.clientId, edit.requestId)->status, runtime::TextStatus::kApplied);
  ASSERT_EQ(send(runtime::TextInputKind::kReplace, u"!", 0), runtime::TextStatus::kApplied);
  const auto committedProjection = facade.readTextEditor().projection;
  ASSERT_NE(committedProjection.find(u"!"), std::u16string::npos);
  edit.kind = runtime::TextIntentKind::kFinish; edit.requestId++;
  edit.token = facade.readTextEditor().token;
  ASSERT_EQ(facade.submitTextIntent(edit).status, runtime::TextStatus::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  ASSERT_EQ(facade.textIntentReceipt(edit.clientId, edit.requestId)->status, runtime::TextStatus::kApplied);
  EXPECT_EQ(facade.readTextEditor().phase, runtime::TextPhase::kClosed);
  EXPECT_EQ(facade.readSelectionSummary().roots, std::vector<foundation::ObjectId>{id(2)});
  const auto* committed = composition.qualificationRuntime().qualificationObjectRecord(id(2));
  ASSERT_NE(committed, nullptr);
  const auto& label = *std::get<semantic::ShapeContent>(committed->content).label;
  ASSERT_FALSE(label.document.paragraphs.empty());
  std::string text;
  for (const auto& run : label.document.paragraphs.front().runs) text += run.text;
  EXPECT_NE(text.find('!'), std::string::npos);
}
#endif

TEST_F(MultiSelectionMSE, MSE_12) {
  preparePartialFillFixture();
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto eligible = projection.properties.at(0x100U).eligible;
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  ASSERT_TRUE(port.submitProperty(0x100U, red, projection.stamp, eligible, 7005U, 1U).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto applied = port.receipt(7005U, 1U);
  ASSERT_TRUE(applied);
  ASSERT_TRUE(applied->applied);
  const auto image = *composition.qualificationRuntime().qualificationObjectRecord(id(4));
  Access::select(composition.qualificationRuntime(), {id(4)});
  auto& facade = composition.controlFacade();
  ASSERT_EQ(facade.undo(100U, facade.readRuntimeState().identity.runtimeGeneration, 7005U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  for (const auto objectId : {id(1), id(2), id(3)}) {
    const auto* object = composition.qualificationRuntime().qualificationObjectRecord(objectId);
    ASSERT_NE(object, nullptr);
    EXPECT_TRUE(std::none_of(object->properties.entries.begin(), object->properties.entries.end(),
                             [](const auto& item) { return item.field_id == 0x100U; }));
  }
  EXPECT_EQ(*composition.qualificationRuntime().qualificationObjectRecord(id(4)), image);
  EXPECT_EQ(facade.readSelectionSummary().roots, std::vector<foundation::ObjectId>{id(4)});
}

TEST_F(MultiSelectionMSE, MSE_13) {
  preparePartialFillFixture();
  const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto eligible = projection.properties.at(0x100U).eligible;
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  ASSERT_TRUE(port.submitProperty(0x100U, red, projection.stamp, eligible, 7006U, 1U).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto applied = port.receipt(7006U, 1U);
  ASSERT_TRUE(applied);
  ASSERT_TRUE(applied->applied);
  auto& facade = composition.controlFacade();
  const auto target = facade.mountedCanvasTargets().front();
  const auto forwardId = facade.readCanvasControlSnapshot(target).lastAppliedOperation;
  ASSERT_TRUE(forwardId);
  ASSERT_EQ(facade.undo(100U, facade.readRuntimeState().identity.runtimeGeneration, 7006U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto undoId = facade.readCanvasControlSnapshot(target).lastAppliedOperation;
  ASSERT_TRUE(undoId);
  ASSERT_EQ(facade.redo(101U, facade.readRuntimeState().identity.runtimeGeneration, 7006U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto redoId = facade.readCanvasControlSnapshot(target).lastAppliedOperation;
  ASSERT_TRUE(redoId);
  EXPECT_NE(undoId, forwardId);
  EXPECT_NE(redoId, forwardId);
  EXPECT_NE(redoId, undoId);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory + 1U);
  for (const auto objectId : eligible) {
    const auto* object = composition.qualificationRuntime().qualificationObjectRecord(objectId);
    ASSERT_NE(object, nullptr);
    const auto entry = std::find_if(object->properties.entries.begin(), object->properties.entries.end(),
                                    [](const auto& item) { return item.field_id == 0x100U; });
    ASSERT_NE(entry, object->properties.entries.end());
    EXPECT_EQ(entry->value, red);
  }
}

TEST_F(MultiSelectionMSE, MSE_27) {
  preparePartialFillFixture();
  const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
  auto& port = composition.multiSelectionPort();
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  auto projection = port.read();
  ASSERT_TRUE(projection.available);
  auto roots = projection.properties.at(0x100U).eligible;
  const semantic::Operation first{semantic::OperationId{id(103)}, semantic::DocumentId{id(1)}, 1, 1,
      semantic::SetTransformsOp{{{id(3), {1, 0, 0, 1, 220, 0}}}}};
  ASSERT_EQ(composition.applyExternalOperation(first, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  auto before = composition.readback();
  EXPECT_FALSE(port.submitProperty(0x100U, red, projection.stamp, roots, 7003U, 1U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);

  projection = port.read();
  roots = projection.properties.at(0x100U).eligible;
  ASSERT_TRUE(port.submitProperty(0x100U, red, projection.stamp, roots, 7003U, 2U).queued);
  const semantic::Operation second{semantic::OperationId{id(104)}, semantic::DocumentId{id(1)}, 1, 1,
      semantic::SetTransformsOp{{{id(3), {1, 0, 0, 1, 240, 0}}}}};
  ASSERT_EQ(composition.applyExternalOperation(second, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  before = composition.readback();
  ASSERT_TRUE(composition.ownerPump());
  auto outcome = port.receipt(7003U, 2U);
  ASSERT_TRUE(outcome);
  EXPECT_TRUE(outcome->terminal);
  EXPECT_FALSE(outcome->applied);
  EXPECT_TRUE(outcome->changed.empty());
  EXPECT_EQ(outcome->state, runtime::CanvasControlReceiptState::kStaleTarget);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);

  projection = port.read();
  roots = projection.properties.at(0x100U).eligible;
  ASSERT_TRUE(port.submitProperty(0x100U, red, projection.stamp, roots, 7003U, 3U).queued);
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  before = composition.readback();
  ASSERT_TRUE(composition.ownerPump());
  outcome = port.receipt(7003U, 3U);
  ASSERT_TRUE(outcome);
  EXPECT_TRUE(outcome->terminal);
  EXPECT_FALSE(outcome->applied);
  EXPECT_TRUE(outcome->changed.empty());
  EXPECT_EQ(outcome->state, runtime::CanvasControlReceiptState::kStaleTarget);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);

  Access::select(composition.qualificationRuntime(), {id(1), id(2), id(3), id(4), id(5)});
  projection = port.read();
  roots = projection.properties.at(0x100U).eligible;
  ASSERT_TRUE(port.submitProperty(0x100U, red, projection.stamp, roots, 7003U, 4U).queued);
  before = composition.readback();
  ASSERT_TRUE(composition.resizeSurface(801, 601));
  ASSERT_TRUE(composition.ownerPump());
  outcome = port.receipt(7003U, 4U);
  ASSERT_TRUE(outcome);
  EXPECT_TRUE(outcome->terminal);
  EXPECT_FALSE(outcome->applied);
  EXPECT_TRUE(outcome->changed.empty());
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory);
}

TEST_F(MultiSelectionMSE, MSE_01) {
  const auto before = composition.readback();
  const auto projection = composition.multiSelectionPort().read();
  ASSERT_TRUE(projection.available);
  EXPECT_TRUE(projection.roots.empty());
  for (const auto& [field, property] : projection.properties) {
    (void)field;
    EXPECT_EQ(property.total, 0U);
    EXPECT_TRUE(property.eligible.empty());
    EXPECT_FALSE(property.uniformValue);
    EXPECT_FALSE(property.editAvailable);
  }
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
}

TEST_F(MultiSelectionMSE, MSE_02) {
  preparePartialFillFixture();
  Access::select(composition.qualificationRuntime(), {id(1)});
  const semantic::Operation initial{semantic::OperationId{id(107)}, semantic::DocumentId{id(1)}, 1, 1,
      semantic::PatchPropertiesOp{{{id(1), 3U, semantic::PropertyPatchAction::kSet,
                                    semantic::PropertyValue{0.5F}}}}};
  ASSERT_EQ(composition.applyExternalOperation(initial, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  auto& facade = composition.controlFacade();
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  EXPECT_EQ(projection.properties.at(3U).eligible, std::vector<foundation::ObjectId>{id(1)});
  const auto beforeHistory = Access::historySize(composition.qualificationRuntime());
  runtime::CanvasControlRequest request;
  request.target = facade.mountedCanvasTargets().front();
  request.clientId = 7010U;
  request.requestId = 1U;
  request.payload.kind = runtime::CanvasControlPayloadKind::kSelectedAppearance;
  request.payload.appearance.opacity = 0.75F;
  ASSERT_EQ(facade.submitCanvasControl(request).state, runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto receipt = facade.canvasControlReceipt({7010U, 1U});
  ASSERT_TRUE(receipt);
  ASSERT_EQ(receipt->state, runtime::CanvasControlReceiptState::kApplied);
  auto after = port.read();
  ASSERT_TRUE(after.available);
  ASSERT_TRUE(after.properties.at(3U).uniformValue);
  EXPECT_EQ(std::get<float>(*after.properties.at(3U).uniformValue), 0.75F);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeHistory + 1U);
  ASSERT_EQ(facade.undo(2U, facade.readRuntimeState().identity.runtimeGeneration, 7010U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  after = port.read();
  ASSERT_TRUE(after.properties.at(3U).uniformValue);
  EXPECT_EQ(std::get<float>(*after.properties.at(3U).uniformValue), 0.5F);
  ASSERT_EQ(facade.redo(3U, facade.readRuntimeState().identity.runtimeGeneration, 7010U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  after = port.read();
  ASSERT_TRUE(after.properties.at(3U).uniformValue);
  EXPECT_EQ(std::get<float>(*after.properties.at(3U).uniformValue), 0.75F);
}

TEST_F(MultiSelectionMSE, MSE_03) {
  semantic::InsertObjectsOp insert;
  for (std::uint64_t n = 1; n <= 2; ++n) {
    semantic::ObjectRecord r;
    r.id = id(n); r.kind = semantic::ObjectKind::kShape; r.kind_version = 1;
    r.content = semantic::ShapeContent{1, 20, 20};
    r.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(n)});
    r.transform.tx = n == 1 ? 10 : 40; r.transform.ty = n == 1 ? 20 : 60;
    insert.objects.push_back(r);
  }
  const semantic::Operation seed{semantic::OperationId{id(100)}, semantic::DocumentId{id(1)},
                                  1, 1, std::move(insert)};
  ASSERT_EQ(composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  ASSERT_TRUE(composition.registerSource(7100U));
  auto& facade = composition.controlFacade();
  ASSERT_EQ(facade.setSelectionMode(true, 1U, facade.readRuntimeState().identity.runtimeGeneration,
                                    7100U).state, runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  std::uint64_t seq = 0;
  const auto send = [&](input::PointerPhase phase, float x, float y, bool shift = false) {
    const auto snapshot = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    input::PlatformPointerSample p;
    p.source = 7100U; p.pointer = 1U; p.sequence = ++seq; p.timestampNs = seq * 1000000U;
    p.phase = phase; p.x = x * snapshot.cameraScale + snapshot.cameraTranslationX;
    p.y = y * snapshot.cameraScale + snapshot.cameraTranslationY;
    p.modifiers = shift ? input::PointerModifiers::kShift : input::PointerModifiers::kNone;
    input::PlatformPointerBatch batch; batch.samples.push_back(p);
    return composition.submit(batch);
  };
  ASSERT_TRUE(send(input::PointerPhase::kDown, 15, 25));
  ASSERT_TRUE(send(input::PointerPhase::kUp, 15, 25));
  ASSERT_TRUE(send(input::PointerPhase::kDown, 45, 65, true));
  ASSERT_TRUE(send(input::PointerPhase::kUp, 45, 65, true));
  ASSERT_EQ(facade.readSelectionSummary().count, 2U);
  const auto history = Access::historySize(composition.qualificationRuntime());
  ASSERT_TRUE(send(input::PointerPhase::kDown, 15, 25));
  ASSERT_TRUE(send(input::PointerPhase::kMove, 20, 32));
  const auto pa = Access::preview(composition.qualificationRuntime(), id(1));
  const auto pb = Access::preview(composition.qualificationRuntime(), id(2));
  ASSERT_TRUE(pa && pb);
  EXPECT_DOUBLE_EQ(pa->tx, 15); EXPECT_DOUBLE_EQ(pa->ty, 27);
  EXPECT_DOUBLE_EQ(pb->tx, 45); EXPECT_DOUBLE_EQ(pb->ty, 67);
  ASSERT_TRUE(send(input::PointerPhase::kUp, 20, 32));
  const auto* a = composition.qualificationRuntime().qualificationObjectRecord(id(1));
  const auto* b = composition.qualificationRuntime().qualificationObjectRecord(id(2));
  ASSERT_NE(a, nullptr); ASSERT_NE(b, nullptr);
  EXPECT_DOUBLE_EQ(a->transform.tx, 15); EXPECT_DOUBLE_EQ(a->transform.ty, 27);
  EXPECT_DOUBLE_EQ(b->transform.tx, 45); EXPECT_DOUBLE_EQ(b->transform.ty, 67);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history + 1U);
  ASSERT_EQ(facade.undo(2U, facade.readRuntimeState().identity.runtimeGeneration, 7100U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  a = composition.qualificationRuntime().qualificationObjectRecord(id(1));
  b = composition.qualificationRuntime().qualificationObjectRecord(id(2));
  ASSERT_NE(a, nullptr); ASSERT_NE(b, nullptr);
  EXPECT_DOUBLE_EQ(a->transform.tx, 10); EXPECT_DOUBLE_EQ(a->transform.ty, 20);
  EXPECT_DOUBLE_EQ(b->transform.tx, 40); EXPECT_DOUBLE_EQ(b->transform.ty, 60);
  const auto beforeDenied = composition.readback();
  const auto redoBefore = facade.readRuntimeState().history.canRedo;
  ASSERT_TRUE(send(input::PointerPhase::kDown, 15, 25));
  ASSERT_TRUE(send(input::PointerPhase::kMove, 20, 32));
  ASSERT_TRUE(Access::preview(composition.qualificationRuntime(), id(1)));
  ASSERT_TRUE(composition.multiSelectionPort().setPermissionMode(runtime::MultiSelectionPermissionMode::kContextUnavailable));
  ASSERT_TRUE(send(input::PointerPhase::kMove, 21, 33));
  ASSERT_TRUE(send(input::PointerPhase::kUp, 21, 33));
  EXPECT_FALSE(Access::preview(composition.qualificationRuntime(), id(1)));
  EXPECT_EQ(composition.readback().semanticGeneration, beforeDenied.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, beforeDenied.sceneRevision);
  EXPECT_EQ(facade.readRuntimeState().history.canRedo, redoBefore);
}

TEST_F(MultiSelectionMSE, MSE_04) {
  semantic::ObjectRecord group;
  group.id = id(1); group.kind = semantic::ObjectKind::kGroup; group.kind_version = 1;
  group.content = semantic::GroupContent{}; group.placement.order_key = semantic::OrderKey({1});
  group.transform.tx = 10; group.transform.ty = 20;
  semantic::ObjectRecord child;
  child.id = id(2); child.kind = semantic::ObjectKind::kShape; child.kind_version = 1;
  child.content = semantic::ShapeContent{1, 20, 20};
  child.placement.parent_id = group.id; child.placement.order_key = semantic::OrderKey({1});
  child.transform.tx = 30; child.transform.ty = 40;
  const semantic::Operation seed{semantic::OperationId{id(100)}, semantic::DocumentId{id(1)},
                                  1, 1, semantic::InsertObjectsOp{{group, child}}};
  ASSERT_EQ(composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  ASSERT_TRUE(composition.registerSource(7101U));
  auto& facade = composition.controlFacade();
  ASSERT_EQ(facade.setSelectionMode(true, 1U, facade.readRuntimeState().identity.runtimeGeneration,
                                    7101U).state, runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  ASSERT_EQ(facade.readSelectionSummary().roots, (std::vector<foundation::ObjectId>{id(1)}));
  const auto history = Access::historySize(composition.qualificationRuntime());
  std::uint64_t seq = 0;
  const auto send = [&](input::PointerPhase phase, float x, float y) {
    const auto snapshot = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    input::PlatformPointerSample sample;
    sample.source = 7101U; sample.pointer = 1U; sample.sequence = ++seq; sample.timestampNs = seq * 1000000U;
    sample.phase = phase; sample.x = x * snapshot.cameraScale + snapshot.cameraTranslationX;
    sample.y = y * snapshot.cameraScale + snapshot.cameraTranslationY;
    input::PlatformPointerBatch batch; batch.samples.push_back(sample); return composition.submit(batch);
  };
  ASSERT_TRUE(send(input::PointerPhase::kDown, 45, 65));
  ASSERT_TRUE(send(input::PointerPhase::kMove, 50, 72));
  const auto previewGroup = Access::preview(composition.qualificationRuntime(), id(1));
  ASSERT_TRUE(previewGroup);
  EXPECT_DOUBLE_EQ(previewGroup->tx, 15); EXPECT_DOUBLE_EQ(previewGroup->ty, 27);
  EXPECT_FALSE(Access::preview(composition.qualificationRuntime(), id(2)));
  ASSERT_TRUE(send(input::PointerPhase::kUp, 50, 72));
  const auto world = Access::world(composition.qualificationRuntime(), id(2));
  ASSERT_TRUE(world); EXPECT_DOUBLE_EQ(world->tx, 45); EXPECT_DOUBLE_EQ(world->ty, 67);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history + 1U);
  ASSERT_EQ(facade.undo(2U, facade.readRuntimeState().identity.runtimeGeneration, 7101U).state,
            runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto restored = Access::world(composition.qualificationRuntime(), id(2));
  ASSERT_TRUE(restored); EXPECT_DOUBLE_EQ(restored->tx, 40); EXPECT_DOUBLE_EQ(restored->ty, 60);
}

TEST_F(MultiSelectionMSE, AttachedConnectorTracksBothRootsDuringMultiDrag) {
  preparePartialFillFixture();
  semantic::ObjectRecord connector;
  connector.id = id(6); connector.kind = semantic::ObjectKind::kConnector; connector.kind_version = 1;
  connector.placement.order_key = semantic::OrderKey({6});
  connector.content = semantic::ConnectorContent{
      {semantic::AttachedEndpoint{id(1), semantic::StablePortAnchor{2}}},
      {semantic::AttachedEndpoint{id(2), semantic::StablePortAnchor{4}}},
      semantic::ConnectorRouting::kStraight};
  const semantic::Operation insert{semantic::OperationId{id(111)}, semantic::DocumentId{id(1)}, 1, 1,
                                    semantic::InsertObjectsOp{{connector}}};
  ASSERT_EQ(composition.applyExternalOperation(insert, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  ASSERT_TRUE(composition.registerSource(7102U));
  auto& facade = composition.controlFacade();
  ASSERT_EQ(facade.setSelectionMode(true, 1U, facade.readRuntimeState().identity.runtimeGeneration,
                                    7102U).state, runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  const auto before = Access::previewConnector(composition.qualificationRuntime(), id(6));
  ASSERT_TRUE(before);
  std::uint64_t seq = 0;
  const auto send = [&](input::PointerPhase phase, float x, float y) {
    const auto snapshot = facade.readCanvasControlSnapshot(facade.mountedCanvasTargets().front());
    input::PlatformPointerSample p;
    p.source = 7102U; p.pointer = 1U; p.sequence = ++seq; p.timestampNs = seq * 1000000U;
    p.phase = phase; p.x = x * snapshot.cameraScale + snapshot.cameraTranslationX;
    p.y = y * snapshot.cameraScale + snapshot.cameraTranslationY;
    input::PlatformPointerBatch batch; batch.samples.push_back(p); return composition.submit(batch);
  };
  ASSERT_TRUE(send(input::PointerPhase::kDown, 65, 10));
  ASSERT_TRUE(send(input::PointerPhase::kMove, 70, 17));
  const auto moved = Access::previewConnector(composition.qualificationRuntime(), id(6));
  ASSERT_TRUE(moved);
  EXPECT_NEAR(moved->start.x, before->start.x + 5, 0.001);
  EXPECT_NEAR(moved->start.y, before->start.y + 7, 0.001);
  EXPECT_NEAR(moved->end.x, before->end.x + 5, 0.001);
  EXPECT_NEAR(moved->end.y, before->end.y + 7, 0.001);
  ASSERT_TRUE(send(input::PointerPhase::kCancel, 70, 17));
}

TEST_F(MultiSelectionMSE, MSE_05) {
  preparePartialFillFixture();
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  semantic::PatchPropertiesOp patch;
  for (const auto objectId : {id(1), id(2)})
    patch.patches.push_back({objectId, 3U, semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.5F}});
  const semantic::Operation operation{semantic::OperationId{id(105)}, semantic::DocumentId{id(1)}, 1, 1, std::move(patch)};
  ASSERT_EQ(composition.applyExternalOperation(operation, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  const auto projection = composition.multiSelectionPort().read();
  ASSERT_TRUE(projection.available);
  const auto& opacity = projection.properties.at(3U);
  EXPECT_EQ(opacity.eligible.size(), 2U);
  EXPECT_FALSE(opacity.partial);
  EXPECT_FALSE(opacity.mixed);
  ASSERT_TRUE(opacity.uniformValue);
  EXPECT_EQ(std::get<float>(*opacity.uniformValue), 0.5F);
}

TEST_F(MultiSelectionMSE, MSE_06) {
  preparePartialFillFixture();
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  semantic::PatchPropertiesOp patch{{
    {id(1), 3U, semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.25F}},
    {id(2), 3U, semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.75F}}}};
  const semantic::Operation operation{semantic::OperationId{id(106)}, semantic::DocumentId{id(1)}, 1, 1, std::move(patch)};
  ASSERT_EQ(composition.applyExternalOperation(operation, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto& opacity = projection.properties.at(3U);
  EXPECT_EQ(opacity.eligible.size(), 2U);
  EXPECT_FALSE(opacity.partial);
  EXPECT_TRUE(opacity.mixed);
  EXPECT_FALSE(opacity.uniformValue);
  const auto history = Access::historySize(composition.qualificationRuntime());
  ASSERT_TRUE(port.submitProperty(3U, semantic::PropertyValue{0.5F}, projection.stamp,
                                  opacity.eligible, 7011U, 1U).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto outcome = port.receipt(7011U, 1U);
  ASSERT_TRUE(outcome);
  ASSERT_TRUE(outcome->applied);
  EXPECT_EQ(outcome->changed, (std::vector<foundation::ObjectId>{id(1), id(2)}));
  const auto after = port.read();
  ASSERT_TRUE(after.properties.at(3U).uniformValue);
  EXPECT_EQ(std::get<float>(*after.properties.at(3U).uniformValue), 0.5F);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history + 1U);
}

TEST_F(MultiSelectionMSE, MSE_14) {
  preparePartialFillFixture();
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  auto& port = composition.multiSelectionPort();
  const semantic::PropertyValue value{0.5F};
  auto displayed = port.read();
  ASSERT_TRUE(displayed.available);
  Access::select(composition.qualificationRuntime(), {id(1), id(3)});
  auto before = composition.readback();
  auto history = Access::historySize(composition.qualificationRuntime());
  EXPECT_FALSE(port.submitProperty(3U, value, displayed.stamp, displayed.properties.at(3U).eligible, 7014U, 1U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);

  displayed = port.read();
  const semantic::Operation change{semantic::OperationId{id(114)}, semantic::DocumentId{id(1)}, 1, 1,
      semantic::SetTransformsOp{{{id(3), {1,0,0,1,220,0}}}}};
  ASSERT_EQ(composition.applyExternalOperation(change, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  before = composition.readback(); history = Access::historySize(composition.qualificationRuntime());
  EXPECT_FALSE(port.submitProperty(3U, value, displayed.stamp, displayed.properties.at(3U).eligible, 7014U, 2U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);

  semantic::ObjectRecord connector;
  connector.id = id(6); connector.kind = semantic::ObjectKind::kConnector; connector.kind_version = 1;
  connector.placement.order_key = semantic::OrderKey({6});
  connector.content = semantic::ConnectorContent{{semantic::FreePointEndpoint{{10,10}}},
      {semantic::FreePointEndpoint{{20,20}}}, semantic::ConnectorRouting::kStraight};
  const semantic::Operation insert{semantic::OperationId{id(115)}, semantic::DocumentId{id(1)}, 1, 1,
                                    semantic::InsertObjectsOp{{connector}}};
  ASSERT_EQ(composition.applyExternalOperation(insert, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  displayed = port.read(); ASSERT_TRUE(displayed.available);
  before = composition.readback();
  ASSERT_TRUE(Access::publishConnectorRoute(composition.qualificationRuntime(), id(6)));
  const auto published = composition.readback();
  ASSERT_EQ(published.semanticGeneration, before.semanticGeneration);
  ASSERT_GT(published.sceneRevision, before.sceneRevision);
  history = Access::historySize(composition.qualificationRuntime());
  EXPECT_FALSE(port.submitProperty(3U, value, displayed.stamp, displayed.properties.at(3U).eligible, 7014U, 3U).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, published.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, published.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
}

TEST_F(MultiSelectionMSE, MSE_16) {
  preparePartialFillFixture();
  auto& facade = composition.controlFacade();
  const auto target = facade.mountedCanvasTargets().front();
  const auto before = composition.readback();
  const auto beforeControl = facade.readCanvasControlSnapshot(target);
  const auto history = Access::historySize(composition.qualificationRuntime());
  for (int iteration = 0; iteration < 4; ++iteration) {
    ASSERT_TRUE(composition.multiSelectionPort().read().available);
    ASSERT_EQ(facade.readSelectionSummary().count, 5U);
  }
  const auto after = composition.readback();
  const auto afterControl = facade.readCanvasControlSnapshot(target);
  EXPECT_EQ(after.semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(after.sceneRevision, before.sceneRevision);
  EXPECT_EQ(afterControl.lastAppliedOperation, beforeControl.lastAppliedOperation);
  EXPECT_EQ(afterControl.canUndo, beforeControl.canUndo);
  EXPECT_EQ(afterControl.canRedo, beforeControl.canRedo);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
}

TEST_F(MultiSelectionMSE, MissingPermissionProviderDisablesWrite) {
  ASSERT_TRUE(composition.multiSelectionPort().setPermissionMode(
      runtime::MultiSelectionPermissionMode::kContextUnavailable));
  preparePartialFillFixture();
  auto& port = composition.multiSelectionPort();
  const auto before = composition.readback();
  const auto history = Access::historySize(composition.qualificationRuntime());
  const auto projection = port.read();
  ASSERT_TRUE(projection.available);
  const auto& fill = projection.properties.at(0x100U);
  EXPECT_FALSE(fill.editAvailable);
  EXPECT_EQ(fill.availability, runtime::MultiSelectionAvailability::kPermissionUnknown);
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  const auto receipt = port.submitProperty(0x100U, red, projection.stamp, fill.eligible, 7999U, 1U);
  EXPECT_FALSE(receipt.queued);
  EXPECT_EQ(receipt.error, runtime::CanvasControlError::kResourceUnavailable);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
}

TEST_F(MultiSelectionMSE, EvictedReceiptReportsExpired) {
  preparePartialFillFixture();
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  const auto eligible = projection.properties.at(3U).eligible;
  for (std::uint64_t request = 1; request <= 1025; ++request) {
    ASSERT_TRUE(port.submitProperty(3U, semantic::PropertyValue{2.0F}, projection.stamp,
                                    eligible, 7997U, request).queued) << request;
    ASSERT_TRUE(composition.ownerPump());
  }
  const auto expired = port.receipt(7997U, 1U);
  ASSERT_TRUE(expired);
  EXPECT_TRUE(expired->terminal);
  EXPECT_FALSE(expired->applied);
  EXPECT_EQ(expired->state, runtime::CanvasControlReceiptState::kExpired);
}

TEST_F(MultiSelectionMSE, PermissionChangeInvalidatesQueuedWrite) {
  preparePartialFillFixture();
  auto& port = composition.multiSelectionPort();
  const auto projection = port.read();
  const auto& fill = projection.properties.at(0x100U);
  ASSERT_TRUE(fill.editAvailable);
  const auto before = composition.readback();
  const auto history = Access::historySize(composition.qualificationRuntime());
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  ASSERT_TRUE(port.submitProperty(0x100U, red, projection.stamp, fill.eligible, 7998U, 1U).queued);
  ASSERT_TRUE(port.setPermissionMode(runtime::MultiSelectionPermissionMode::kContextUnavailable));
  ASSERT_TRUE(composition.ownerPump());
  const auto outcome = port.receipt(7998U, 1U);
  ASSERT_TRUE(outcome);
  EXPECT_TRUE(outcome->terminal);
  EXPECT_FALSE(outcome->applied);
  EXPECT_TRUE(outcome->changed.empty());
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
}

TEST_F(MultiSelectionMSE, ProjectionRejectsWrongThreadAndClosedOwner) {
  runtime::MultiSelectionProjection other;
  std::thread worker([&] { other = composition.multiSelectionPort().read(); });
  worker.join();
  EXPECT_FALSE(other.available);
  EXPECT_TRUE(other.roots.empty());
  ASSERT_TRUE(composition.close());
  const auto closed = composition.multiSelectionPort().read();
  EXPECT_FALSE(closed.available);
  EXPECT_TRUE(closed.roots.empty());
}
TEST_F(MultiSelectionMSE, PublicFacadePermissionChangeInvalidatesQueuedWrite) {
  preparePartialFillFixture();
  auto& facade = composition.controlFacade();
  const auto projection = facade.readMultiSelection();
  ASSERT_TRUE(projection.available);
  const auto& fill = projection.properties.at(0x100U);
  ASSERT_TRUE(fill.editAvailable);
  const auto before = composition.readback();
  const auto history = Access::historySize(composition.qualificationRuntime());
  const auto state = facade.readRuntimeState().history;
  const semantic::PropertyValue red = semantic::FillStyleValue{semantic::SolidFill{{1, 0, 0, 1}}};
  ASSERT_TRUE(facade.submitMultiSelectionProperty(0x100U, red, projection.stamp, fill.eligible, 7996U, 1U).queued);
  ASSERT_TRUE(facade.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kContextUnavailable));
  ASSERT_TRUE(composition.ownerPump());
  const auto outcome = facade.multiSelectionReceipt(7996U, 1U);
  ASSERT_TRUE(outcome);
  EXPECT_TRUE(outcome->terminal);
  EXPECT_FALSE(outcome->applied);
  EXPECT_TRUE(outcome->changed.empty());
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(composition.readback().sceneRevision, before.sceneRevision);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
  EXPECT_EQ(facade.readRuntimeState().history.canRedo, state.canRedo);
}

TEST_F(MultiSelectionMSE, PublicStrokeWidthPreservesEachObjectAndUndoRedo) {
  semantic::SolidStroke first{{1, 0, 0, 1}, 5, semantic::StrokeCap::kRound,
                              semantic::MiterJoin{4}, semantic::DashPattern{{2, 1}, 0}};
  semantic::SolidStroke second{{0, 0, 1, 1}, 7, semantic::StrokeCap::kSquare,
                               semantic::MiterJoin{7}, semantic::DashPattern{{4, 2}, 0}};
  semantic::InsertObjectsOp insert;
  for (std::uint64_t n = 1; n <= 2; ++n) {
    semantic::ObjectRecord object{};
    object.id = id(n); object.kind = semantic::ObjectKind::kShape; object.kind_version = 1;
    object.content = semantic::ShapeContent{1, 40, 30};
    object.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(n)});
    object.transform.tx = static_cast<double>(n * 70);
    object.properties.entries.push_back({0x100U, semantic::FillStyleValue{semantic::SolidFill{{0, 0, 1, 1}}}});
    object.properties.entries.push_back({0x101U, semantic::StrokeStyleValue{n == 1 ? first : second}});
    insert.objects.push_back(std::move(object));
  }
  const semantic::Operation seed{semantic::OperationId{id(101)}, semantic::DocumentId{id(1)}, 1, 1, std::move(insert)};
  ASSERT_EQ(composition.applyExternalOperation(seed, semantic::ApplySource::kRemoteSync).disposition,
            semantic::ApplyDisposition::kApplied);
  Access::select(composition.qualificationRuntime(), {id(1), id(2)});
  auto& facade = composition.controlFacade();
  const auto projection = facade.readMultiSelection();
  ASSERT_TRUE(projection.available);
  const auto& stroke = projection.properties.at(0x101U);
  ASSERT_TRUE(stroke.mixed);
  ASSERT_EQ(stroke.eligible, (std::vector<foundation::ObjectId>{id(1), id(2)}));
  const auto roots = facade.readMultiSelectionPropertyValues(0x101U, projection.stamp, stroke.eligible);
  ASSERT_TRUE(roots);
  ASSERT_EQ(roots->size(), 2U);
  EXPECT_EQ((*roots)[0].object, id(1));
  EXPECT_EQ((*roots)[0].value, semantic::PropertyValue{semantic::StrokeStyleValue{first}});
  EXPECT_EQ((*roots)[1].object, id(2));
  EXPECT_EQ((*roots)[1].value, semantic::PropertyValue{semantic::StrokeStyleValue{second}});
  EXPECT_FALSE(facade.readMultiSelectionPropertyValues(0x101U, projection.stamp, {id(1)}));
  auto staleStamp = projection.stamp;
  ++staleStamp.selectionRevision;
  EXPECT_FALSE(facade.readMultiSelectionPropertyValues(0x101U, staleStamp, stroke.eligible));
  const auto history = Access::historySize(composition.qualificationRuntime());
  ASSERT_TRUE(facade.submitMultiSelectionStrokeWidth(19, projection.stamp, stroke.eligible, 8800, 1).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto receipt = facade.multiSelectionReceipt(8800, 1);
  ASSERT_TRUE(receipt && receipt->terminal && receipt->applied);
  EXPECT_EQ(receipt->changed, stroke.eligible);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history + 1U);
  const auto check = [&](const semantic::SolidStroke& expectedFirst, const semantic::SolidStroke& expectedSecond) {
    for (const auto& pair : {std::pair{id(1), expectedFirst}, std::pair{id(2), expectedSecond}}) {
      const auto* object = composition.qualificationRuntime().qualificationObjectRecord(pair.first);
      ASSERT_NE(object, nullptr);
      const auto it = std::find_if(object->properties.entries.begin(), object->properties.entries.end(),
                                   [](const auto& value) { return value.field_id == 0x101U; });
      ASSERT_NE(it, object->properties.entries.end());
      EXPECT_EQ(it->value, semantic::PropertyValue{semantic::StrokeStyleValue{pair.second}});
      const auto fill = std::find_if(object->properties.entries.begin(), object->properties.entries.end(),
                                     [](const auto& value) { return value.field_id == 0x100U; });
      ASSERT_NE(fill, object->properties.entries.end());
      EXPECT_EQ(fill->value, (semantic::PropertyValue{semantic::FillStyleValue{semantic::SolidFill{{0, 0, 1, 1}}}}));
    }
  };
  auto changedFirst = first; changedFirst.width = 19;
  auto changedSecond = second; changedSecond.width = 19;
  check(changedFirst, changedSecond);
  const auto generation = facade.readRuntimeState().identity.runtimeGeneration;
  ASSERT_EQ(facade.undo(2, generation, 8800).state, runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  check(first, second);
  ASSERT_EQ(facade.redo(3, generation, 8800).state, runtime::ProductControlState::kQueued);
  ASSERT_TRUE(composition.ownerPump());
  check(changedFirst, changedSecond);
  EXPECT_FALSE(facade.submitMultiSelectionStrokeWidth(21, projection.stamp, stroke.eligible, 8800, 4).queued);
  const auto fresh = facade.readMultiSelection();
  const auto& currentStroke = fresh.properties.at(0x101U);
  const auto beforeRejectedHistory = Access::historySize(composition.qualificationRuntime());
  ASSERT_TRUE(facade.submitMultiSelectionStrokeWidth(-1, fresh.stamp, currentStroke.eligible, 8800, 4).queued);
  ASSERT_TRUE(composition.ownerPump());
  const auto invalid = facade.multiSelectionReceipt(8800, 4);
  ASSERT_TRUE(invalid && invalid->terminal);
  EXPECT_FALSE(invalid->applied);
  EXPECT_TRUE(invalid->changed.empty());
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeRejectedHistory);
  check(changedFirst, changedSecond);
  ASSERT_TRUE(facade.submitMultiSelectionStrokeWidth(21, fresh.stamp, currentStroke.eligible, 8800, 5).queued);
  ASSERT_TRUE(facade.setMultiSelectionPermission(runtime::MultiSelectionPermissionMode::kContextUnavailable));
  EXPECT_FALSE(facade.readMultiSelectionPropertyValues(0x101U, fresh.stamp, currentStroke.eligible));
  ASSERT_TRUE(composition.ownerPump());
  const auto rejected = facade.multiSelectionReceipt(8800, 5);
  ASSERT_TRUE(rejected && rejected->terminal);
  EXPECT_FALSE(rejected->applied);
  EXPECT_TRUE(rejected->changed.empty());
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), beforeRejectedHistory);
  check(changedFirst, changedSecond);
}

TEST_F(MultiSelectionMSE, PublicStrokeWidthRejectsNoStrokeWithoutMutation) {
  preparePartialFillFixture();
  auto& facade = composition.controlFacade();
  const auto projection = facade.readMultiSelection();
  ASSERT_TRUE(projection.available);
  const auto& stroke = projection.properties.at(0x101U);
  EXPECT_FALSE(stroke.strokeWidthAvailable);
  const auto before = composition.readback();
  const auto history = Access::historySize(composition.qualificationRuntime());
  EXPECT_FALSE(facade.submitMultiSelectionStrokeWidth(19, projection.stamp, stroke.eligible, 8801, 1).queued);
  EXPECT_EQ(composition.readback().semanticGeneration, before.semanticGeneration);
  EXPECT_EQ(Access::historySize(composition.qualificationRuntime()), history);
}
}  // namespace
