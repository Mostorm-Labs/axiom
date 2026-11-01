#include "canvas/interaction/text_edit_session.hpp"
#include "canvas/runtime/canvas_runtime_facade.hpp"
#include "multi_selection_queue.hpp"

#include <algorithm>
#include <array>

namespace canvas::runtime {
namespace {
bool scope(TextSessionToken a, TextSessionToken b) {
    a.projection = b.projection = 0;
    return a == b;
}
TextReceipt receipt(TextStatus status, std::uint64_t id, TextSessionToken token = {}) {
    TextReceipt r;
    r.status = status;
    r.request = id;
    r.token = token;
    return r;
}
} // namespace
TextReceipt CanvasRuntimeFacade::submitTextIntent(const TextIntent& request) {
    if (!ownerThread() || request.clientId == 0 || request.requestId == 0)
        return receipt(TextStatus::kRejected, request.requestId);
    std::scoped_lock lock(ingressMutex_);
    for (const auto& entry : textIntents_)
        if (entry.first.clientId == request.clientId &&
            entry.first.requestId == request.requestId) {
            if (entry.first != request)
                return receipt(TextStatus::kCollision, request.requestId);
            auto r = entry.second;
            if (runtime_.closed() && r.status == TextStatus::kQueued)
                r.status = TextStatus::kRejected;
            r.replay = true;
            return r;
        }
    const auto high = textClientHighWater_.find(request.clientId);
    if (high != textClientHighWater_.end() && request.requestId <= high->second)
        return receipt(TextStatus::kExpired, request.requestId);
    if (!isTargetCurrent(request.target))
        return receipt(TextStatus::kStale, request.requestId);
    if (pendingWork_.load() >= 256 ||
        (high == textClientHighWater_.end() && textClientHighWater_.size() >= 1024))
        return receipt(TextStatus::kBackpressure, request.requestId);
    if (textIntents_.size() >= 1024) {
        if (textIntents_.front().second.status == TextStatus::kQueued)
            return receipt(TextStatus::kBackpressure, request.requestId);
        textIntents_.pop_front();
    }
    auto r = receipt(TextStatus::kQueued, request.requestId, request.token);
    textIntents_.push_back({request, r});
    textClientHighWater_[request.clientId] = request.requestId;
    ingress_.push_back(request);
    ++pendingWork_;
    return r;
}
TextReceipt CanvasRuntimeFacade::submitTextInput(const TextInputEvent& event) {
    if (!ownerThread() || event.sequence == 0 || event.text.size() > 65536)
        return receipt(TextStatus::kRejected, event.sequence, event.token);
    std::scoped_lock lock(ingressMutex_);
    for (const auto& entry : textInputs_)
        if (scope(entry.first.token, event.token) && entry.first.sequence == event.sequence) {
            if (entry.first != event)
                return receipt(TextStatus::kCollision, event.sequence, event.token);
            auto r = entry.second;
            if (runtime_.closed() && r.status == TextStatus::kQueued)
                r.status = TextStatus::kRejected;
            r.replay = true;
            return r;
        }
    if (runtime_.closed())
        return receipt(TextStatus::kStale, event.sequence, event.token);
    if (pendingWork_.load() >= 256)
        return receipt(TextStatus::kBackpressure, event.sequence, event.token);
    if (textInputs_.size() >= 1024) {
        if (textInputs_.front().second.status == TextStatus::kQueued)
            return receipt(TextStatus::kBackpressure, event.sequence, event.token);
        textInputs_.pop_front();
    }
    auto r = receipt(TextStatus::kQueued, event.sequence, event.token);
    textInputs_.push_back({event, r});
    ingress_.push_back(event);
    ++pendingWork_;
    return r;
}
TextEditorSnapshot CanvasRuntimeFacade::readTextEditor() const {
    if (!ownerThread() || !runtime_.interaction_ || !runtime_.interaction_->textSession())
        return {};
    return runtime_.interaction_->textSession()->read();
}
TextGeometrySnapshot CanvasRuntimeFacade::readTextGeometry(const std::optional<TextGeometryToken>& expected) const {
    if(!ownerThread() || runtime_.closed())return {};
    auto geometry=runtime_.textGeometry(target());if(expected && *expected!=geometry.token)geometry.available=false;return geometry;
}
std::optional<TextReceipt> CanvasRuntimeFacade::textInputReceipt(const TextSessionToken& token,
                                                                 std::uint64_t sequence) const {
    if (!ownerThread())
        return std::nullopt;
    std::scoped_lock lock(ingressMutex_);
    for (const auto& entry : textInputs_)
        if (scope(entry.first.token, token) && entry.first.sequence == sequence) {
            auto result = entry.second;
            if (runtime_.closed() && result.status == TextStatus::kQueued)
                result.status = TextStatus::kRejected;
            return result;
        }
    return std::nullopt;
}
std::optional<TextReceipt> CanvasRuntimeFacade::textIntentReceipt(std::uint64_t client,
                                                                  std::uint64_t request) const {
    if (!ownerThread())
        return std::nullopt;
    std::scoped_lock lock(ingressMutex_);
    for (const auto& entry : textIntents_)
        if (entry.first.clientId == client && entry.first.requestId == request) {
            auto result = entry.second;
            if (runtime_.closed() && result.status == TextStatus::kQueued)
                result.status = TextStatus::kRejected;
            return result;
        }
    return std::nullopt;
}
bool CanvasRuntimeFacade::acknowledgeTextInput(const TextSessionToken& token,
                                               std::uint64_t through) {
    if (!ownerThread() || !runtime_.interaction_ || !runtime_.interaction_->textSession())
        return false;
    std::scoped_lock lock(ingressMutex_);
    for (const auto& entry : textInputs_)
        if (scope(entry.first.token, token) && entry.first.sequence <= through &&
            entry.second.status == TextStatus::kQueued)
            return false;
    if (!runtime_.interaction_->textSession()->acknowledge(token, through))
        return false;
    std::erase_if(textInputs_, [&](const auto& entry) {
        return scope(entry.first.token, token) && entry.first.sequence <= through;
    });
    return true;
}
void CanvasRuntimeFacade::finishTextReceipts() {
    const auto snapshot = readTextEditor();
    std::scoped_lock lock(ingressMutex_);
    for (auto& entry : textIntents_)
        if (entry.second.status == TextStatus::kQueued &&
            (entry.first.kind == TextIntentKind::kFinish || entry.first.kind == TextIntentKind::kCompleteComposition) &&
            scope(entry.first.token, snapshot.token) &&
            snapshot.exitStatus != TextStatus::kQueued) {
            entry.second.status = snapshot.exitStatus;
            entry.second.token = snapshot.token;
            if (snapshot.exitStatus == TextStatus::kApplied) {
                for (auto input = textInputs_.rbegin(); input != textInputs_.rend(); ++input) {
                    if (input->first.token == entry.first.token &&
                        input->first.kind == TextInputKind::kCommit && input->first.finishEffect &&
                        (input->second.status == TextStatus::kApplied ||
                         input->second.status == TextStatus::kObserved)) {
                        entry.second.operation = input->second.operation;
                        entry.second.semanticGeneration = input->second.semanticGeneration;
                        break;
                    }
                }
            }
        }
}
std::size_t CanvasRuntimeFacade::processIngress(std::uint64_t sequence) {
    auto* text = runtime_.interaction_ ? runtime_.interaction_->textSession() : nullptr;
    std::deque<Work> work;
    {
        std::scoped_lock lock(ingressMutex_);
        work.swap(ingress_);
    }
    std::size_t count = 0;
    const auto flush = [&] {
        while (!deferredControls_.empty()) {
            const auto& request = deferredControls_.front();
            const CanvasControlReceiptKey key{request.clientId, request.requestId};
            const auto existing = service_->receipt(key);
            if (existing && existing->state == CanvasControlReceiptState::kQueued) {
                const auto state = text ? text->read() : TextEditorSnapshot{};
                if (state.exitStatus == TextStatus::kQueued)
                    return;
                if (state.exitStatus == TextStatus::kRejected)
                    service_->rejectPending(key, CanvasControlError::kBusy);
                else
                    count += service_->processPending(sequence, key);
            }
            deferredControls_.pop_front();
            --pendingWork_;
        }
    };
    flush();
    for (auto& item : work) {
        flush();
        if (auto* request = std::get_if<CanvasControlRequest>(&item)) {
            const CanvasControlReceiptKey key{request->clientId, request->requestId};
            if (!deferredControls_.empty()) {
                deferredControls_.push_back(*request);
                continue;
            }
            const auto current = service_->receipt(key);
            if (!current || current->state != CanvasControlReceiptState::kQueued) {
                --pendingWork_;
                continue;
            }
            const auto state = text ? text->read() : TextEditorSnapshot{};
            if (state.phase != TextPhase::kClosed &&
                (request->payload.kind == CanvasControlPayloadKind::kSelectTool ||
                 request->payload.kind == CanvasControlPayloadKind::kSelectCreationPreset||
                 request->payload.kind == CanvasControlPayloadKind::kSelectionAcquisitionMode)) {
                const auto finish = text->finish();
                if (finish == TextStatus::kQueued) {
                    deferredControls_.push_back(*request);
                    continue;
                }
                if (finish == TextStatus::kRejected) {
                    service_->rejectPending(key, CanvasControlError::kBusy);
                    --pendingWork_;
                    continue;
                }
            }
            if (state.composition && (request->payload.kind == CanvasControlPayloadKind::kUndo ||
                                      request->payload.kind == CanvasControlPayloadKind::kRedo))
                service_->rejectPending(key, CanvasControlError::kBusy);
            else
                count += service_->processPending(sequence, key);
        } else if (auto* intent = std::get_if<TextIntent>(&item)) {
            const bool editAtPoint = intent->kind == TextIntentKind::kEditAtPoint;
            bool editAllowed=intent->kind!=TextIntentKind::kEditAtPoint || runtime_.textTool() || (runtime_.selectionMode() && runtime_.selectionAcquisitionMode()==CanvasSelectionMode::kPoint);
            foundation::ObjectId editSelectionRoot{};
            bool enteredGroup=false;
            // The native double-click fact is shared with text editing. In
            // Select mode the first double-click on a grouped descendant enters
            // that Group; a subsequent double-click can edit the actual child.
            if (editAtPoint && editAllowed && runtime_.selectionMode() && isTargetCurrent(intent->target) &&
                text && text->read().phase==TextPhase::kClosed && !runtime_.selectionTransformActive() &&
                !runtime_.creationActive() && !runtime_.imageCropPresentation_) {
                enteredGroup=runtime_.enterGroupAtViewPoint(intent->viewX,intent->viewY);
            }
            if (editAtPoint && editAllowed && !enteredGroup) {
                intent->object = runtime_.hitTextAtView(intent->viewX, intent->viewY);
                if (runtime_.selectionMode()) {
                    editSelectionRoot = runtime_.resolveSelectionRoot(intent->object);
                    if (editSelectionRoot.isZero()) editAllowed = false;
                }
                (void)runtime_.tableTargetAt(*intent);
                intent->kind = TextIntentKind::kEdit;
            }
            const auto result = enteredGroup ? receipt(TextStatus::kObserved,intent->requestId) : !editAllowed ? receipt(TextStatus::kRejected,intent->requestId) : !isTargetCurrent(intent->target)
                                    ? receipt(TextStatus::kStale, intent->requestId)
                                : text ? text->intent(*intent)
                                       : receipt(TextStatus::kUnsupported, intent->requestId);
            if (editAtPoint && !enteredGroup && result.status == TextStatus::kApplied && text) {
                if (!editSelectionRoot.isZero()) {
                    const std::array roots{editSelectionRoot};
                    (void)runtime_.selection_.applyAcquisition(
                        roots, interaction::SelectionCombineMode::kReplace,
                        runtime_.selection_.revision());
                }
                if (const auto record = runtime_.textPresentation()) {
                    if (const auto hit = runtime_.textHit(*record, intent->viewX, intent->viewY)) {
                        const auto state = text->read();
                        TextInputEvent selection;
                        selection.token = state.token;
                        selection.sequence = state.nextInputSequence;
                        selection.kind = TextInputKind::kSelection;
                        selection.range = {*hit, 0};
                        (void)text->input(selection);
                    }
                }
            }
            {
                std::scoped_lock lock(ingressMutex_);
                for (auto& entry : textIntents_)
                    if (entry.first.clientId == intent->clientId &&
                        entry.first.requestId == intent->requestId) {
                        entry.second = result;
                        break;
                    }
            }
            ++count;
        } else if (auto* multi = std::get_if<std::shared_ptr<MultiSelectionControlWork>>(&item)) {
            const auto key = (*multi)->key;
            const auto current = service_->receipt(key);
            if (current && current->state == CanvasControlReceiptState::kQueued) {
                const auto state = text ? text->read() : TextEditorSnapshot{};
                // A pending TextEdit/IME or earlier deferred control cannot be
                // overtaken by this edit. Reject without mutating either owner.
                if (!deferredControls_.empty() || state.phase != TextPhase::kClosed)
                    service_->rejectPending(key, CanvasControlError::kBusy);
                else
                    count += service_->processPending(sequence, key);
            }
        } else {
            const auto& event = std::get<TextInputEvent>(item);
            const auto result =
                text ? text->input(event)
                     : receipt(TextStatus::kUnsupported, event.sequence, event.token);
            {
                std::scoped_lock lock(ingressMutex_);
                for (auto& entry : textInputs_)
                    if (scope(entry.first.token, event.token) &&
                        entry.first.sequence == event.sequence) {
                        entry.second = result;
                        break;
                    }
            }
            ++count;
        }
        runtime_.canonicalPresentationDirty_=true;
        --pendingWork_;
        finishTextReceipts();
    }
    flush();
    finishTextReceipts();
    return count;
}
} // namespace canvas::runtime
