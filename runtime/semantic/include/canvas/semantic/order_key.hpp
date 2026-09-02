#pragma once

#include <compare>
#include <cstddef>
#include <cstdint>
#include <span>
#include <utility>
#include <vector>

namespace canvas::semantic {

class OrderKey final {
  public:
    static constexpr std::size_t kMinSize = 1;
    static constexpr std::size_t kMaxSize = 32;

    OrderKey() = default;
    explicit OrderKey(std::vector<std::uint8_t> bytes) : bytes_(std::move(bytes)) {}

    [[nodiscard]] bool isValid() const noexcept {
        return bytes_.size() >= kMinSize && bytes_.size() <= kMaxSize && bytes_.back() != 0U;
    }
    [[nodiscard]] std::span<const std::uint8_t> bytes() const noexcept { return bytes_; }

    auto operator<=>(const OrderKey&) const = default;
    bool operator==(const OrderKey&) const = default;

  private:
    std::vector<std::uint8_t> bytes_;
};

} // namespace canvas::semantic
