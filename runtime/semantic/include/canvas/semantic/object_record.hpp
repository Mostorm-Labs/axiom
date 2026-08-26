#pragma once

#include "canvas/foundation/object_id.hpp"
#include "canvas/semantic/order_key.hpp"

#include <cstdint>
#include <optional>
#include <vector>

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

// A semantic leaf value is owned by the domain record, never by a protobuf
// DTO. GT-G1-03 preserves its value and equality only; field-/kind-specific
// validation and mutation semantics begin in GT-G1-04.
struct SemanticValue final {
    std::vector<std::uint8_t> value;

    bool operator==(const SemanticValue&) const = default;
};

struct PropertyValue final {
    SemanticValue semantic_value{};

    bool operator==(const PropertyValue&) const = default;
};

struct PropertyEntry final {
    std::uint32_t field_id = 0;
    PropertyValue value{};

    bool operator==(const PropertyEntry&) const = default;
};

struct PropertyBag final {
    std::vector<PropertyEntry> entries;

    bool operator==(const PropertyBag&) const = default;
};

// ObjectContent carries the released ObjectKind branch and its semantic leaf
// value. It intentionally contains no renderer, scene, storage, or protobuf
// object; future validation establishes branch/kind compatibility.
struct ObjectContent final {
    ObjectKind kind = ObjectKind::kShape;
    SemanticValue semantic_value{};

    bool operator==(const ObjectContent&) const = default;
};

struct EraseMaskRecord final {
    ObjectId id{};
    SemanticValue geometry{};

    bool operator==(const EraseMaskRecord&) const = default;
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
