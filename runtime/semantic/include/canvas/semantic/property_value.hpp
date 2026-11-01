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

enum class PatternKind : std::uint8_t { kDiagonal = 1, kCrosshatch = 2 };
struct PatternFillV1 final {
    std::uint32_t version = 1;
    ColorValue color{};
    PatternKind kind = PatternKind::kDiagonal;
    double angle = -45, gap = 8, width = 1;
    bool operator==(const PatternFillV1&) const = default;
};
using FillStyleValue = std::variant<NoFill, SolidFill, PatternFillV1>;

struct SketchStyleV1 final {
    std::uint32_t version = 1;
    std::uint64_t seed = 0;
    double roughness = 1;
    bool enabled = true;
    bool operator==(const SketchStyleV1&) const = default;
};

struct CornerStyleV1 final {
    std::uint32_t version = 1;
    double radius = 0;
    bool operator==(const CornerStyleV1&) const = default;
};

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

// Closed renderer-neutral tagged union. Released V1 alternatives retain their
// indices; the accepted sketch-v1 extension appends a versioned value. Registry,
// normalization and validation define applicability/default/clear semantics.
// An opaque payload is never canonical property state.
using PropertyValue = std::variant<
    bool,
    float,
    ColorValue,
    FillStyleValue,
    StrokeStyleValue,
    BlendModeValue,
    ConnectorDecorationValue,
    SketchStyleV1,
    CornerStyleV1>;

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
