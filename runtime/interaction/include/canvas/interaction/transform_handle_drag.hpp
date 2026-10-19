#pragma once

#include "canvas/interaction/transform_session.hpp"

namespace canvas::interaction {

// Presentation-only adapter. It owns no transform semantics; all preview,
// commit, cancel, and conflict behavior remains in TransformSession.
class TransformHandleDrag final {
  public:
    TransformHandleDrag(TransformSubmitPort& submit, TransientSceneOverride& overrideState) noexcept
        : session_(submit, overrideState) {}

    [[nodiscard]] bool begin(std::span<const std::pair<foundation::ObjectId, semantic::Transform2D>> targets) {
        return session_.begin(targets);
    }
    [[nodiscard]] bool preview(std::span<const std::pair<foundation::ObjectId, semantic::Transform2D>> values) {
        return session_.preview(values);
    }
    [[nodiscard]] bool commit() { return session_.commit(); }
    void cancel() noexcept { session_.cancel(); }
    [[nodiscard]] bool active() const noexcept { return session_.active(); }
    [[nodiscard]] std::size_t transientValueCount() const noexcept {
        return session_.transientValueCount();
    }
    [[nodiscard]] TransformConflict onChangeSet(const semantic::ChangeSet& changes) noexcept {
        return session_.onChangeSet(changes);
    }

  private:
    TransformSession session_;
};

} // namespace canvas::interaction
