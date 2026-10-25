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

    bool operator==(const OrderKey& other) const noexcept { return bytes_ == other.bytes_; }
    auto operator<=>(const OrderKey& other) const noexcept {
        const auto count = std::min(bytes_.size(), other.bytes_.size());
        for (std::size_t index = 0U; index < count; ++index) {
            if (bytes_[index] < other.bytes_[index]) return std::strong_ordering::less;
            if (bytes_[index] > other.bytes_[index]) return std::strong_ordering::greater;
        }
        if (bytes_.size() < other.bytes_.size()) return std::strong_ordering::less;
        if (bytes_.size() > other.bytes_.size()) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

  private:
    std::vector<std::uint8_t> bytes_;
};

} // namespace canvas::semantic
