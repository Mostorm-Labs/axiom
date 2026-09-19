#pragma once

#include "canvas/foundation/object_id.hpp"

#include <span>
#include <vector>

namespace canvas::interaction {

struct SelectionSummary final {
    std::size_t count = 0;
    foundation::ObjectId primary{};
};

class SelectionState final {
  public:
    [[nodiscard]] std::span<const foundation::ObjectId> selected() const noexcept {
        return selected_;
    }
    [[nodiscard]] foundation::ObjectId primary() const noexcept {
        return selected_.empty() ? foundation::ObjectId{} : selected_.front();
    }
    [[nodiscard]] SelectionSummary summary() const noexcept {
        return {selected_.size(), primary()};
    }

  private:
    friend class SelectionSession;
    std::vector<foundation::ObjectId> selected_;
};

} // namespace canvas::interaction
