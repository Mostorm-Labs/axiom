#pragma once

#include <compare>
#include <array>
#include <cstddef>
#include <span>
#include <vector>
#include <cstdint>

namespace canvas::foundation {

class StableOrderKey final {
  public:
    constexpr StableOrderKey() = default;
    explicit constexpr StableOrderKey(std::uint64_t value) {
        for (std::size_t i = 0; i < 8; ++i) {
            _bytes[i] = static_cast<std::uint8_t>((value >> (56U - i * 8U)) & 0xffU);
        }
        _size = 8;
    }
    explicit StableOrderKey(std::span<const std::uint8_t> bytes) noexcept {
        _size = static_cast<std::uint8_t>(bytes.size() > _bytes.size() ? _bytes.size() : bytes.size());
        for (std::size_t i = 0; i < _size; ++i) _bytes[i] = bytes[i];
    }

    [[nodiscard]] constexpr std::uint64_t value() const {
        std::uint64_t value = 0;
        const std::size_t count = _size > 8 ? 8U : _size;
        for (std::size_t i = 0; i < count; ++i) value = (value << 8U) | _bytes[i];
        return value;
    }
    [[nodiscard]] constexpr std::span<const std::uint8_t> bytes() const noexcept {
        return std::span<const std::uint8_t>(_bytes.data(), _size);
    }
    friend constexpr bool operator==(const StableOrderKey& left,
                                     const StableOrderKey& right) noexcept {
        if (left._size != right._size) return false;
        for (std::size_t i = 0; i < left._size; ++i) {
            if (left._bytes[i] != right._bytes[i]) return false;
        }
        return true;
    }
    friend constexpr std::strong_ordering operator<=>(const StableOrderKey& left,
                                                       const StableOrderKey& right) noexcept {
        const auto common = left._size < right._size ? left._size : right._size;
        for (std::size_t i = 0; i < common; ++i) {
            if (left._bytes[i] < right._bytes[i]) return std::strong_ordering::less;
            if (left._bytes[i] > right._bytes[i]) return std::strong_ordering::greater;
        }
        if (left._size < right._size) return std::strong_ordering::less;
        if (left._size > right._size) return std::strong_ordering::greater;
        return std::strong_ordering::equal;
    }

  private:
    std::array<std::uint8_t, 32> _bytes{};
    std::uint8_t _size = 0;
};

} // namespace canvas::foundation
