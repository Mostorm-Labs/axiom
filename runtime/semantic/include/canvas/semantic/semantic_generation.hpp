#pragma once

#include <compare>
#include <cstdint>

namespace canvas::semantic {

// Runtime-local canonical post-state token. A SemanticDocument establishes a
// baseline at create/restore and advances this only for a true Applied
// Operation. It is deliberately not a server revision, cache key, or bridge
// commit stamp.
class SemanticGeneration final {
  public:
    constexpr SemanticGeneration() = default;
    explicit constexpr SemanticGeneration(std::uint64_t value) : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr auto operator<=>(const SemanticGeneration&) const = default;

  private:
    std::uint64_t value_ = 0;
};

} // namespace canvas::semantic
