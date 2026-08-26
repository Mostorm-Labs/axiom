#pragma once

#include <compare>
#include <cstdint>

namespace canvas::semantic {

class SemanticRevision final {
  public:
    constexpr SemanticRevision() = default;
    explicit constexpr SemanticRevision(std::uint64_t value) : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr auto operator<=>(const SemanticRevision&) const = default;

  private:
    std::uint64_t value_ = 0;
};

} // namespace canvas::semantic
