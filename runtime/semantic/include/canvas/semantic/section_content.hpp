#pragma once

#include "canvas/semantic/semantic_geometry.hpp"

#include <string>

namespace canvas::semantic {

// Section/1 owns its local boundary and title metadata. Membership remains
// ObjectRecord::placement; layout and editor state are not canonical fields.
struct SectionContent final {
    Rect2 boundary{};
    std::string title;
    double padding = 0.0;
    double header_height = 0.0;

    bool operator==(const SectionContent&) const = default;
};

} // namespace canvas::semantic
