#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/semantic/order_key.hpp"

#include <cstdint>

namespace canvas::semantic {

using ObjectId = canvas::foundation::ObjectId;

enum class ObjectKind : std::uint8_t {
    kShape = 1,
    kImage = 2,
    kVectorPath = 3,
    kRichText = 4,
    kVectorStroke = 5,
    kDabStroke = 6,
    kConnector = 7,
    kSticky = 8,
    kGroup = 9,
};

struct ObjectRecord final {
    ObjectId id{};
    ObjectKind kind = ObjectKind::kShape;
    OrderKey order_key{};
};

[[nodiscard]] bool isKnownObjectKind(ObjectKind kind) noexcept;

} // namespace canvas::semantic
