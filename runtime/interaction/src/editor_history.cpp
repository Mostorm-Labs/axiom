#include "canvas/interaction/editor_history.hpp"

#include <algorithm>
#include <new>
#include <type_traits>
#include "undoable_command.hpp"
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#include "qualification_checkpoint.hpp"
#endif

namespace canvas::interaction {

bool historyExpectationsMatch(const semantic::ObjectStore& store,
                               std::span<const HistoryExpectation> expectations) noexcept {
    for (const auto& expected : expectations) {
        if (expected.aspect == HistoryAspect::kDocumentBackground) {
            if (store.documentBackground() != expected.background) return false;
            continue;
        }
        const auto* current = store.find(expected.objectId);
        if (!expected.record) { if (current != nullptr) return false; continue; }
        if (!current || current->kind != expected.record->kind ||
            current->kind_version != expected.record->kind_version) return false;
        const auto& record = *expected.record;
        switch (expected.aspect) {
            case HistoryAspect::kDocumentBackground: break;
            case HistoryAspect::kTopology:
                if (current->placement != record.placement || current->transform != record.transform) return false;
                break;
            case HistoryAspect::kRichTextContent: if (current->content != record.content) return false; break;
            case HistoryAspect::kWholeRecord: if (*current != record) return false; break;
            case HistoryAspect::kTransform: if (current->transform != record.transform) return false; break;
            case HistoryAspect::kPlacement: if (current->placement != record.placement) return false; break;
            case HistoryAspect::kSize: {
                const auto* a=std::get_if<semantic::ShapeContent>(&current->content);
                const auto* b=std::get_if<semantic::ShapeContent>(&record.content);
                if (a != nullptr && b != nullptr) {
                    if (a->width != b->width || a->height != b->height) return false;
                    break;
                }
                const auto* imageA = std::get_if<semantic::ImageContent>(&current->content);
                const auto* imageB = std::get_if<semantic::ImageContent>(&record.content);
                if (imageA != nullptr && imageB != nullptr) {
                    if (imageA->width != imageB->width || imageA->height != imageB->height)
                        return false;
                    break;
                }
                const auto* stickyA = std::get_if<semantic::StickyContent>(&current->content);
                const auto* stickyB = std::get_if<semantic::StickyContent>(&record.content);
                if (stickyA == nullptr || stickyB == nullptr ||
                    stickyA->width != stickyB->width || stickyA->height != stickyB->height)
                    return false;
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
        case OperationKind::kSetImageContent:
        case OperationKind::kEditTable:
        case OperationKind::kEditRichText:
        case OperationKind::kSetConnectorContent:
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
        case OperationKind::kSetImageContent:
        case OperationKind::kEditTable:
        case OperationKind::kEditRichText:
        case OperationKind::kSetConnectorContent:
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
    return prepareValidatedRecord(forward, inverse, std::move(before), std::move(after), false);
}
std::optional<EditorHistory::PreparedEntry> EditorHistory::prepareValidatedRecord(
    const semantic::Operation& forward, const std::vector<semantic::Operation>& inverse,
    std::vector<HistoryExpectation> before, std::vector<HistoryExpectation> after, bool admitted) {
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    if (canvas::qualification::observing(fc08Qualification_)) return std::nullopt;
    canvas::qualification::FaultScope fc08Scope{fc08Qualification_.get()};
#endif
    if (!(admitted ? supportsUndoableCommand(forward.kind()) : supportedForward(forward)) || inverse.empty() || forward.id.isZero()) {
        return std::nullopt;
    }
    for (const auto& item : inverse) {
        if (!(admitted ? supportsUndoableCommand(item.kind()) : supportedInverse(item)) || item.document_id != forward.document_id || item.id.isZero()) {
            return std::nullopt;
        }
    }
    try {
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
        if (canvas::qualification::historyCheckpoint(fc08Qualification_, this,
            canvas::qualification::Point::kHistBeforeReserve, forward, entries_.size(), cursor_)) return std::nullopt;
#endif
        entries_.reserve(cursor_+1);
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
        PreparedEntry fc08Prepared{forward,inverse,std::move(before),std::move(after),admitted,{}};
        if (canvas::qualification::historyCheckpoint(fc08Qualification_, this,
            canvas::qualification::Point::kHistReady, forward, entries_.size(), cursor_)) return std::nullopt;
        return fc08Prepared;
#else
        return PreparedEntry{forward,inverse,std::move(before),std::move(after),admitted,{}};
#endif
    } catch(const std::bad_alloc&) { return std::nullopt; }
}
std::optional<EditorHistory::PreparedEntry> EditorHistory::prepareIntentRecord(
    std::vector<semantic::Operation> forwards, std::vector<semantic::Operation> inverses,
    std::vector<HistoryExpectation> before, std::vector<HistoryExpectation> after) try {
    if (forwards.empty() || inverses.empty()) return std::nullopt;
    for (const auto& op : forwards)
        if (!supportsUndoableCommand(op.kind()) || op.id.isZero() ||
            op.document_id != forwards.front().document_id) return std::nullopt;
    for (const auto& op : inverses)
        if (!supportsUndoableCommand(op.kind()) || op.id.isZero() ||
            op.document_id != forwards.front().document_id) return std::nullopt;
    entries_.reserve(cursor_ + 1U);
    auto first = forwards.front();
    return PreparedEntry{std::move(first), std::move(inverses), std::move(before),
                         std::move(after), true, std::move(forwards)};
} catch (const std::bad_alloc&) { return std::nullopt; }

void EditorHistory::commitPrepared(PreparedEntry&& entry) noexcept {
    static_assert(std::is_nothrow_move_constructible_v<PreparedEntry>);
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    if (canvas::qualification::observing(fc08Qualification_)) return;
#endif
    entries_.resize(cursor_);
    entries_.push_back(std::move(entry));
    cursor_ = entries_.size();
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    canvas::qualification::historyTransition(fc08Qualification_, this,
        entries_.back().forward.id, entries_.back().forward.document_id, entries_.size(), cursor_);
#endif
}

class HistoryCommandAdmission final : public semantic::OperationAdmission {
  public:
    explicit HistoryCommandAdmission(EditorHistory* history,
        std::span<const HistoryExpectation> expected = {}) : history_(history), expected_(expected) {}
    bool admit(const semantic::PreparedApplyPlan& plan,
        const semantic::StatefulValidationContext& context,
        const semantic::StagedObjectView& after) override {
        if (called_ || (!expected_.empty() &&
            (!historyExpectationsMatch(context.objects, expected_) || !historyFootprintMatches(plan, expected_)))) return false;
        called_ = true;
        auto command = prepareUndoableCommand(plan, context, after);
        if (!command) return false;
        if (history_) {
            entry = history_->prepareValidatedRecord(command->forward, {command->inverse},
                std::move(command->before), std::move(command->after), true);
            return entry.has_value();
        }
        return true;
    }
    std::optional<EditorHistory::PreparedEntry> entry;
  private:
    EditorHistory* history_;
    std::span<const HistoryExpectation> expected_;
    bool called_ = false;
};
bool EditorHistory::executeUndoable(const semantic::Operation& forward) try {
    HistoryCommandAdmission admission(this);
    if (!submit_.submitAdmitted(forward, semantic::ApplySource::kLocalInteraction, admission).accepted) return false;
    if (admission.entry) commitPrepared(std::move(*admission.entry));
    submit_.publishAdmitted();
    return true;
} catch (const std::bad_alloc&) { return false; }

bool EditorHistory::identityKnown(const semantic::OperationId& id) const noexcept {
    if (std::find(emittedOperationIds_.begin(), emittedOperationIds_.end(), id) !=
        emittedOperationIds_.end()) {
        return true;
    }
    for (const auto& entry : entries_) {
        if (entry.forward.id == id) return true;
        for (const auto& forward : entry.intentForwards) {
            if (forward.id == id) return true;
        }
        for (const auto& inverse : entry.inverse) {
            if (inverse.id == id) return true;
        }
    }
    return false;
}

bool EditorHistory::submitFresh(std::span<const semantic::Operation> operations,
                               std::span<const HistoryExpectation> expectations, bool admissionRequired) try {
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
    if (admissionRequired) {
        if (fresh.size() != 1U) return false;
        HistoryCommandAdmission admission(nullptr, expectations);
        if (!submit_.submitAdmitted(fresh.front(), semantic::ApplySource::kUndoRedo, admission).accepted) return false;
    } else if (!submit_.submitConditional(fresh, semantic::ApplySource::kUndoRedo, expectations).accepted) return false;
    emittedOperationIds_.insert(emittedOperationIds_.end(), allocated.begin(), allocated.end());
    return true;
} catch (const std::bad_alloc&) { return false; }

bool EditorHistory::submitFreshIntent(std::span<const semantic::Operation> operations,
                                      std::span<const HistoryExpectation> expectations,
                                      bool redo) try {
    if (!intentSubmit_ || operations.empty()) return false;
    std::vector<semantic::Operation> fresh(operations.begin(), operations.end());
    std::vector<semantic::OperationId> allocated;
    allocated.reserve(fresh.size());
    for (auto& operation : fresh) {
        const auto id = submit_.allocateOperationId();
        if (id.isZero() || identityKnown(id) ||
            std::find(allocated.begin(), allocated.end(), id) != allocated.end()) return false;
        operation.id = id;
        allocated.push_back(id);
    }
    emittedOperationIds_.reserve(emittedOperationIds_.size() + allocated.size());
    struct Commit final {
        EditorHistory* history;
        const std::vector<semantic::OperationId>* ids;
        bool redo;
        static void apply(void* context) noexcept {
            auto& self = *static_cast<Commit*>(context);
            self.history->emittedOperationIds_.insert(self.history->emittedOperationIds_.end(),
                                                       self.ids->begin(), self.ids->end());
            if (self.redo) ++self.history->cursor_; else --self.history->cursor_;
        }
    } commit{this, &allocated, redo};
    return intentSubmit_(intentContext_, fresh, expectations, &Commit::apply, &commit);
} catch (const std::bad_alloc&) { return false; }

bool EditorHistory::undo() {
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    if (canvas::qualification::observing(fc08Qualification_)) return false;
    canvas::qualification::FaultScope fc08Scope{fc08Qualification_.get()};
#endif
    if (!canUndo()) return false;
    if (!entries_[cursor_ - 1].intentForwards.empty())
        return submitFreshIntent(entries_[cursor_ - 1].inverse, entries_[cursor_ - 1].after, false);
    if (!submitFresh(entries_[cursor_ - 1].inverse, entries_[cursor_ - 1].after, entries_[cursor_ - 1].admissionRequired)) return false;
    --cursor_;
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    canvas::qualification::historyTransition(fc08Qualification_, this,
        emittedOperationIds_.back(), entries_[cursor_].forward.document_id, entries_.size(), cursor_);
#endif
    if (entries_[cursor_].admissionRequired) submit_.publishAdmitted();
    return true;
}

bool EditorHistory::redo() try {
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    if (canvas::qualification::observing(fc08Qualification_)) return false;
    canvas::qualification::FaultScope fc08Scope{fc08Qualification_.get()};
#endif
    if (!canRedo()) return false;
    if (!entries_[cursor_].intentForwards.empty())
        return submitFreshIntent(entries_[cursor_].intentForwards, entries_[cursor_].before, true);
    auto forward = entries_[cursor_].forward;
    if (const auto* insert=std::get_if<semantic::InsertObjectsOp>(&forward.payload); insert &&
        std::all_of(insert->objects.begin(),insert->objects.end(),[](const auto& object){return object.kind==semantic::ObjectKind::kRichText || object.kind==semantic::ObjectKind::kImage;}))
        forward.payload=semantic::RestoreObjectsOp{insert->objects};
    if (!submitFresh(std::span<const semantic::Operation>(&forward, 1), entries_[cursor_].before, entries_[cursor_].admissionRequired)) return false;
    ++cursor_;
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    canvas::qualification::historyTransition(fc08Qualification_, this,
        emittedOperationIds_.back(), entries_[cursor_ - 1].forward.document_id, entries_.size(), cursor_);
#endif
    if (entries_[cursor_ - 1].admissionRequired) submit_.publishAdmitted();
    return true;
} catch (const std::bad_alloc&) { return false; }

} // namespace canvas::interaction
