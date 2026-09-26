#pragma once
#include "canvas/ink/brush_package.hpp"
#include <cstdint>
#include <string>
namespace canvas::ink {
struct ResolvedBrushState final { BrushPackage package{}; std::uint64_t seed=0; std::string identity; };
[[nodiscard]] ResolvedBrushState resolveBrushState(const BrushPackage&,std::uint64_t seed);
}
