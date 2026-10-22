// Protocol reference model. No Semantic store/Operation/History/layout is faked
// into production: the counters below belong only to an explicit fake commit port.
#include "text_input_contract.hpp"

#include <algorithm>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

namespace axiom::fc02::draft {
namespace {
void require(bool condition, const char* message) {
    if (!condition) throw std::runtime_error(message);
}
std::optional<std::uint32_t> scalarOffset(const std::u16string& text, std::size_t offset) {
    if (offset > text.size()) return std::nullopt;
    std::uint32_t scalars = 0;
    std::optional<std::uint32_t> mapped;
    for (std::size_t index = 0; index < text.size();) {
        if (index == offset) mapped = scalars;
        const auto first = text[index++];
        if (first >= 0xD800 && first <= 0xDBFF) {
            if (index >= text.size() || text[index] < 0xDC00 || text[index] > 0xDFFF) return std::nullopt;
            ++index;
        } else if (first >= 0xDC00 && first <= 0xDFFF) return std::nullopt;
        ++scalars;
    }
    return offset == text.size() ? std::optional{scalars} : mapped;
}
bool rangeValid(const std::u16string& text, Utf16Range range) {
    const auto end = static_cast<std::uint64_t>(range.start) + range.length;
    return end <= text.size() && scalarOffset(text, range.start).has_value() &&
           scalarOffset(text, static_cast<std::size_t>(end)).has_value();
}
struct FakeCommitPort {
    std::size_t attempts = 0, accepted = 0, history = 0;
};
class ProtocolModel final : public TextIngress {
public:
    explicit ProtocolModel(FakeCommitPort& port, std::u16string initial = {}) : port_(port) {
        state_.target = {{7, Id::fromUint64(10), 2, 3, 4, 1}, Id::fromUint64(11), 1};
        state_.target.object.bytes[15] = 0xAA; // Exercise high 64 bits.
        state_.projection = std::move(initial);
        state_.phase = Phase::kEditing;
    }
    EditorSnapshot read() const override { return state_; }
    InputReceipt submit(const InputEvent& event) override {
        if (event.target.session != state_.target.session || event.target.object != state_.target.object)
            return {InputStatus::kStale, event.sequence};
        for (const auto& entry : journal_) {
            if (entry.first.sequence != event.sequence) continue;
            if (entry.first != event) return {InputStatus::kCollision, event.sequence};
            auto receipt = entry.second;
            receipt.replay = true;
            return receipt;
        }
        if (event.sequence <= acknowledged_) return {InputStatus::kExpired, event.sequence};
        if (state_.phase == Phase::kClosed) return {InputStatus::kStale, event.sequence};
        if (event.sequence != next_) return {InputStatus::kOutOfOrder, event.sequence};
        if (journal_.size() == journalCapacity) return {InputStatus::kBackpressure, event.sequence};
        if (pending_) return {InputStatus::kBusy, event.sequence};
        InputStatus status;
        if (terminal_ && event.kind == InputKind::kCommit && event.composition == terminal_->composition) {
            auto comparable = event;
            comparable.sequence = terminal_->sequence;
            status = comparable == *terminal_ ? InputStatus::kAlreadyTerminal : InputStatus::kCollision;
        } else if (event.target.revision != state_.target.revision) status = InputStatus::kStale;
        else status = process(event);
        journal_.push_back({event, {status, event.sequence}});
        ++next_;
        return journal_.back().second;
    }
    bool exit(ExitReason reason, std::uint64_t request) {
        if (state_.phase == Phase::kClosed || request == 0) return false;
        if (exitRequest_ != 0) return exitRequest_ == request && exitReason_ == reason;
        exitRequest_ = request;
        exitReason_ = reason;
        if (pending_) return true;
        if (!composition_) { close(); return true; }
        state_.phase = Phase::kFinishing;
        state_.finishComposition = HostEffect{state_.target.session, ++effectSequence_};
        return true;
    }
    bool finishFailed(std::uint64_t effect) {
        if (!state_.finishComposition || state_.finishComposition->id != effect) return false;
        state_.finishComposition.reset();
        exitRequest_ = 0;
        state_.phase = Phase::kComposing;
        return true;
    }
    bool complete(bool accepted) {
        if (!pending_) return false;
        const auto edit = *pending_;
        pending_.reset();
        if (!pendingFromRetry_) for (auto& entry : journal_) if (entry.first.sequence == edit.sequence)
            entry.second.status = accepted ? InputStatus::kApplied : InputStatus::kRejected;
        pendingFromRetry_ = false;
        if (!accepted) { failed_ = edit; state_.phase = Phase::kCommitFailed; return true; }
        failed_.reset();
        state_.projection.replace(edit.range.start, edit.range.length, edit.text);
        ++state_.target.revision;
        ++port_.accepted;
        ++port_.history;
        composition_.reset();
        state_.preedit.reset();
        state_.finishComposition.reset();
        state_.phase = Phase::kEditing;
        if (exitRequest_ != 0) close();
        return true;
    }
    bool retryKnownRejection() {
        if (!failed_ || pending_ || state_.phase != Phase::kCommitFailed) return false;
        // Simulates a DISTINCT explicit retry intent, not a rewrite of the
        // original input's terminal Rejected receipt.
        pending_ = failed_;
        pendingFromRetry_ = true;
        ++port_.attempts;
        state_.phase = Phase::kApplying;
        return true;
    }
    void forcedDetach() {
        // Models a queued edit cancelled BEFORE apply, never rollback of accepted state.
        pending_.reset(); failed_.reset(); composition_.reset();
        pendingFromRetry_ = false;
        state_.preedit.reset();
        state_.finishComposition.reset();
        state_.phase = Phase::kClosed;
        ++state_.target.session.documentAttachment;
    }
    void contentChanged(bool targetChanged) {
        if (!targetChanged) return;
        ++state_.target.revision;
        pending_.reset(); failed_.reset(); composition_.reset();
        pendingFromRetry_ = false;
        state_.preedit.reset();
        state_.finishComposition.reset(); exitRequest_ = 0;
        state_.phase = Phase::kEditing;
    }
    void geometryChanged() { ++geometry_; }
    bool geometryCurrent(std::uint64_t revision) const { return revision == geometry_; }
    bool enter(bool consumedByIme, bool control) {
        if (consumedByIme || composition_) return false;
        if (control) return false; // Paragraph translation belongs to the future editor resolver.
        return exit(ExitReason::kEnter, 99);
    }
    bool acknowledge(std::uint64_t through) {
        if (through >= next_ || (pending_ && through >= pending_->sequence) ||
            (failed_ && through >= failed_->sequence)) return false;
        if (through <= acknowledged_) return true;
        acknowledged_ = through;
        std::erase_if(journal_, [through](const auto& entry) { return entry.first.sequence <= through; });
        return true;
    }
    std::size_t completedExits() const { return exits_; }
    static constexpr std::size_t journalCapacity = 8;
private:
    InputStatus process(const InputEvent& event) {
        if (!rangeValid(state_.projection, event.range) || !scalarOffset(event.text, event.text.size()))
            return InputStatus::kInvalidOffset;
        if (event.preeditSelection &&
            ((event.kind != InputKind::kUpdate && event.kind != InputKind::kCommit) ||
             !rangeValid(event.text, *event.preeditSelection))) return InputStatus::kInvalidOffset;
        if (event.kind != InputKind::kCommit && event.finishEffect != 0) return InputStatus::kRejected;
        switch (event.kind) {
        case InputKind::kBegin:
            if (composition_ || event.composition == 0 || event.composition <= lastComposition_ ||
                exitRequest_ != 0 || !event.text.empty())
                return InputStatus::kRejected;
            composition_ = event;
            state_.preedit = CompositionProjection{event.composition, event.range, {}, std::nullopt};
            lastComposition_ = event.composition;
            state_.phase = Phase::kComposing;
            return InputStatus::kObserved;
        case InputKind::kUpdate:
        case InputKind::kCommit:
        case InputKind::kCancel:
            if (!composition_ || event.composition != composition_->composition || event.range != composition_->range)
                return InputStatus::kRejected;
            if (event.kind == InputKind::kUpdate) {
                state_.preedit = CompositionProjection{event.composition, event.range, event.text, event.preeditSelection};
                return InputStatus::kObserved;
            }
            if (event.kind == InputKind::kCancel) {
                composition_.reset();
                state_.preedit.reset();
                state_.phase = Phase::kEditing;
                if (exitRequest_ != 0) { // Forced completion cannot silently become successful cancellation.
                    exitRequest_ = 0; state_.finishComposition.reset();
                }
                return InputStatus::kObserved;
            }
            if (state_.finishComposition ? event.finishEffect != state_.finishComposition->id : event.finishEffect != 0)
                return InputStatus::kRejected;
            state_.preedit = CompositionProjection{event.composition, event.range, event.text, event.preeditSelection};
            terminal_ = event;
            break;
        case InputKind::kReplace:
            if (composition_ || exitRequest_ != 0 || event.composition != 0) return InputStatus::kRejected;
            break;
        case InputKind::kSelection:
            return event.composition == 0 && event.text.empty() ? InputStatus::kObserved : InputStatus::kRejected;
        default: return InputStatus::kRejected;
        }
        if (state_.projection.substr(event.range.start, event.range.length) == event.text) {
            composition_.reset();
            state_.preedit.reset();
            state_.finishComposition.reset();
            state_.phase = Phase::kEditing;
            if (exitRequest_ != 0) close();
            return InputStatus::kObserved;
        }
        pending_ = event;
        pendingFromRetry_ = false;
        ++port_.attempts;
        state_.phase = Phase::kApplying;
        return InputStatus::kPending;
    }
    void close() { state_.phase = Phase::kClosed; ++exits_; }
    FakeCommitPort& port_;
    EditorSnapshot state_;
    std::optional<InputEvent> composition_, terminal_, pending_, failed_;
    std::vector<std::pair<InputEvent, InputReceipt>> journal_;
    std::uint64_t next_ = 1, acknowledged_ = 0, lastComposition_ = 0;
    std::uint64_t exitRequest_ = 0, effectSequence_ = 0, geometry_ = 1;
    std::size_t exits_ = 0;
    ExitReason exitReason_ = ExitReason::kEnter;
    bool pendingFromRetry_ = false;
};
class FakeIME {
public:
    explicit FakeIME(TextIngress& ingress) : ingress_(ingress), target_(ingress.read().target) {}
    InputEvent event(InputKind kind, std::u16string text = {}, Utf16Range range = {},
                     std::uint64_t composition = 0, std::uint64_t finish = 0) {
        if (composition == 0 && kind != InputKind::kReplace && kind != InputKind::kSelection) composition = 1;
        return {target_, next_++, composition, finish, kind, range, std::move(text), std::nullopt};
    }
    InputReceipt send(InputKind kind, std::u16string text = {}, Utf16Range range = {},
                      std::uint64_t composition = 0, std::uint64_t finish = 0) {
        return ingress_.submit(event(kind, std::move(text), range, composition, finish));
    }
private:
    TextIngress& ingress_;
    ProjectionToken target_;
    std::uint64_t next_ = 1;
};
} // namespace
} // namespace axiom::fc02::draft

