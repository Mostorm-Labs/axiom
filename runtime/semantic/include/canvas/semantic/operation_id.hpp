#pragma once

#include "canvas/foundation/object_id.hpp"

#include <compare>

namespace canvas::semantic {

// OperationId shares the Id128 physical representation but never the semantic
// namespace of ObjectId. No implicit conversion is provided in either
// direction, so idempotency identity cannot be substituted for object identity.
class OperationId final {
  public:
    constexpr OperationId() = default;
    explicit constexpr OperationId(canvas::foundation::ObjectId value) : value_(value) {}

    [[nodiscard]] constexpr const canvas::foundation::ObjectId& value() const noexcept {
        return value_;
    }
    [[nodiscard]] constexpr bool isZero() const noexcept { return value_.isZero(); }
    constexpr auto operator<=>(const OperationId&) const = default;

  private:
    canvas::foundation::ObjectId value_{};
};

} // namespace canvas::semantic
