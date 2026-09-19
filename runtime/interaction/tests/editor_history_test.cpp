#include "canvas/interaction/editor_history.hpp"

#include <array>
#include <cassert>
#include <cstdint>
#include <span>
#include <vector>

namespace {
using canvas::foundation::ObjectId;
using canvas::interaction::EditorHistory;
using canvas::interaction::HistorySubmitPort;
using canvas::semantic::AddEraseMasksOp;
using canvas::semantic::AddStrokeOp;
using canvas::semantic::ApplySource;
using canvas::semantic::EraseMaskAddItem;
using canvas::semantic::EraseMaskRecord;
using canvas::semantic::ObjectKind;
using canvas::semantic::ObjectRecord;
using canvas::semantic::Operation;
using canvas::semantic::OperationId;
using canvas::semantic::RestoreObjectsOp;
using canvas::semantic::SetTransformsOp;
using canvas::semantic::SplitStrokesOp;
using canvas::semantic::StrokeSplit;
using canvas::semantic::Transform2D;
using canvas::semantic::TransformItem;

ObjectId id(std::uint64_t value) { return ObjectId::fromUint64(value); }
Operation operation(std::uint64_t value, canvas::semantic::OperationPayload payload) {
    Operation out;
    out.id = OperationId(id(value));
    out.document_id = canvas::semantic::DocumentId(id(900));
    out.schema_version = 1;
    out.payload_version = 1;
    out.payload = std::move(payload);
    return out;
}

struct Submit final : HistorySubmitPort {
    bool accept = true;
    std::vector<Operation> last;
    ApplySource lastSource = ApplySource::kLocalInteraction;
    int calls = 0;
    std::uint64_t nextId = 1000;

    OperationId allocateOperationId() override { return OperationId(id(nextId++)); }

