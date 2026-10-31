#pragma once
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "canvas/semantic/compact_stroke.hpp"
#include "auditoryworks/axiom/v1/compact_stroke.pb.h"
namespace canvas::semantic::internal {
void setCompactStroke(const CompactStrokeContent&, auditoryworks::axiom::v1::CompactStrokeContent&);
bool getCompactStroke(const auditoryworks::axiom::v1::CompactStrokeContent&, CompactStrokeContent&);
}
#endif
