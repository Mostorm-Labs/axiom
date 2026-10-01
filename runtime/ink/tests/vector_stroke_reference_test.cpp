#include "canvas/ink/vector_stroke_reference.hpp"
#include "vector_stroke_reference_fixtures.hpp"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <iomanip>
#include <iostream>

namespace {
auto scalars(const canvas::ink::reference::StrokePoint& p) {
  return std::array{p.x, p.y, p.pressure, p.vector_x, p.vector_y, p.distance, p.running_length};
}
auto scalars(const canvas::ink::reference::StrokeOutlinePoint& p) {
  return std::array{p.x, p.y};
}
template <typename T> bool identical(const std::vector<T>& a, const std::vector<T>& b) {
  if (a.size() != b.size()) return false;
  for (std::size_t i = 0; i < a.size(); ++i) {
    auto x = scalars(a[i]), y = scalars(b[i]);
    for (std::size_t j = 0; j < x.size(); ++j)
      if (std::bit_cast<std::uint64_t>(x[j]) != std::bit_cast<std::uint64_t>(y[j])) return false;
  }
  return true;
}
bool close(double a, double b, double tolerance) {
  if (std::isnan(a) || std::isnan(b)) return std::isnan(a) && std::isnan(b);
  if (std::isinf(a) || std::isinf(b)) return a == b;
  return std::abs(a - b) <= tolerance;
}
}  // namespace

int main() {
  using namespace canvas::ink::reference;
  constexpr double pointTolerance = 1e-6;
  constexpr double pressureTolerance = 1e-9;
  constexpr double outlineTolerance = 1e-5;
  double maxPointError = 0, maxPressureError = 0, maxOutlineError = 0;
  std::cout << std::setprecision(17) << "{\"fixtures\":[";
  bool first = true;
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
      auto a = scalars(actual), b = scalars(expected);
      for (std::size_t j = 0; j < a.size(); ++j) {
        if (std::isfinite(a[j]) && std::isfinite(b[j])) {
          auto& error = j == 2 ? maxPressureError : maxPointError;
          error = std::max(error, std::abs(a[j] - b[j]));
        }
      }
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
      if (!close(outline[i].x, fixture.outline[i].x, outlineTolerance) ||
          !close(outline[i].y, fixture.outline[i].y, outlineTolerance)) {
        std::cerr << fixture.id << " outline mismatch at " << i << " actual=" << outline[i].x << "," << outline[i].y << " expected=" << fixture.outline[i].x << "," << fixture.outline[i].y << "\n";
        return 1;
      }
      if (std::isfinite(outline[i].x)) maxOutlineError = std::max(maxOutlineError, std::abs(outline[i].x - fixture.outline[i].x));
      if (std::isfinite(outline[i].y)) maxOutlineError = std::max(maxOutlineError, std::abs(outline[i].y - fixture.outline[i].y));
    }
    for (int repeat = 0; repeat < 16; ++repeat) {
      const auto p = getStrokePoints(fixture.input, fixture.options);
      const auto o = getStrokeOutlinePoints(p, fixture.options);
      if (!identical(conditioned, p) || !identical(outline, o)) {
        std::cerr << fixture.id << " scalar bit-pattern determinism mismatch\n";
        return 1;
      }
    }
    if (!first) std::cout << ',';
    first = false;
    std::cout << "{\"id\":\"" << fixture.id << "\",\"points\":" << conditioned.size()
              << ",\"outline\":" << outline.size() << ",\"parity\":\"PASS\",\"bitwise_determinism\":\"PASS\"}";
  }
  std::cout << "],\"max_point_error\":" << maxPointError << ",\"max_pressure_error\":" << maxPressureError
            << ",\"max_outline_error\":" << maxOutlineError << ",\"repeat_count\":16,\"status\":\"PASS\"}\n";
  return 0;
}
