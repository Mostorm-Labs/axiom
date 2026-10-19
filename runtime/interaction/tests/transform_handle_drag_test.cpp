#include "canvas/interaction/transform_handle_drag.hpp"

#include <array>
#include <cassert>

namespace {
using namespace canvas;

struct Submit final : interaction::TransformSubmitPort {
    interaction::SubmitResult submit(const semantic::SetTransformsOp& operation) override {
        ++calls;
        last = operation;
        return interaction::SubmitResult::acceptedResult();
    }
    int calls = 0;
    semantic::SetTransformsOp last;
};

foundation::ObjectId id(std::uint64_t value) { return foundation::ObjectId::fromUint64(value); }

void preview_is_transient_and_commit_is_one_operation() {
    Submit submit;
    interaction::TransientSceneOverride overrideState;
    interaction::TransformHandleDrag drag(submit, overrideState);
    const std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1> target{{
        {id(1), {}}}};
    semantic::Transform2D moved{};
    moved.tx = 8.0;
    const std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1> preview{{
        {id(1), moved}}};
    assert(drag.begin(target));
    assert(drag.preview(preview));
    assert(submit.calls == 0);
    assert(overrideState.size() == 1);
    assert(drag.active());
    assert(drag.transientValueCount() == 1);
    assert(drag.commit());
    assert(submit.calls == 1);
}

void cancel_does_not_submit() {
    Submit submit;
    interaction::TransientSceneOverride overrideState;
    interaction::TransformHandleDrag drag(submit, overrideState);
    const std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1> target{{
        {id(2), {}}}};
    assert(drag.begin(target));
    drag.cancel();
    assert(!drag.commit());
    assert(submit.calls == 0);
}

void move_resize_rotate_preview_is_transient_and_commit_is_single_operation() {
    Submit submit;
    interaction::TransientSceneOverride overrideState;
    interaction::TransformHandleDrag drag(submit, overrideState);
    const std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1> target{{
        {id(3), {}}}};
    assert(drag.begin(target));
    semantic::Transform2D move{};
    move.tx = 5.0;
    assert(drag.preview(std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1>{{{id(3), move}}}));
    semantic::Transform2D resize{};
    resize.a = 2.0;
    resize.d = 3.0;
    assert(drag.preview(std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1>{{{id(3), resize}}}));
    semantic::Transform2D rotate{};
    rotate.a = 0.0;
    rotate.b = 1.0;
    rotate.c = -1.0;
    rotate.d = 0.0;
    assert(drag.preview(std::array<std::pair<foundation::ObjectId, semantic::Transform2D>, 1>{{{id(3), rotate}}}));
    assert(submit.calls == 0);
    assert(drag.commit());
    assert(submit.calls == 1);
    assert(submit.last.items.size() == 1);
    assert(submit.last.items.front().transform == rotate);
}
} // namespace

int main() {
    preview_is_transient_and_commit_is_one_operation();
    cancel_does_not_submit();
    move_resize_rotate_preview_is_transient_and_commit_is_single_operation();
    return 0;
}
