#pragma once

#include <cstdint>
#include <variant>
#include <vector>

namespace canvas::semantic {

struct Vec2 final {
    double x = 0.0;
    double y = 0.0;

    bool operator==(const Vec2&) const = default;
};

struct Rect2 final {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    bool operator==(const Rect2&) const = default;
};

struct NormalizedRect final {
    double x = 0.0;
    double y = 0.0;
    double width = 0.0;
    double height = 0.0;

    bool operator==(const NormalizedRect&) const = default;
};

enum class FillRule : std::uint8_t {
    kNonZero = 1,
    kEvenOdd = 2,
};

struct MoveTo final {
    Vec2 point{};

    bool operator==(const MoveTo&) const = default;
};

struct LineTo final {
    Vec2 end{};

    bool operator==(const LineTo&) const = default;
};

struct QuadTo final {
    Vec2 control{};
    Vec2 end{};

    bool operator==(const QuadTo&) const = default;
};

struct CubicTo final {
    Vec2 control1{};
    Vec2 control2{};
    Vec2 end{};

    bool operator==(const CubicTo&) const = default;
};

struct ClosePath final {
    bool operator==(const ClosePath&) const = default;
};

using PathCommand = std::variant<MoveTo, LineTo, QuadTo, CubicTo, ClosePath>;

struct VectorPathGeometry final {
    FillRule fill_rule = FillRule::kNonZero;
    std::vector<PathCommand> commands;

    bool operator==(const VectorPathGeometry&) const = default;
};

} // namespace canvas::semantic
