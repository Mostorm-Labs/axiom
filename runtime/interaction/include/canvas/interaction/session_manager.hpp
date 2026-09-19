#pragma once

#include "canvas/interaction/interaction_ports.hpp"

#include <cstddef>
#include <cstdint>

namespace canvas::interaction {

class InteractionSessionManager final {
  public:
    explicit InteractionSessionManager(TransientPresentationPort& transient) noexcept;
    [[nodiscard]] bool start(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool finish(std::uint64_t sessionId) noexcept;
    [[nodiscard]] bool cancel(std::uint64_t sessionId) noexcept;
    void cancelAll() noexcept;
    [[nodiscard]] std::size_t activeCount() const noexcept { return activeCount_; }

  private:
    TransientPresentationPort& transient_;
    std::uint64_t activeSession_ = 0;
    std::size_t activeCount_ = 0;
};

} // namespace canvas::interaction
