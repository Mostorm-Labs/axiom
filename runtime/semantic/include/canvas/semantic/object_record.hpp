#pragma once

#include "canvas/semantic/erase_mask.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/order_key.hpp"
#include "canvas/semantic/property_value.hpp"
#include "canvas/semantic/semantic_id.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::semantic {

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

// Placement is one atomic structural semantic value. The store deliberately
// does not validate parent existence, cycle freedom, or duplicate order keys;
// those are Operation validation / PreparedApplyPlan responsibilities.
struct Placement final {
    std::optional<ObjectId> parent_id;
    OrderKey order_key{};

    bool operator==(const Placement&) const = default;
};

// Renderer-neutral affine transform value from the frozen semantic schema.
struct Transform2D final {
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double tx = 0.0;
    double ty = 0.0;

    bool operator==(const Transform2D&) const = default;
};

struct ObjectRecord final {
    ObjectId id{};
    ObjectKind kind = ObjectKind::kShape;
    std::uint32_t kind_version = 0;
    Placement placement{};
    Transform2D transform{};
    PropertyBag properties{};
    ObjectContent content{};
    std::vector<EraseMaskRecord> erase_masks;

    bool operator==(const ObjectRecord&) const = default;
};

[[nodiscard]] bool isKnownObjectKind(ObjectKind kind) noexcept;

} // namespace canvas::semantic
