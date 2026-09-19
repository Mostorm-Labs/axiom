#pragma once
#include "canvas/ink/ink_engine.hpp"
namespace canvas::ink { class BrushEngine final { public: static StrokePoint point(const BrushDescriptor&, const input::PointerSample& sample) noexcept { return {sample.x,sample.y,sample.pressure}; } }; }
