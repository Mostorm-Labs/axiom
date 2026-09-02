#pragma once

#include "canvas/foundation/object_id.hpp"

#include <compare>

namespace canvas::semantic {

// Document identity is a distinct semantic namespace from both object and
// operation identities.  The physical representation remains the released
// sixteen-byte foundation identifier, but no implicit conversion is exposed.
class DocumentId final {
  public:
    constexpr DocumentId() = default;
    explicit constexpr DocumentId(canvas::foundation::ObjectId value) : value_(value) {}

    [[nodiscard]] constexpr const canvas::foundation::ObjectId& value() const noexcept { return value_; }
    [[nodiscard]] constexpr bool isZero() const noexcept { return value_.isZero(); }
    constexpr auto operator<=>(const DocumentId&) const = default;

  private:
    canvas::foundation::ObjectId value_{};
};

} // namespace canvas::semantic
