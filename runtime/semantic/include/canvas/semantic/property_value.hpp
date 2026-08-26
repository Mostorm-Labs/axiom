#pragma once

#include <cstdint>
#include <variant>
#include <vector>

namespace canvas::semantic {

struct ColorValue final {
    float r = 0.0F;
    float g = 0.0F;
    float b = 0.0F;
    float a = 0.0F;

    bool operator==(const ColorValue&) const = default;
};

struct NoFill final {
    bool operator==(const NoFill&) const = default;
};

struct SolidFill final {
    ColorValue color{};

    bool operator==(const SolidFill&) const = default;
};

using FillStyleValue = std::variant<NoFill, SolidFill>;

struct NoStroke final {
    bool operator==(const NoStroke&) const = default;
};

enum class StrokeCap : std::uint8_t {
    kButt = 1,
    kRound = 2,
    kSquare = 3,
};

struct MiterJoin final {
    double limit = 0.0;

    bool operator==(const MiterJoin&) const = default;
};

struct RoundJoin final {
    bool operator==(const RoundJoin&) const = default;
};

struct BevelJoin final {
    bool operator==(const BevelJoin&) const = default;
};

using StrokeJoin = std::variant<MiterJoin, RoundJoin, BevelJoin>;

struct SolidDash final {
    bool operator==(const SolidDash&) const = default;
};

struct DashPattern final {
    std::vector<double> segments;
    double offset = 0.0;

    bool operator==(const DashPattern&) const = default;
};

using StrokeDash = std::variant<SolidDash, DashPattern>;

struct SolidStroke final {
    ColorValue color{};
    double width = 0.0;
    StrokeCap cap = StrokeCap::kButt;
    StrokeJoin join{MiterJoin{}};
    StrokeDash dash{SolidDash{}};

    bool operator==(const SolidStroke&) const = default;
};

using StrokeStyleValue = std::variant<NoStroke, SolidStroke>;

enum class BlendModeValue : std::uint8_t {
    kNormal = 1,
};

enum class ConnectorDecorationValue : std::uint8_t {
    kNone = 1,
    kArrow = 2,
};

// Frozen V1 PropertyValue: a closed, renderer-neutral tagged union. Property
// applicability, defaults, clear semantics and range validation are deferred
// to GT-G1-04; an opaque payload is never canonical property state.
using PropertyValue = std::variant<
    bool,
    float,
    ColorValue,
    FillStyleValue,
    StrokeStyleValue,
    BlendModeValue,
    ConnectorDecorationValue>;

struct PropertyEntry final {
    std::uint32_t field_id = 0;
    PropertyValue value{};

    bool operator==(const PropertyEntry&) const = default;
};

struct PropertyBag final {
    std::vector<PropertyEntry> entries;

    bool operator==(const PropertyBag&) const = default;
};

} // namespace canvas::semantic