    canvas::interaction::SubmitResult submit(std::span<const Operation> operations,
                                              ApplySource source) override {
        ++calls;
        last.assign(operations.begin(), operations.end());
        lastSource = source;
        return accept ? canvas::interaction::SubmitResult::acceptedResult()
                      : canvas::interaction::SubmitResult::rejected();
    }
};

void undo_redo_emits_fresh_typed_compensation_for_transform() {
    Submit submit;
    EditorHistory history(submit);
    Transform2D before;
    Transform2D after;
    after.tx = 4;
    const Operation forward = operation(10, SetTransformsOp{{TransformItem{id(1), after}}});
    assert(history.record(forward, {operation(11, SetTransformsOp{{TransformItem{id(1), before}}})}));

    assert(history.undo());
    assert(submit.calls == 1);
    assert(submit.lastSource == ApplySource::kUndoRedo);
    assert(submit.last.size() == 1);
    assert(submit.last.front().id.value() != forward.id.value());
    const auto& undo_payload = std::get<SetTransformsOp>(submit.last.front().payload);
    assert((undo_payload == SetTransformsOp{{TransformItem{id(1), before}}}));
    assert(history.canRedo());

    assert(history.redo());
    assert(submit.calls == 2);
    assert(submit.last.size() == 1);
    assert(submit.last.front().id.value() != forward.id.value());
    assert((std::get<SetTransformsOp>(submit.last.front().payload) ==
            std::get<SetTransformsOp>(forward.payload)));
    assert(!history.canRedo());
}

void rejected_compensation_is_retryable_without_cursor_advance() {
    Submit submit;
    EditorHistory history(submit);
    ObjectRecord stroke;
    stroke.id = id(2);
    stroke.kind = ObjectKind::kVectorStroke;
    const Operation forward = operation(20, AddStrokeOp{stroke});
    assert(history.record(forward, {operation(21, canvas::semantic::DeleteObjectsOp{{id(2)}})}));

    submit.accept = false;
    assert(!history.undo());
    assert(history.canUndo());
    assert(!history.canRedo());
    assert(submit.calls == 1);
}

void mask_history_preserves_typed_payload() {
    Submit submit;
    EditorHistory history(submit);
    EraseMaskRecord mask;
    mask.id = id(30);
    const Operation forward = operation(30, AddEraseMasksOp{{EraseMaskAddItem{id(3), {mask}}}});
    assert(history.record(forward, {operation(31, canvas::semantic::RemoveEraseMasksOp{{{id(3), {id(30)}}}})}));
    assert(history.undo());
    assert((std::get<canvas::semantic::RemoveEraseMasksOp>(submit.last.front().payload) ==
            canvas::semantic::RemoveEraseMasksOp{{{id(3), {id(30)}}}}));
}

void delete_and_split_preimages_submit_exact_restore_batches() {
    Submit submit;
    EditorHistory history(submit);

    ObjectRecord deleted;
    deleted.id = id(40);
    deleted.kind = ObjectKind::kShape;
    const Operation delete_forward = operation(40, canvas::semantic::DeleteObjectsOp{{id(40)}});
    assert(history.record(delete_forward, {operation(41, RestoreObjectsOp{{deleted}})}));
    assert(history.undo());
    assert(submit.last.size() == 1);
    assert((std::get<RestoreObjectsOp>(submit.last.front().payload) ==
            RestoreObjectsOp{{deleted}}));

    ObjectRecord source;
    source.id = id(50);
    source.kind = ObjectKind::kVectorStroke;
    ObjectRecord fragmentA = source;
    ObjectRecord fragmentB = source;
    fragmentA.id = id(51);
    fragmentB.id = id(52);
    const Operation split_forward = operation(
        50, SplitStrokesOp{{StrokeSplit{id(50), {fragmentA, fragmentB}}}});
    std::vector<Operation> split_inverse{
        operation(51, canvas::semantic::DeleteObjectsOp{{id(51), id(52)}}),
        operation(52, RestoreObjectsOp{{source}})};
    assert(history.record(split_forward, std::move(split_inverse)));
    assert(history.undo());
    assert(submit.last.size() == 2);
    assert((std::get<canvas::semantic::DeleteObjectsOp>(submit.last[0].payload) ==
            canvas::semantic::DeleteObjectsOp{{id(51), id(52)}}));
    assert((std::get<RestoreObjectsOp>(submit.last[1].payload) ==
            RestoreObjectsOp{{source}}));
}

void fresh_local_intention_clears_redo_and_duplicate_allocator_identity_fails_closed() {
    Submit submit;
    EditorHistory history(submit);
    Transform2D before;
    Transform2D after;
    after.ty = 9;
    assert(history.record(
        operation(60, SetTransformsOp{{TransformItem{id(6), after}}}),
        {operation(61, SetTransformsOp{{TransformItem{id(6), before}}})}));
    assert(history.undo());
    assert(history.canRedo());

    ObjectRecord stroke;
    stroke.id = id(62);
    stroke.kind = ObjectKind::kDabStroke;
    assert(history.record(operation(62, AddStrokeOp{stroke}),
                          {operation(63, canvas::semantic::DeleteObjectsOp{{id(62)}})}));
    assert(!history.canRedo());

    submit.nextId = 62;
    assert(!history.undo());
    assert(history.canUndo());
}

void every_frozen_family_redoes_the_original_typed_operation() {
    Submit submit;
    EditorHistory history(submit);

    ObjectRecord stroke;
    stroke.id = id(70);
    stroke.kind = ObjectKind::kVectorStroke;
    const Operation add = operation(70, AddStrokeOp{stroke});
    assert(history.record(add, {operation(71, canvas::semantic::DeleteObjectsOp{{id(70)}})}));

    Transform2D before;
    Transform2D after;
    after.tx = 7;
    const Operation transform =
        operation(72, SetTransformsOp{{TransformItem{id(70), after}}});
    assert(history.record(
        transform, {operation(73, SetTransformsOp{{TransformItem{id(70), before}}})}));

    const Operation deletion = operation(74, canvas::semantic::DeleteObjectsOp{{id(70)}});
    assert(history.record(deletion, {operation(75, RestoreObjectsOp{{stroke}})}));

    ObjectRecord fragment = stroke;
    fragment.id = id(76);
    const Operation split =
        operation(76, SplitStrokesOp{{StrokeSplit{id(70), {fragment}}}});
    assert(history.record(split,
                          {operation(77, canvas::semantic::DeleteObjectsOp{{id(76)}}),
                           operation(78, RestoreObjectsOp{{stroke}})}));

    EraseMaskRecord mask;
    mask.id = id(79);
    const Operation addMask =
        operation(79, AddEraseMasksOp{{EraseMaskAddItem{id(76), {mask}}}});
    assert(history.record(
        addMask,
        {operation(80, canvas::semantic::RemoveEraseMasksOp{{{id(76), {id(79)}}}})}));

    for (int index = 0; index < 5; ++index) assert(history.undo());

    const std::array<const Operation*, 5> expected{{&add, &transform, &deletion, &split, &addMask}};
    for (const Operation* item : expected) {
        assert(history.redo());
        assert(submit.last.size() == 1);
        assert(submit.last.front().payload == item->payload);
        assert(submit.last.front().id != item->id);
    }
}
} // namespace

int main() {
    undo_redo_emits_fresh_typed_compensation_for_transform();
    rejected_compensation_is_retryable_without_cursor_advance();
    mask_history_preserves_typed_payload();
    delete_and_split_preimages_submit_exact_restore_batches();
    fresh_local_intention_clears_redo_and_duplicate_allocator_identity_fails_closed();
    every_frozen_family_redoes_the_original_typed_operation();
    return 0;
}
