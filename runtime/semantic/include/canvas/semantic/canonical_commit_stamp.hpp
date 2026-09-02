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
    explicit constexpr RuntimeEpoch(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr auto operator<=>(const RuntimeEpoch&) const = default;

  private:
    std::uint64_t value_ = 0;
};

// Per-runtime-epoch canonical commit sequence token. Zero is reserved and
// therefore cannot describe a true canonical commit.
class CommitOrdinal final {
  public:
    constexpr CommitOrdinal() = default;
    explicit constexpr CommitOrdinal(std::uint64_t value) noexcept : value_(value) {}

    [[nodiscard]] constexpr std::uint64_t value() const noexcept { return value_; }
    constexpr auto operator<=>(const CommitOrdinal&) const = default;

  private:
    std::uint64_t value_ = 0;
};

struct CanonicalCommitStamp final {
    RuntimeEpoch runtime_epoch{};
    CommitOrdinal ordinal{};

    constexpr auto operator<=>(const CanonicalCommitStamp&) const = default;
};

} // namespace canvas::semantic
