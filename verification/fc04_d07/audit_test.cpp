// Audit only: real Runtime/History/Scene, existing test access seams, no product adapter.
#include "canvas/runtime/runtime_composition.hpp"
#include "canvas/semantic/codec.hpp"
#include "canvas/semantic/staged_object_view.hpp"

#include <algorithm>
#include <array>
#include <cstdio>
#include <gtest/gtest.h>

namespace canvas {
class IncrementalRuntimeTestAccess {
  public:
    static void fail(IncrementalRuntimeCoordinator& c, RuntimeCheckpoint p) {
        c.setCheckpointFailure(p);
    }
    static void clear(IncrementalRuntimeCoordinator& c) {
        c.clearCheckpointFailure();
    }
    static void observe(IncrementalRuntimeCoordinator& c, void (*f)(void*) noexcept, void* p) {
        c.setTestPublicationObserver(f, p);
    }
};
namespace ink_playground {
class InkPlaygroundHistoryTestAccess {
  public:
    static auto& history(interaction::CanvasEditingRuntime& r) {
        return r.history_;
    }
    static auto& coordinator(interaction::CanvasEditingRuntime& r) {
        return *r.sceneCoordinator_;
    }
    static auto& compiler(interaction::CanvasEditingRuntime& r) {
        return *r.sceneCompiler_;
    }
    static const auto& objects(const interaction::CanvasEditingRuntime& r) {
        return r.semanticObjects_;
    }
    static const auto& scene(const interaction::CanvasEditingRuntime& r) {
        return *r.runtimeSceneHost_;
    }
    static void select(interaction::CanvasEditingRuntime& r, foundation::ObjectId id) {
        (void)r.selection_.click(id);
    }
    static bool recover(interaction::CanvasEditingRuntime& r) {
        return r.recoverHistoryScene();
    }
};
} // namespace ink_playground
} // namespace canvas
namespace {
using namespace canvas;
using Access = ink_playground::InkPlaygroundHistoryTestAccess;
using Id = foundation::ObjectId;
Id id(std::uint64_t n) {
    return Id::fromUint64(n);
}
semantic::Operation op(std::uint64_t n, semantic::OperationPayload payload) {
    return {semantic::OperationId{id(n)}, semantic::DocumentId{id(1)}, 1, 1, std::move(payload)};
}
semantic::ObjectRecord shape(std::uint64_t n) {
    semantic::ObjectRecord r;
    r.id = id(n);
    r.kind = semantic::ObjectKind::kShape;
    r.kind_version = 1;
    r.content = semantic::ShapeContent{1, 20, 20};
    r.transform.tx = static_cast<double>(n * 40);
    r.placement.order_key = semantic::OrderKey({static_cast<std::uint8_t>(n)});
    return r;
}
semantic::Operation move(std::uint64_t n, bool invalid = false) {
    return op(n,
              semantic::SetTransformsOp{
                  {{id(1), {1, 0, 0, 1, 200, 30}}, {id(invalid ? 99 : 2), {1, 0, 0, 1, 250, 30}}}});
}
semantic::Operation color(std::uint64_t n, bool invalid = false) {
    return op(n,
              semantic::PatchPropertiesOp{
                  {{id(1), 3, semantic::PropertyPatchAction::kSet, semantic::PropertyValue{0.5F}},
                   {id(invalid ? 99 : 2),
                    3,
                    semantic::PropertyPatchAction::kSet,
                    semantic::PropertyValue{0.75F}}}});
}
struct Observation {
    semantic::OperationId operation{};
    std::uint64_t generation = 0, ordinal = 0, scene = 0;
    std::size_t changes = 0, history = 0;
};
struct Sink final : interaction::LocalOperationSink {
    interaction::CanvasEditingRuntime* runtime = nullptr;
    std::array<Observation, 32> events{};
    std::size_t count = 0, publications = 0;
    bool publicationCoherent = true;
    void applied(const semantic::Operation& operation,
                 const semantic::CanonicalCommitRecord& record) noexcept override {
        if (count == events.size()) {
            publicationCoherent = false;
            return;
        }
        events[count++] = {operation.id,
                           record.after_generation.value(),
                           record.commit_stamp.ordinal.value(),
                           runtime->qualificationSceneGeneration(),
                           record.change_set.objects().size(),
                           Access::history(*runtime).size()};
    }
    interaction::AxisOperationStatus
    status(const semantic::OperationId&,
           const semantic::CanonicalCommitStamp&) const noexcept override {
        return {};
    }
    static void published(void* p) noexcept {
        auto& sink = *static_cast<Sink*>(p);
        ++sink.publications;
        const auto& rt = *sink.runtime;
        sink.publicationCoherent =
            sink.publicationCoherent &&
            rt.qualificationSceneGeneration() ==
                Access::coordinator(*sink.runtime).runtimeScene().generation().value();
    }
};
struct State {
    std::vector<semantic::ObjectRecord> objects;
    std::vector<std::pair<Id, foundation::WorldRect>> bounds;
    std::vector<Id> index;
    std::uint64_t generation = 0, ordinal = 0, scene = 0, projection = 0, revision = 0;
    std::size_t history = 0, submitted = 0, events = 0, publications = 0;
    bool undo = false, redo = false;
    bool operator==(const State&) const = default;
};
class D07 : public testing::Test {
  protected:
    runtime::RuntimeComposition c;
    Sink sink;
    interaction::CanvasEditingRuntime& rt() {
        return c.qualificationRuntime();
    }
    interaction::EditorHistory& history() {
        return Access::history(rt());
    }
    State read() {
        State s;
        s.objects = Access::objects(rt()).allObjects();
        for (const auto& r : Access::coordinator(rt()).runtimeScene().records())
            s.bounds.push_back({r.objectId, r.worldBounds});
        std::sort(s.bounds.begin(), s.bounds.end(), [](const auto& a, const auto& b) {
            return a.first < b.first;
        });
        const auto q = Access::scene(rt()).query({{-1000, -1000, 1000, 1000}});
        EXPECT_TRUE(q.hasValue());
        if (q.hasValue())
            s.index = q.value().backToFront;
        s.generation = rt().semanticGeneration().value();
        s.ordinal = rt().canonicalCommitOrdinal();
        s.scene = rt().qualificationSceneGeneration();
        s.projection = Access::coordinator(rt()).runtimeScene().generation().value();
        s.revision = rt().sceneRevision();
        s.history = history().size();
        s.submitted = rt().submittedOperationCount();
        s.undo = history().canUndo();
        s.redo = history().canRedo();
        s.events = sink.count;
        s.publications = sink.publications;
        return s;
    }
    void trace(const char* phase, const State& s) {
        std::printf(
            "OBS phase=%s generation=%llu ordinal=%llu scene=%llu projection=%llu revision=%llu "
            "history=%zu undo=%d redo=%d events=%zu publications=%zu index=%zu\n",
            phase,
            static_cast<unsigned long long>(s.generation),
            static_cast<unsigned long long>(s.ordinal),
            static_cast<unsigned long long>(s.scene),
            static_cast<unsigned long long>(s.projection),
            static_cast<unsigned long long>(s.revision),
            s.history,
            s.undo,
            s.redo,
            s.events,
            s.publications,
            s.index.size());
        for (const auto& r : s.objects) {
            std::printf("record id=");
            for (const auto b : r.id.bytes)
                std::printf("%02x", static_cast<unsigned>(b));
            std::printf(" tx=%.17g ty=%.17g properties=%zu\n",
                        r.transform.tx,
                        r.transform.ty,
                        r.properties.entries.size());
        }
        for (std::size_t i = 0; i < sink.count; ++i) {
            const auto& e = sink.events[i];
            std::printf("event id=");
            for (const auto b : e.operation.value().bytes)
                std::printf("%02x", static_cast<unsigned>(b));
            std::printf(" generation=%llu ordinal=%llu callback_scene=%llu callback_history=%zu "
                        "changes=%zu\n",
                        static_cast<unsigned long long>(e.generation),
                        static_cast<unsigned long long>(e.ordinal),
                        static_cast<unsigned long long>(e.scene),
                        e.history,
                        e.changes);
        }
    }
    void SetUp() override {
        ASSERT_TRUE(c.open());
        ASSERT_TRUE(c.bindSurface(64, 64));
        ASSERT_EQ(c.applyExternalOperation(
                       op(500, semantic::InsertObjectsOp{{shape(1), shape(2), shape(3)}}),
                       semantic::ApplySource::kRemoteSync)
                      .disposition,
                  semantic::ApplyDisposition::kApplied);
        sink.runtime = &rt();
        rt().setLocalOperationSink(&sink);
        IncrementalRuntimeTestAccess::observe(Access::coordinator(rt()), &Sink::published, &sink);
    }
    void TearDown() override {
        rt().setLocalOperationSink(nullptr);
        IncrementalRuntimeTestAccess::observe(Access::coordinator(rt()), nullptr, nullptr);
        EXPECT_TRUE(c.close());
    }
    void redoBranch() {
        ASSERT_TRUE(history().executeUndoable(move(501)));
        ASSERT_TRUE(rt().undo());
        ASSERT_TRUE(history().canRedo());
    }
};

TEST_F(D07, ActualHistoryPortRejectsAllMultiOperationBatchesWithoutMutation) {
    redoBranch();
    const auto before = read();
    auto& port = static_cast<interaction::HistorySubmitPort&>(rt());
    for (const auto source :
         {semantic::ApplySource::kLocalInteraction, semantic::ApplySource::kUndoRedo}) {
        const std::array operations{move(510), color(511)};
        EXPECT_FALSE(port.submit(operations, source).accepted);
        EXPECT_EQ(read(), before);
    }
}
TEST_F(D07, HomogeneousMultiTargetValidationRejectsWholeOperationAndPreservesRedo) {
    redoBranch();
    const auto before = read();
    EXPECT_FALSE(history().executeUndoable(move(510, true)));
    EXPECT_EQ(read(), before);
    EXPECT_FALSE(history().executeUndoable(color(511, true)));
    EXPECT_EQ(read(), before);
}
TEST_F(D07, RealPrecommitAdmissionRejectionHasZeroMutationAndPreservesRedo) {
    redoBranch();
    const auto before = read();
    struct Reject final : semantic::OperationAdmission {
        bool called = false;
        bool admit(const semantic::PreparedApplyPlan& plan,
                   const semantic::StatefulValidationContext& current,
                   const semantic::StagedObjectView& after) override {
            called = true;
            EXPECT_EQ(plan.operation.kind(), semantic::OperationKind::kSetTransforms);
            EXPECT_EQ(current.objects.find(id(1))->transform.tx, 40);
            EXPECT_EQ(after.find(id(1))->transform.tx, 200);
            return false; // Actual engine admission seam, not a replacement submit port.
        }
    } reject;
    EXPECT_FALSE(static_cast<interaction::HistorySubmitPort&>(rt())
                     .submitAdmitted(move(512), semantic::ApplySource::kLocalInteraction, reject)
                     .accepted);
    EXPECT_TRUE(reject.called);
    EXPECT_EQ(read(), before);
}
TEST_F(D07, TransformBatchHasOneUndoWithChangedSelectionAndFreshRedoIdentity) {
    const auto before = read();
    trace("transform-before", before);
    Access::select(rt(), id(1));
    ASSERT_TRUE(history().executeUndoable(move(510)));
    const auto after = read();
    trace("transform-forward", after);
    ASSERT_EQ(after.history, 1U);
    ASSERT_EQ(sink.count, 1U);
    EXPECT_EQ(sink.events[0].changes, 2U);
    EXPECT_EQ(sink.events[0].scene, before.scene);
    EXPECT_EQ(sink.events[0].history, 0U);
    Access::select(rt(), id(3));
    ASSERT_TRUE(rt().undo());
    trace("transform-undo-selection-is-third-object", read());
    EXPECT_EQ(rt().selectedPrimaryObject(), id(3));
    EXPECT_EQ(read().objects, before.objects);
    EXPECT_EQ(read().bounds, before.bounds);
    EXPECT_EQ(read().index, before.index);
    ASSERT_TRUE(rt().redo());
    trace("transform-redo", read());
    EXPECT_EQ(rt().selectedPrimaryObject(), id(3));
    EXPECT_EQ(read().objects, after.objects);
    EXPECT_EQ(read().bounds, after.bounds);
    ASSERT_EQ(sink.count, 3U);
    EXPECT_NE(sink.events[0].operation, sink.events[1].operation);
    EXPECT_NE(sink.events[1].operation, sink.events[2].operation);
    EXPECT_NE(sink.events[0].operation, sink.events[2].operation);
    EXPECT_EQ(read().generation, before.generation + 3);
    EXPECT_EQ(read().ordinal, before.ordinal + 3);
    EXPECT_EQ(read().history, 1U);
    EXPECT_TRUE(read().undo);
    EXPECT_FALSE(read().redo);
}
TEST_F(D07, PropertiesBatchHasOneUndoWithChangedSelection) {
    const auto before = read();
    trace("properties-before", before);
    ASSERT_TRUE(history().executeUndoable(color(510)));
    const auto after = read();
    trace("properties-forward", after);
    EXPECT_EQ(after.history, 1U);
    EXPECT_EQ(sink.events[0].changes, 2U);
    Access::select(rt(), id(3));
    ASSERT_TRUE(rt().undo());
    EXPECT_EQ(rt().selectedPrimaryObject(), id(3));
    trace("properties-undo", read());
    EXPECT_EQ(read().objects, before.objects);
    ASSERT_TRUE(rt().redo());
    trace("properties-redo", read());
    EXPECT_EQ(read().objects, after.objects);
    EXPECT_EQ(read().history, 1U);
}
TEST_F(D07, IndependentCommandsExposeAppliedPrefixAtEveryTwoOperationRejectionBoundary) {
    // Explicit negative control: this test's loop is NOT a production IntentPlan.
    for (int fail = 0; fail < 2; ++fail) {
        const auto before = read();
        const bool first =
            history().executeUndoable(move(520 + static_cast<std::uint64_t>(fail) * 10, fail == 0));
        ASSERT_EQ(first, fail != 0);
        if (first)
            EXPECT_FALSE(history().executeUndoable(
                color(521 + static_cast<std::uint64_t>(fail) * 10, true)));
        const auto after = read();
        EXPECT_EQ(after.generation, before.generation + static_cast<unsigned>(fail));
        EXPECT_EQ(after.events, before.events + static_cast<unsigned>(fail));
        EXPECT_EQ(after.history, before.history + static_cast<unsigned>(fail));
        if (fail == 0)
            EXPECT_EQ(after, before);
        else {
            EXPECT_NE(after.objects, before.objects);
            EXPECT_EQ(after.scene, after.generation);
        }
        std::printf("negative-control boundary=%d prefix=%d generation=%llu history=%zu "
                    "observer_events=%zu\n",
                    fail,
                    fail,
                    static_cast<unsigned long long>(after.generation),
                    after.history,
                    after.events);
    }
}
TEST_F(D07, ManualCanonicalCompensationRestoresProjectionButDoesNotRewindMetadataOrEvents) {
    // Mechanism evidence only: no automatic compensation coordinator is implied.
    const auto before = read();
    ASSERT_TRUE(history().executeUndoable(move(530)));
    const auto prefix = read();
    EXPECT_EQ(prefix.generation, before.generation + 1);
    ASSERT_TRUE(rt().undo());
    const auto compensated = read();
    EXPECT_EQ(compensated.objects, before.objects);
    EXPECT_EQ(compensated.bounds, before.bounds);
    EXPECT_EQ(compensated.index, before.index);
    EXPECT_EQ(compensated.generation, before.generation + 2);
    EXPECT_EQ(compensated.ordinal, before.ordinal + 2);
    EXPECT_EQ(compensated.events, before.events + 2);
    EXPECT_FALSE(compensated.undo);
    EXPECT_TRUE(compensated.redo);
    EXPECT_EQ(compensated.history,
              1U); // This retained ordinary Undo entry is NOT a failed-intent HistoryGroup.
    std::printf("mechanism-only prefix_generation=%llu compensated_generation=%llu events=%zu "
                "retained_history=%zu\n",
                static_cast<unsigned long long>(prefix.generation),
                static_cast<unsigned long long>(compensated.generation),
                compensated.events,
                compensated.history);
}
TEST_F(D07, RealScenePublicationFaultsRetainCommittedHistoryAndRecoverCoherently) {
    for (unsigned n = 0; n < 10; ++n) {
        const auto before = read();
        const auto point = static_cast<RuntimeCheckpoint>(n);
        IncrementalRuntimeTestAccess::fail(Access::coordinator(rt()), point);
        auto command = move(550 + n);
        for (auto& item : std::get<semantic::SetTransformsOp>(command.payload).items)
            item.transform.tx += n * 10;
        const bool accepted = history().executeUndoable(command);
        IncrementalRuntimeTestAccess::clear(Access::coordinator(rt()));
        ASSERT_TRUE(accepted);
        EXPECT_EQ(rt().semanticGeneration().value(), before.generation + 1);
        EXPECT_EQ(history().size(), before.history + 1);
        EXPECT_EQ(sink.count, before.events + 1);
        EXPECT_EQ(rt().qualificationSceneGeneration(),
                  Access::coordinator(rt()).runtimeScene().generation().value());
        ASSERT_TRUE(Access::recover(rt()));
        const auto recovered = read();
        EXPECT_EQ(recovered.scene, recovered.generation);
        EXPECT_EQ(recovered.projection, recovered.generation);
        EXPECT_TRUE(sink.publicationCoherent);
        std::printf("single-operation publication_checkpoint=%u accepted=1 generation=%llu "
                    "scene=%llu events=%zu\n",
                    n,
                    static_cast<unsigned long long>(recovered.generation),
                    static_cast<unsigned long long>(recovered.scene),
                    sink.count);
    }
}
TEST_F(D07, TwoIndependentCommandsNeedTwoUndoUnits) {
    const auto before = read();
    ASSERT_TRUE(history().executeUndoable(move(580)));
    ASSERT_TRUE(history().executeUndoable(color(581)));
    EXPECT_EQ(history().size(), 2U);
    ASSERT_TRUE(rt().undo());
    EXPECT_NE(read().objects, before.objects);
    ASSERT_TRUE(rt().undo());
    EXPECT_EQ(read().objects, before.objects);
    EXPECT_FALSE(history().canUndo());
    ASSERT_TRUE(rt().redo());
    ASSERT_TRUE(rt().redo());
    EXPECT_EQ(sink.count, 6U);
}
template <class Runtime>
void submitRealExplicitIntent(Runtime& runtime, std::span<const semantic::Operation> operations) {
    if constexpr (requires {
                      runtime.beginExplicitIntent();
                      runtime.prepareExplicitIntent(operations);
                      runtime.commitExplicitIntent();
                      runtime.abortExplicitIntent();
                  }) {
        ASSERT_TRUE(runtime.beginExplicitIntent());
        ASSERT_TRUE(runtime.prepareExplicitIntent(operations));
        ASSERT_TRUE(runtime.commitExplicitIntent());
    } else {
        FAIL() << "Production CanvasEditingRuntime explicit intent entry is not implemented";
    }
}
TEST_F(D07, ExplicitIntentGroupProducesOneUndoUnit) {
    const auto before = read();
    const std::array operations{move(582), color(583)};
    ASSERT_NO_FATAL_FAILURE(submitRealExplicitIntent(rt(), operations));
    const auto after = read();
    ASSERT_EQ(after.history, before.history + 1U);
    EXPECT_EQ(after.generation, before.generation + operations.size());
    EXPECT_EQ(sink.count, operations.size());
    EXPECT_EQ(sink.publications, before.publications + 1U);
    EXPECT_TRUE(sink.publicationCoherent);
    EXPECT_EQ(sink.events[0].generation, before.generation + 1U);
    EXPECT_EQ(sink.events[1].generation, before.generation + 2U);
    EXPECT_LT(sink.events[0].ordinal, sink.events[1].ordinal);
    EXPECT_EQ(sink.events[0].scene, after.generation);
    EXPECT_EQ(sink.events[1].scene, after.generation);
    Access::select(rt(), id(3));
    ASSERT_TRUE(rt().undo());
    EXPECT_EQ(read().objects, before.objects);
    ASSERT_TRUE(rt().redo());
    EXPECT_EQ(read().objects, after.objects);
    ASSERT_EQ(sink.count, 6U);
    EXPECT_NE(sink.events[4].operation, operations[0].id);
    EXPECT_NE(sink.events[5].operation, operations[1].id);
}
TEST_F(D07, ExplicitIntentRejectsInvalidLaterOperationWithoutMutation) {
    const auto before = read();
    const std::array operations{move(584), color(585, true)};
    ASSERT_TRUE(rt().beginExplicitIntent());
    EXPECT_FALSE(rt().prepareExplicitIntent(operations));
    EXPECT_EQ(read(), before);
    ASSERT_TRUE(rt().beginExplicitIntent());
    rt().abortExplicitIntent();
    EXPECT_EQ(read(), before);
}
TEST_F(D07, ExplicitIntentRejectsStaleBasisWithoutAdditionalMutation) {
    const std::array operations{move(593), color(594)};
    ASSERT_TRUE(rt().beginExplicitIntent());
    ASSERT_TRUE(rt().prepareExplicitIntent(operations));
    ASSERT_EQ(c.applyExternalOperation(move(595), semantic::ApplySource::kRemoteSync).disposition,
              semantic::ApplyDisposition::kApplied);
    const auto changed = read();
    EXPECT_FALSE(rt().commitExplicitIntent());
    EXPECT_EQ(read(), changed);
    ASSERT_TRUE(rt().beginExplicitIntent());
    rt().abortExplicitIntent();
}
TEST_F(D07, ScenePublicationRejectsReentrantCanonicalWrite) {
    struct Attempt {
        runtime::RuntimeComposition* composition;
        semantic::Operation operation;
        bool called = false;
        semantic::ApplyDisposition disposition = semantic::ApplyDisposition::kRejected;
        static void observe(void* context) noexcept {
            auto& attempt = *static_cast<Attempt*>(context);
            if (attempt.called) return;
            attempt.called = true;
            attempt.disposition = attempt.composition
                                      ->applyExternalOperation(attempt.operation,
                                                               semantic::ApplySource::kRemoteSync)
                                      .disposition;
        }
    } attempt{&c, color(587)};
    IncrementalRuntimeTestAccess::observe(Access::coordinator(rt()), &Attempt::observe, &attempt);
    const auto before = read();
    ASSERT_TRUE(history().executeUndoable(move(586)));
    EXPECT_TRUE(attempt.called);
    EXPECT_EQ(attempt.disposition, semantic::ApplyDisposition::kRejected);
    EXPECT_EQ(rt().semanticGeneration().value(), before.generation + 1U);
    IncrementalRuntimeTestAccess::observe(Access::coordinator(rt()), &Sink::published, &sink);
}
TEST_F(D07, LiveSceneApplyIsNotUnpublishedPreparation) {
    const auto before = read();
    semantic::StagedObjectView staged(Access::objects(rt()));
    auto changed = *staged.find(id(1));
    changed.transform.tx += 10;
    ASSERT_TRUE(staged.stageReplace(changed));
    const semantic::SemanticGeneration next(before.generation + 1U);
    const auto changes = semantic::ChangeSet::fromChanges(
        semantic::SemanticGeneration(before.generation), next,
        {{id(1), semantic::SemanticChangeFlags::kTransform, {}}});
    const semantic::SemanticReadView post(staged, next);
    const SceneCommitInput input(semantic::SemanticGeneration(before.generation), next, post, &changes);
    ASSERT_TRUE(Access::coordinator(rt()).apply(Access::compiler(rt()), input).hasValue());
    EXPECT_EQ(rt().semanticGeneration().value(), before.generation);
    EXPECT_EQ(rt().qualificationSceneGeneration(), next.value());
    EXPECT_GT(sink.publications, before.publications);
}
template <class Coordinator, class Compiler, class Check>
void assertSceneIntentPreparationIsInvisible(Coordinator& coordinator, const Compiler& compiler,
                                             const SceneCommitInput& input, Check check) {
    if constexpr (requires { coordinator.prepareIntent(compiler, input); coordinator.abortIntent(); }) {
        ASSERT_TRUE(coordinator.prepareIntent(compiler, input));
        check();
        coordinator.abortIntent();
    } else {
        FAIL() << "Real IncrementalRuntimeCoordinator intent preparation is not implemented";
    }
}
TEST_F(D07, SceneIntentPrepareAndAbortAreInvisible) {
    const auto before = read();
    semantic::StagedObjectView staged(Access::objects(rt()));
    auto changed = *staged.find(id(1));
    changed.transform.tx += 10;
    ASSERT_TRUE(staged.stageReplace(changed));
    const semantic::SemanticGeneration next(before.generation + 1U);
    const auto changes = semantic::ChangeSet::fromChanges(
        semantic::SemanticGeneration(before.generation), next,
        {{id(1), semantic::SemanticChangeFlags::kTransform, {}}});
    const semantic::SemanticReadView post(staged, next);
    const SceneCommitInput input(semantic::SemanticGeneration(before.generation), next, post, &changes);
    ASSERT_NO_FATAL_FAILURE(assertSceneIntentPreparationIsInvisible(
        Access::coordinator(rt()), Access::compiler(rt()), input,
        [&] { EXPECT_EQ(read(), before); }));
    EXPECT_EQ(read(), before);
}
TEST_F(D07, CapturedExpectationsRejectUndoConflictWithoutCursorOrRedoMutation) {
    ASSERT_TRUE(history().executeUndoable(move(590)));
    ASSERT_EQ(c.applyExternalOperation(move(591), semantic::ApplySource::kRemoteSync).disposition,
              semantic::ApplyDisposition::kApplied);
    // Make the precondition observably different from the retained postimage.
    auto changed = move(592);
    std::get<semantic::SetTransformsOp>(changed.payload).items[0].transform.tx = 777;
    ASSERT_EQ(c.applyExternalOperation(changed, semantic::ApplySource::kRemoteSync).disposition,
              semantic::ApplyDisposition::kApplied);
    const auto before = read();
    EXPECT_FALSE(rt().undo());
    EXPECT_EQ(read(), before);
}
TEST_F(D07, RequiredMultiIntentEntryIsUnavailable_RED) {
    const auto before = read();
    const std::array operations{move(600), color(601)};
    const bool accepted = static_cast<interaction::HistorySubmitPort&>(rt())
                              .submit(operations, semantic::ApplySource::kLocalInteraction)
                              .accepted;
    EXPECT_EQ(read(), before);
    EXPECT_FALSE(accepted) << "The legacy span port must keep rejecting local multi-operation "
                              "submissions; grouped success is covered by the explicit intent test.";
}
TEST_F(D07, NaiveSequentialFallbackCannotSatisfyOneUndo_RED) {
    const auto before = read();
    ASSERT_TRUE(history().executeUndoable(move(610)));
    ASSERT_TRUE(history().executeUndoable(color(611)));
    ASSERT_TRUE(rt().undo());
    EXPECT_NE(read().objects, before.objects)
        << "Independent commands must not be silently grouped.";
    ASSERT_TRUE(rt().undo());
    EXPECT_EQ(read().objects, before.objects);
}
} // namespace
