#include "canvas/interaction/editor_history.hpp"

#include <algorithm>
#include <new>

namespace canvas::interaction {

bool historyExpectationsMatch(const semantic::ObjectStore& store,
                               std::span<const HistoryExpectation> expectations) noexcept {
    for (const auto& expected : expectations) {
        const auto* current = store.find(expected.objectId);
        if (!expected.record) { if (current != nullptr) return false; continue; }
        if (!current || current->kind != expected.record->kind ||
            current->kind_version != expected.record->kind_version) return false;
        const auto& record = *expected.record;
        switch (expected.aspect) {
            case HistoryAspect::kRichTextContent: if (current->content != record.content) return false; break;
            case HistoryAspect::kWholeRecord: if (*current != record) return false; break;
            case HistoryAspect::kTransform: if (current->transform != record.transform) return false; break;
            case HistoryAspect::kPlacement: if (current->placement != record.placement) return false; break;
            case HistoryAspect::kSize: {
                const auto* a=std::get_if<semantic::ShapeContent>(&current->content);
                const auto* b=std::get_if<semantic::ShapeContent>(&record.content);
                if (!a || !b || a->width!=b->width || a->height!=b->height) return false;
                break;
            }
            case HistoryAspect::kProperties:
                for (const auto field:expected.fields) {
                    const auto find=[field](const semantic::PropertyBag& bag)->const semantic::PropertyValue* {
                        for (const auto& entry:bag.entries) if(entry.field_id==field)return &entry.value;
                        return nullptr;
                    };
                    const auto* a=find(current->properties);const auto* b=find(record.properties);
                    if ((a==nullptr)!=(b==nullptr) || (a && *a!=*b)) return false;
                }
                break;
        }
    }
    return true;
}

namespace {
bool supportedForward(const semantic::Operation& operation) noexcept {
    using semantic::OperationKind;
    switch (operation.kind()) {
        case OperationKind::kAddStroke:
        case OperationKind::kInsertObjects:
        case OperationKind::kSetTransforms:
        case OperationKind::kDeleteObjects:
        case OperationKind::kSplitStrokes:
        case OperationKind::kAddEraseMasks:
        case OperationKind::kSetPlacements:
        case OperationKind::kPatchProperties:
        case OperationKind::kSetObjectSize:
        case OperationKind::kEditRichText:
            return true;
        default:
            return false;
    }
}

bool supportedInverse(const semantic::Operation& operation) noexcept {
    using semantic::OperationKind;
    switch (operation.kind()) {
        case OperationKind::kDeleteObjects:
        case OperationKind::kRestoreObjects:
        case OperationKind::kSetTransforms:
        case OperationKind::kRemoveEraseMasks:
        case OperationKind::kSetPlacements:
        case OperationKind::kPatchProperties:
        case OperationKind::kSetObjectSize:
        case OperationKind::kEditRichText:
            return true;
        default:
            return false;
    }
}
} // namespace

bool EditorHistory::record(semantic::Operation forward,
                           std::vector<semantic::Operation> inverse) {
    return recordConditional(std::move(forward),std::move(inverse),{},{});
}

bool EditorHistory::recordConditional(semantic::Operation forward,
    std::vector<semantic::Operation> inverse, std::vector<HistoryExpectation> before,
    std::vector<HistoryExpectation> after) {
    auto prepared=prepareRecord(forward,inverse,std::move(before),std::move(after));
    if(!prepared) return false;
    commitPrepared(std::move(*prepared));return true;
}
std::optional<EditorHistory::PreparedEntry> EditorHistory::prepareRecord(
    const semantic::Operation& forward,const std::vector<semantic::Operation>& inverse,
    std::vector<HistoryExpectation> before,std::vector<HistoryExpectation> after) {
    if (!supportedForward(forward) || inverse.empty() || forward.id.isZero()) {
        return std::nullopt;
    }
    for (const auto& item : inverse) {
        if (!supportedInverse(item) || item.document_id != forward.document_id || item.id.isZero()) {
            return std::nullopt;
        }
    }
    try {
        entries_.reserve(cursor_+1);
        return PreparedEntry{forward,inverse,std::move(before),std::move(after)};
    } catch(const std::bad_alloc&) { return std::nullopt; }
}
void EditorHistory::commitPrepared(PreparedEntry&& entry) noexcept {
    entries_.resize(cursor_);
    entries_.push_back(std::move(entry));
    cursor_ = entries_.size();
}

bool EditorHistory::identityKnown(const semantic::OperationId& id) const noexcept {
    if (std::find(emittedOperationIds_.begin(), emittedOperationIds_.end(), id) !=
        emittedOperationIds_.end()) {
        return true;
    }
    for (const auto& entry : entries_) {
        if (entry.forward.id == id) return true;
        for (const auto& inverse : entry.inverse) {
            if (inverse.id == id) return true;
        }
    }
    return false;
}

bool EditorHistory::submitFresh(std::span<const semantic::Operation> operations,
                               std::span<const HistoryExpectation> expectations) {
    std::vector<semantic::Operation> fresh(operations.begin(), operations.end());
    std::vector<semantic::OperationId> allocated;
    allocated.reserve(fresh.size());
    for (auto& operation : fresh) {
        const auto id = submit_.allocateOperationId();
        if (id.isZero() || identityKnown(id) ||
            std::find(allocated.begin(), allocated.end(), id) != allocated.end()) {
            return false;
        }
        operation.id = id;
        allocated.push_back(id);
    }
    emittedOperationIds_.reserve(emittedOperationIds_.size()+allocated.size());
    if (!submit_.submitConditional(fresh, semantic::ApplySource::kUndoRedo, expectations).accepted) return false;
    emittedOperationIds_.insert(emittedOperationIds_.end(), allocated.begin(), allocated.end());
    return true;
}

bool EditorHistory::undo() {
    if (!canUndo() || !submitFresh(entries_[cursor_ - 1].inverse, entries_[cursor_ - 1].after)) return false;
    --cursor_;
    return true;
}

bool EditorHistory::redo() {
    if (!canRedo()) return false;
    auto forward = entries_[cursor_].forward;
    if (const auto* insert=std::get_if<semantic::InsertObjectsOp>(&forward.payload); insert &&
        std::all_of(insert->objects.begin(),insert->objects.end(),[](const auto& object){return object.kind==semantic::ObjectKind::kRichText;}))
        forward.payload=semantic::RestoreObjectsOp{insert->objects};
    if (!submitFresh(std::span<const semantic::Operation>(&forward, 1), entries_[cursor_].before)) return false;
    ++cursor_;
    return true;
}

} // namespace canvas::interaction
