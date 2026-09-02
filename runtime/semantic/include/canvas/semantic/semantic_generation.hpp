#pragma once

#include <compare>
#include <cstdint>
#include <limits>

namespace canvas::semantic {

class OperationEngine;

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

// Mutable runtime-local generation owned by one loaded semantic document.
// Ordinary consumers can observe the current token, while only the canonical
// OperationEngine write lane can prepare and commit a successor.
class SemanticGenerationState final {
  public:
    explicit constexpr SemanticGenerationState(
        SemanticGeneration baseline = SemanticGeneration{}) noexcept
        : current_(baseline) {}

    [[nodiscard]] constexpr SemanticGeneration current() const noexcept {
        return current_;
    }

  private:
    friend class OperationEngine;

    [[nodiscard]] constexpr bool prepareSuccessor(
        SemanticGeneration& out) const noexcept {
        if (current_.value() == std::numeric_limits<std::uint64_t>::max()) {
            return false;
        }
        out = SemanticGeneration(current_.value() + 1U);
        return true;
    }

    constexpr void commitSuccessor(SemanticGeneration successor) noexcept {
        current_ = successor;
    }

    SemanticGeneration current_{};
};

} // namespace canvas::semantic
