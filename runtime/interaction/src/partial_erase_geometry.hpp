#pragma once
#include "canvas/semantic/erase_mask.hpp"
#include "canvas/semantic/object_record.hpp"
#include "canvas/foundation/world_geometry.hpp"
#include <span>
#include <optional>
#include <cmath>
namespace canvas::interaction {
// Same local mask for live presentation and the canonical AddEraseMasks commit.
inline std::optional<semantic::EraseMaskGeometry> partialEraseGeometry(
    std::span<const foundation::WorldPoint> trace, const semantic::Transform2D& transform, double radius) {
    if(trace.empty())return std::nullopt;
      const double determinant = transform.a * transform.d - transform.b * transform.c;
      if (!std::isfinite(determinant) || determinant == 0.0) return std::nullopt;
      const auto localPoint = [&](const semantic::Vec2& point) {
        const double x = point.x - transform.tx;
        const double y = point.y - transform.ty;
        return semantic::Vec2{(transform.d * x - transform.c * y) / determinant,
                              (transform.a * y - transform.b * x) / determinant};
      };
      // EraseMaskGeometry is object-local. The retained trace and preview
      // are world-space; convert at this operation construction boundary.
      semantic::SweptCircleMask swept;
      for (std::size_t i = 1; i < trace.size(); ++i) {
        const auto& a = trace[i - 1U];
        const auto& b = trace[i];
        swept.segments.push_back({
            {{a.x, a.y}, radius}, {{b.x, b.y}, radius},
            {a.x + (b.x - a.x) / 3.0F, a.y + (b.y - a.y) / 3.0F},
            {a.x + (b.x - a.x) * 2.0F / 3.0F, a.y + (b.y - a.y) * 2.0F / 3.0F}});
      }
      if (swept.segments.empty()) {
        const auto& p = trace.front();
        swept.segments.push_back({{{p.x, p.y}, radius}, {{p.x, p.y}, radius},
                                  {p.x, p.y}, {p.x, p.y}});
      }
      semantic::EraseMaskGeometry geometry;
      const double scaleX = std::hypot(transform.a, transform.b);
      const double scaleY = std::hypot(transform.c, transform.d);
      const double scale = std::max(scaleX, scaleY);
      const bool similarity = std::abs(scaleX - scaleY) <= 1e-12 * scale &&
          std::abs((transform.a / scale) * (transform.c / scale) +
                   (transform.b / scale) * (transform.d / scale)) <= 1e-12;
      if (similarity) {
        for (auto& segment : swept.segments) {
          segment.p0.position = localPoint(segment.p0.position);
          segment.p1.position = localPoint(segment.p1.position);
          segment.control1 = localPoint(segment.control1);
          segment.control2 = localPoint(segment.control2);
          segment.p0.radius /= scaleX;
          segment.p1.radius /= scaleX;
        }
        geometry = std::move(swept);
      } else {
        // A world circle is an ellipse in a nonuniformly scaled/sheared
        // object's local space. Use the existing FilledPathMask variant for
        // the inverse-mapped union of round caps and connecting rectangles.
        // Never approximate it with a single local radius.
        semantic::FilledPathMask filled;
        auto& commands = filled.path.commands;
        constexpr double kCircleControl = 0.5522847498307936;
        for (const auto& point : trace) {
          const double x = point.x, y = point.y;
          const double k = radius * kCircleControl;
          commands.push_back(semantic::MoveTo{localPoint({x + radius, y})});
          commands.push_back(semantic::CubicTo{localPoint({x + radius, y + k}),
              localPoint({x + k, y + radius}), localPoint({x, y + radius})});
          commands.push_back(semantic::CubicTo{localPoint({x - k, y + radius}),
              localPoint({x - radius, y + k}), localPoint({x - radius, y})});
          commands.push_back(semantic::CubicTo{localPoint({x - radius, y - k}),
              localPoint({x - k, y - radius}), localPoint({x, y - radius})});
          commands.push_back(semantic::CubicTo{localPoint({x + k, y - radius}),
              localPoint({x + radius, y - k}), localPoint({x + radius, y})});
          commands.push_back(semantic::ClosePath{});
        }
        for (std::size_t i = 1; i < trace.size(); ++i) {
          const auto& a = trace[i - 1U];
          const auto& b = trace[i];
          const double dx = static_cast<double>(b.x) - a.x;
          const double dy = static_cast<double>(b.y) - a.y;
          const double length = std::hypot(dx, dy);
          if (length == 0.0) continue;
          const double nx = -dy * radius / length, ny = dx * radius / length;
          commands.push_back(semantic::MoveTo{localPoint({a.x + nx, a.y + ny})});
          commands.push_back(semantic::LineTo{localPoint({a.x - nx, a.y - ny})});
          commands.push_back(semantic::LineTo{localPoint({b.x - nx, b.y - ny})});
          commands.push_back(semantic::LineTo{localPoint({b.x + nx, b.y + ny})});
          commands.push_back(semantic::ClosePath{});
        }
        geometry = std::move(filled);
      }
    return geometry;
}
}
