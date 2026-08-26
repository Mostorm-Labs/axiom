#pragma once

#include <compare>
#include <cstdint>

namespace canvas::semantic {

// RuntimeEpoch and CanonicalCommitStamp are bridge-facing canonical-order
// evidence. Their type identity remains distinct from SemanticGeneration even
// when a future runtime happens to advance both counters together.
class RuntimeEpoch final {
  public:
    constexpr RuntimeEpoch() = default;
    explicit constexpr RuntimeEpoch(std::uint64_t value) : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr auto operator<=>(const RuntimeEpoch&) const = default;

  private:
    std::uint64_t value_ = 0;
};

struct CanonicalCommitStamp final {
    RuntimeEpoch runtime_epoch{};
    std::uint64_t ordinal = 0;

    constexpr auto operator<=>(const CanonicalCommitStamp&) const = default;
};

} // namespace canvas::semantic
