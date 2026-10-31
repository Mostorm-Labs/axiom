#pragma once

#include "canvas/foundation/document_background.hpp"
#if defined(CANVAS_SEMANTIC_PROTOBUF)
#include "auditoryworks/axiom/v1/document_background.pb.h"

namespace canvas::semantic::internal {
inline void toProtobufBackground(const foundation::DocumentBackground& value,
    auditoryworks::axiom::v1::DocumentBackgroundV1& wire) {
    wire.set_version(1);
    wire.set_mode(static_cast<std::uint32_t>(value.mode));
    const auto color = [](const auto& input, auto* output) {
        output->set_r(input[0]); output->set_g(input[1]);
        output->set_b(input[2]); output->set_a(input[3]);
    };
    color(value.color, wire.mutable_color());
    color(value.patternColor, wire.mutable_pattern_color());
}
inline bool fromProtobufBackground(const auditoryworks::axiom::v1::DocumentBackgroundV1& wire,
    foundation::DocumentBackground& value) {
    if (!wire.has_version() || wire.version() != 1 || !wire.has_mode() || wire.mode() > 3 ||
        !wire.has_color() || !wire.has_pattern_color()) return false;
    const auto color = [](const auto& input, auto& output) {
        if (!input.has_r() || !input.has_g() || !input.has_b() || !input.has_a()) return false;
        output = {input.r(),input.g(),input.b(),input.a()};
        return true;
    };
    value.mode = static_cast<foundation::BackgroundMode>(wire.mode());
    return color(wire.color(), value.color) && color(wire.pattern_color(), value.patternColor) &&
        foundation::validDocumentBackground(value);
}
} // namespace canvas::semantic::internal
#endif
