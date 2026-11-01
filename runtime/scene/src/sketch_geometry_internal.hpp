#pragma once
#include "canvas/scene/sketch_geometry.hpp"
namespace canvas::scene::detail {
double sketchRandom(std::uint64_t &state);
semantic::CubicTo sketchSegment(semantic::Vec2, semantic::Vec2, double, std::uint64_t &);
std::shared_ptr<const SketchGeometry> extendedSketchGeometry(const semantic::ObjectContent &,
                                                             const semantic::PropertyBag &,std::size_t maximumCommands=4096);
std::shared_ptr<const SketchGeometry> roundedSketchGeometry(const semantic::ObjectContent&,const semantic::PropertyBag&,const ConnectorGeometry*,double radius);
} // namespace canvas::scene::detail
