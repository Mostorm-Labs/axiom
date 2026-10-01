#pragma once
#include "canvas/ink/resolved_brush_state.hpp"
#include "canvas/ink/vector_stroke_reference.hpp"
#include <span>
#include <string>
#include <vector>
namespace canvas::ink { struct VectorPathResult final { std::vector<reference::StrokePoint> points; std::vector<reference::StrokeOutlinePoint> outline; std::string error; explicit operator bool() const noexcept{return error.empty()&&!outline.empty();} }; class VectorPathNode final { public: explicit VectorPathNode(ResolvedBrushState state):state_(std::move(state)){} [[nodiscard]] VectorPathResult evaluate(std::span<const reference::VectorStrokeInput>,bool last) const; private: ResolvedBrushState state_; }; }
