#pragma once
#include "skia_native_platform.hpp"
namespace canvas::sdk {
// Borrowed CAMetalLayer handles are retained by providers for the binding.
NativeSurfacePair makeMetalSurfaces(const NativeView&);
}
