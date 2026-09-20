#include "canvas/interaction/interaction_runtime.hpp"

#include <cassert>
#include <cstdint>

namespace {

struct ReadPort final : canvas::interaction::SemanticReadPort {
    bool attached() const noexcept override { return attachedValue; }
    bool attachedValue = true;
};

struct QueryPort final : canvas::interaction::SceneQueryPort {
    bool available() const noexcept override { return true; }
};

struct ViewPort final : canvas::interaction::ViewStatePort {
    std::uint64_t generation() const noexcept override { return 1; }
};

struct SubmitPort final : canvas::interaction::OperationSubmitPort {
    canvas::interaction::SubmitResult submit(
        const canvas::interaction::OperationRequest&) override {
        ++calls;
        return accepted ? canvas::interaction::SubmitResult::acceptedResult()
                        : canvas::interaction::SubmitResult::rejected();
    }
    bool accepted = true;
    int calls = 0;
};

struct TransientPort final : canvas::interaction::TransientPresentationPort {
    void cancel(std::uint64_t sessionId) noexcept override { cancelledId = sessionId; }
    std::uint64_t cancelledId = 0;
};

void session_lifecycle_and_detach_cancels_active_session() {
    ReadPort read;
    QueryPort query;
    ViewPort view;
    SubmitPort submit;
    TransientPort transient;
    canvas::interaction::InteractionRuntime runtime(read, query, view, submit, transient);

    assert(runtime.startSession(7));
    assert(runtime.manager().activeCount() == 1);
    runtime.documentDetached();
    assert(runtime.manager().activeCount() == 0);
    assert(runtime.manager().lastCancellationReason() ==
           canvas::interaction::CancellationReason::kDocumentDetached);
    assert(transient.cancelledId == 7);
}

void rejected_operation_does_not_mutate_session_canonical_state() {
    ReadPort read;
    QueryPort query;
    ViewPort view;
    SubmitPort submit;
    TransientPort transient;
    submit.accepted = false;
    canvas::interaction::InteractionRuntime runtime(read, query, view, submit, transient);

    assert(runtime.startSession(9));
    assert(!runtime.commit(9, canvas::interaction::OperationRequest{42}));
    assert(runtime.manager().activeCount() == 1);
    assert(submit.calls == 1);
}

} // namespace

int main() {
    session_lifecycle_and_detach_cancels_active_session();
    rejected_operation_does_not_mutate_session_canonical_state();
    TransientPort transient;
    canvas::interaction::InteractionSessionManager keyed(transient);
    const canvas::input::PointerKey first{2, 1, 1};
    const canvas::input::PointerKey second{2, 2, 1};
    assert(keyed.start(first, 101));
    assert(keyed.start(second, 102));
    assert(keyed.keyedActiveCount() == 2);
    assert(keyed.cancel(first, 101));
    assert(transient.cancelledId == 101);
    assert(keyed.finish(second, 102));
    assert(keyed.keyedActiveCount() == 0);
    assert(keyed.start(first, 103));
    assert(keyed.start(second, 104));
    keyed.cancelAllKeyed(canvas::interaction::CancellationReason::kSourceLost);
    assert(keyed.keyedActiveCount() == 0);
    assert(keyed.lastCancellationReason() == canvas::interaction::CancellationReason::kSourceLost);
    return 0;
}
