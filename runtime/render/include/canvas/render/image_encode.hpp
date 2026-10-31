#pragma once
#include "canvas/render/skia_surface_provider.hpp"
#include <vector>

namespace canvas::render {
// Internal encoder seam; no Skia object escapes to a consumer SDK.
[[nodiscard]] std::vector<std::uint8_t> encodeRasterImage(
    RasterSkiaSurfaceProvider&, bool jpeg, int quality);
}
