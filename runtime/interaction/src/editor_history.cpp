#include "canvas/interaction/editor_history.hpp"

#include <algorithm>

namespace canvas::interaction {

namespace {
bool supportedForward(const semantic::Operation& operation) noexcept {
    using semantic::OperationKind;
    switch (operation.kind()) {
        case OperationKind::kAddStroke:
        case OperationKind::kSetTransforms:
        case OperationKind::kDeleteObjects:
        case OperationKind::kSplitStrokes:
        case OperationKind::kAddEraseMasks:
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
            return true;
        default:
            return false;
    }
}
} // namespace

bool EditorHistory::record(semantic::Operation forward,
                           std::vector<semantic::Operation> inverse) {
    if (!supportedForward(forward) || inverse.empty() || forward.id.isZero()) {
        return false;
    }
    for (const auto& item : inverse) {
        if (!supportedInverse(item) || item.document_id != forward.document_id || item.id.isZero()) {
            return false;
        }
    }
    entries_.resize(cursor_);
    entries_.push_back({std::move(forward), std::move(inverse)});
    cursor_ = entries_.size();
    return true;
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

bool EditorHistory::submitFresh(std::span<const semantic::Operation> operations) {
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
    if (!submit_.submit(fresh, semantic::ApplySource::kUndoRedo).accepted) return false;
    emittedOperationIds_.insert(emittedOperationIds_.end(), allocated.begin(), allocated.end());
    return true;
}

bool EditorHistory::undo() {
    if (!canUndo() || !submitFresh(entries_[cursor_ - 1].inverse)) return false;
    --cursor_;
    return true;
}

bool EditorHistory::redo() {
    if (!canRedo()) return false;
    const auto& forward = entries_[cursor_].forward;
    if (!submitFresh(std::span<const semantic::Operation>(&forward, 1))) return false;
    ++cursor_;
    return true;
}

} // namespace canvas::interaction