int main() {
    using namespace axiom::fc02::draft;
    std::size_t passed = 0;
    const auto test = [&](const char* name, auto body) {
        try { body(); ++passed; std::cout << "PASS " << name << '\n'; }
        catch (const std::exception& error) { std::cerr << "FAIL " << name << ": " << error.what() << '\n'; throw; }
    };
    try {
        test("updates_are_transient", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            require(ime.send(InputKind::kBegin).status == InputStatus::kObserved, "begin");
            for (const auto* value : {u"n", u"ni", u"\u4F60"})
                require(ime.send(InputKind::kUpdate, value).status == InputStatus::kObserved, "update");
            require(p.attempts == 0 && m.read().projection.empty(), "transient wrote confirmed text");
            require(ime.send(InputKind::kCommit, u"\u4F60").status == InputStatus::kPending, "commit queued");
            require(p.accepted == 0, "queue receipt claimed apply");
            require(m.complete(true) && p.accepted == 1 && p.history == 1, "fake receipt");
        });
        test("duplicate_terminal_and_payload_collision", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            auto e = ime.event(InputKind::kCommit, u"X");
            require(m.submit(e).status == InputStatus::kPending, "queued");
            require(m.submit(e).replay && p.attempts == 1, "duplicate queued again");
            require(m.complete(true), "apply");
            require(m.submit(e).status == InputStatus::kApplied && m.submit(e).replay, "lost final receipt");
            e.text = u"Y";
            require(m.submit(e).status == InputStatus::kCollision, "collision accepted");
            require(ime.send(InputKind::kCommit, u"X").status == InputStatus::kAlreadyTerminal && p.accepted == 1, "late terminal duplicated");
        });
        test("utf16_scalar_and_malformed_payload", [] {
            const std::u16string text = u"A\U0001F600B";
            require(scalarOffset(text, 3) == 2U && !scalarOffset(text, 2), "surrogate mapping");
            FakeCommitPort p; ProtocolModel m(p, text); FakeIME ime(m);
            require(ime.send(InputKind::kReplace, u"X", {2, 0}).status == InputStatus::kInvalidOffset, "split surrogate");
            require(ime.send(InputKind::kReplace, std::u16string(1, 0xD800)).status == InputStatus::kInvalidOffset, "unpaired surrogate");
            require(p.attempts == 0, "invalid payload reached commit");
            require(ime.send(InputKind::kReplace, u"X", {3, 0}).status == InputStatus::kPending, "valid scalar boundary");
        });
        test("tool_switch_waits_for_finish_and_apply", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            require(m.exit(ExitReason::kToolSwitch, 17), "exit request");
            const auto effect = m.read().finishComposition->id;
            require(m.exit(ExitReason::kToolSwitch, 17) && m.read().finishComposition->id == effect, "effect repeated");
            require(m.completedExits() == 0, "premature tool change");
            require(ime.send(InputKind::kCommit, u"X", {}, 1, effect).status == InputStatus::kPending, "finish callback");
            require(m.completedExits() == 0 && p.accepted == 0, "pending is not success");
            require(m.complete(true) && m.completedExits() == 1 && p.history == 1, "exit barrier");
        });
        test("rejected_commit_keeps_exit_blocked_and_can_retry", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            (void)m.exit(ExitReason::kPageExit, 18);
            const auto effect = m.read().finishComposition->id;
            const auto original = ime.event(InputKind::kCommit, u"X", {}, 1, effect);
            (void)m.submit(original);
            require(m.complete(false) && m.completedExits() == 0 && p.history == 0, "rejection exited");
            require(m.read().preedit->text == u"X", "rejected final input lost");
            require(m.retryKnownRejection() && m.complete(true) && m.completedExits() == 1 && p.accepted == 1, "retry");
            require(m.submit(original).status == InputStatus::kRejected && m.submit(original).replay,
                    "explicit retry rewrote original rejection");
        });
        test("finish_failure_and_wrong_effect", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin); (void)m.exit(ExitReason::kDebugFocus, 19);
            const auto effect = m.read().finishComposition->id;
            require(ime.send(InputKind::kCommit, u"X", {}, 1, effect + 1).status == InputStatus::kRejected, "wrong effect");
            require(!m.finishFailed(effect + 1) && m.finishFailed(effect) && m.completedExits() == 0 && p.attempts == 0, "finish failure");
        });
        test("resize_preserves_session_stale_geometry_rejected", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            const auto token = m.read().target;
            m.geometryChanged();
            require(m.read().target == token && !m.geometryCurrent(1), "geometry replaced session");
            require(ime.send(InputKind::kReplace, u"X").status == InputStatus::kPending, "resize lost edit");
        });
        test("stale_attachment_and_full_id_rejected", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            auto e = ime.event(InputKind::kReplace, u"X");
            auto wrong = e; wrong.target.object.bytes[15] ^= 1;
            require(m.submit(wrong).status == InputStatus::kStale, "ID truncated");
            m.forcedDetach();
            require(m.submit(e).status == InputStatus::kStale && p.attempts == 0, "late callback crossed detach");
        });
        test("remote_target_invalidates_projection_disjoint_does_not", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin); m.contentChanged(false);
            require(ime.send(InputKind::kUpdate, u"X").status == InputStatus::kObserved, "disjoint cancelled");
            m.contentChanged(true);
            require(ime.send(InputKind::kCommit, u"X").status == InputStatus::kStale && p.attempts == 0, "stale projection committed");
        });
        test("ordered_delivery_and_bounded_journal", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            auto first = ime.event(InputKind::kSelection); auto gap = first; gap.sequence = 3;
            require(m.submit(gap).status == InputStatus::kOutOfOrder, "gap accepted");
            require(m.submit(first).status == InputStatus::kObserved, "expected event");
            for (std::size_t i = 1; i < ProtocolModel::journalCapacity; ++i) (void)ime.send(InputKind::kSelection);
            auto terminal = ime.event(InputKind::kReplace, u"X");
            require(m.submit(terminal).status == InputStatus::kBackpressure && p.attempts == 0, "queue silently dropped");
            require(m.acknowledge(ProtocolModel::journalCapacity), "ack");
            require(m.submit(terminal).status == InputStatus::kPending, "terminal not retryable");
            require(m.submit(first).status == InputStatus::kExpired, "expired event reapplied");
            require(!m.acknowledge(terminal.sequence), "pending receipt evicted");
        });
        test("escape_cancel_and_ime_enter_do_not_commit", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            require(!m.enter(true, false) && m.read().phase == Phase::kComposing, "IME Enter exited");
            require(ime.send(InputKind::kCancel).status == InputStatus::kObserved && p.attempts == 0, "cancel committed");
            require(!m.enter(false, true) && m.enter(false, false) && m.completedExits() == 1, "Enter rule");
        });
        test("detach_before_apply_and_after_apply_are_distinct", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kReplace, u"X"); m.forcedDetach();
            require(!m.complete(true) && p.accepted == 0, "unapplied work escaped detach");
            FakeCommitPort q; ProtocolModel n(q); FakeIME other(n);
            (void)other.send(InputKind::kReplace, u"Y"); require(n.complete(true), "apply"); n.forcedDetach();
            require(q.accepted == 1 && q.history == 1, "detach rolled back accepted work");
        });
        test("empty_draft_and_identical_replacement_emit_no_commit", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            require(ime.send(InputKind::kCommit).status == InputStatus::kObserved && p.attempts == 0, "empty draft inserted");
            require(m.enter(false, false) && p.history == 0, "empty exit wrote history");
            FakeCommitPort q; ProtocolModel n(q, u"AB"); FakeIME other(n);
            require(other.send(InputKind::kReplace, u"AB", {0, 2}).status == InputStatus::kObserved && q.attempts == 0, "identical replacement wrote");
        });
        test("busy_commit_retains_retryable_input_sequence", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kReplace, u"X");
            auto selection = ime.event(InputKind::kSelection);
            require(m.submit(selection).status == InputStatus::kBusy, "pending accepted another event");
            require(m.complete(true), "apply");
            selection.target = m.read().target;
            require(m.submit(selection).status == InputStatus::kObserved, "busy event consumed sequence");
        });
        test("composition_reuse_and_exit_identity_collision", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            require(m.exit(ExitReason::kToolSwitch, 42), "exit");
            require(!m.exit(ExitReason::kPageExit, 42), "same exit ID changed payload");
            const auto effect = m.read().finishComposition->id;
            require(m.finishFailed(effect), "failure");
            require(ime.send(InputKind::kCommit, u"X", {}, 1, effect).status == InputStatus::kRejected, "late failed finish committed");
            (void)ime.send(InputKind::kCancel);
            require(ime.send(InputKind::kBegin).status == InputStatus::kRejected, "composition ID reused");
            require(ime.send(InputKind::kBegin, {}, {}, 2).status == InputStatus::kObserved, "new composition unavailable");
            require(p.attempts == 0, "rejected event reached fake apply");
        });
        test("preedit_selection_has_its_own_utf16_coordinates", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            (void)ime.send(InputKind::kBegin);
            auto invalid = ime.event(InputKind::kUpdate, u"\u4F60\U0001F600");
            invalid.preeditSelection = Utf16Range{2, 0};
            require(m.submit(invalid).status == InputStatus::kInvalidOffset, "preedit split surrogate");
            auto update = ime.event(InputKind::kUpdate, u"\u4F60\U0001F600");
            update.preeditSelection = Utf16Range{3, 0};
            require(m.submit(update).status == InputStatus::kObserved, "valid preedit selection");
            const auto before = m.read(); m.geometryChanged();
            require(m.read().preedit->text == before.preedit->text && m.read().preedit->selection == before.preedit->selection,
                    "resize lost preedit");
            require(m.read().projection.empty() && p.attempts == 0, "preedit became confirmed text");
        });
        test("unknown_event_kind_is_rejected", [] {
            FakeCommitPort p; ProtocolModel m(p); FakeIME ime(m);
            auto event = ime.event(InputKind::kReplace, u"X");
            event.kind = static_cast<InputKind>(255);
            require(m.submit(event).status == InputStatus::kRejected && p.attempts == 0, "unknown kind committed");
        });
    } catch (const std::exception&) { return 1; }
    std::cout << "PROTOCOL_MODEL_PASS " << passed << '/' << passed
              << "; production_runtime=NOT_CONNECTED; semantic_v2=NOT_IMPLEMENTED\n";
    return 0;
}
