#pragma once

#include <cstddef>
#include <span>
#include <vector>
#include <limits>

namespace canvas::ink::reference {

struct VectorStrokeInput {
  double x = 0.0;
  double y = 0.0;
  double pressure = std::numeric_limits<double>::quiet_NaN();
};

struct StrokePoint {
  double x = 0.0;
  double y = 0.0;
  double pressure = 0.0;
  double vector_x = 0.0;
  double vector_y = 0.0;
  double distance = 0.0;
  double running_length = 0.0;
};

struct StrokeOptions {
  double size = 16.0;
  double thinning = 0.5;
  double smoothing = 0.5;
  double streamline = 0.5;
  bool simulate_pressure = true;
  bool start_cap = true;
  double start_taper = 0.0;
  bool start_taper_enabled = false;
  bool end_cap = true;
  double end_taper = 0.0;
  bool end_taper_enabled = false;
  bool last = false;
  bool start_taper_full_length = false;
  bool end_taper_full_length = false;
};

struct StrokeOutlinePoint {
  double x = 0.0;
  double y = 0.0;
};

std::vector<StrokePoint> getStrokePoints(std::span<const VectorStrokeInput> input,
                                         const StrokeOptions& options = {});
std::vector<StrokeOutlinePoint> getStrokeOutlinePoints(
    std::span<const StrokePoint> points, const StrokeOptions& options = {});

}  // namespace canvas::ink::reference
