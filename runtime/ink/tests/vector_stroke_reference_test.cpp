#include "canvas/ink/vector_stroke_reference.hpp"
#include "vector_stroke_reference_fixtures.hpp"

#include <cassert>
#include <cmath>
#include <iostream>

int main() {
  using namespace canvas::ink::reference;
  constexpr double pointTolerance = 1e-6;
  constexpr double pressureTolerance = 1e-9;
  constexpr double outlineTolerance = 1e-5;
  for (const auto& fixture : canvas::ink::reference::test_fixture::all()) {
    const auto conditioned = getStrokePoints(fixture.input, fixture.options);
    const auto outline = getStrokeOutlinePoints(conditioned, fixture.options);
    if (conditioned.size() != fixture.points.size() ||
        outline.size() != fixture.outline.size()) {
      std::cerr << fixture.id << " count mismatch: points " << conditioned.size()
                << "/" << fixture.points.size() << ", outline " << outline.size()
                << "/" << fixture.outline.size() << "\n";
      return 1;
    }
    for (std::size_t i = 0; i < conditioned.size(); ++i) {
      const auto& actual = conditioned[i];
      const auto& expected = fixture.points[i];
      const auto close = [&](double a, double b, double tolerance) {
        return std::isfinite(a) == std::isfinite(b) &&
               (!std::isfinite(a) || std::abs(a - b) <= tolerance);
      };
      if (!close(actual.x, expected.x, pointTolerance) ||
          !close(actual.y, expected.y, pointTolerance) ||
          !close(actual.pressure, expected.pressure, pressureTolerance) ||
          !close(actual.vector_x, expected.vector_x, pointTolerance) ||
          !close(actual.vector_y, expected.vector_y, pointTolerance) ||
          !close(actual.distance, expected.distance, pointTolerance) ||
          !close(actual.running_length, expected.running_length, pointTolerance)) {
        std::cerr << fixture.id << " stroke point mismatch at " << i << " actual pressure=" << actual.pressure << " expected=" << expected.pressure << "\n";
        return 1;
      }
    }
    for (std::size_t i = 0; i < outline.size(); ++i) {
      if (std::abs(outline[i].x - fixture.outline[i].x) > outlineTolerance ||
          std::abs(outline[i].y - fixture.outline[i].y) > outlineTolerance) {
        std::cerr << fixture.id << " outline mismatch at " << i << " actual=" << outline[i].x << "," << outline[i].y << " expected=" << fixture.outline[i].x << "," << fixture.outline[i].y << "\n";
        return 1;
      }
    }
  }
  return 0;
}
