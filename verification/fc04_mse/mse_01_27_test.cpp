#include "canvas/runtime/runtime_composition.hpp"

#include <gtest/gtest.h>
#include <algorithm>
#include <thread>

#include "canvas/runtime/internal/multi_selection_port.hpp"

namespace canvas::ink_playground {
class InkPlaygroundHistoryTestAccess {
 public:
  static void select(interaction::CanvasEditingRuntime& runtime,
                     std::vector<foundation::ObjectId> ids) {
    ASSERT_TRUE(runtime.selection_.applyAcquisition(
        ids, interaction::SelectionCombineMode::kReplace, runtime.selection_.revision()));
  }
  static std::size_t historySize(const interaction::CanvasEditingRuntime& runtime) {
    return runtime.history_.size();
  }
};
}

namespace {
using namespace canvas;
using Access = ink_playground::InkPlaygroundHistoryTestAccess;
foundation::ObjectId id(std::uint64_t value) { return foundation::ObjectId::fromUint64(value); }

class MultiSelectionMSE : public testing::Test {
 protected:
  runtime::RuntimeComposition composition;
  void SetUp() override {
    ASSERT_TRUE(composition.open());
    ASSERT_TRUE(composition.bindSurface(800, 600));
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
      ASSERT_TRUE(composition.ownerPump());
      const auto outcome = port.receipt(7001U, 1U);
      ASSERT_TRUE(outcome);
      ASSERT_TRUE(outcome->applied);
      EXPECT_EQ(outcome->changed, (std::vector<foundation::ObjectId>{id(1), id(2), id(3)}));
      ASSERT_EQ(outcome->skipped.size(), 2U);
      EXPECT_EQ(outcome->skipped[0].object, id(4));
      EXPECT_EQ(outcome->skipped[1].object, id(5));
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

TEST_F(MultiSelectionMSE, MSE_19) {
  preparePartialFillFixture();
  verifyPartialFillCommit(composition.multiSelectionPort());
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
}  // namespace
