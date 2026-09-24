#pragma once
#include "canvas/ink/resolved_brush_state.hpp"
#include <array>
#include <string>
namespace canvas::ink { struct BrushExecutionPlan final { std::array<std::uint32_t,3> active{1,2,3}; std::string error; explicit operator bool() const noexcept{return error.empty();} }; [[nodiscard]] BrushExecutionPlan compileBrushExecutionPlan(const ResolvedBrushState&); }
