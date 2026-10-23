#pragma once

#include "canvas/foundation/world_geometry.hpp"
#include "canvas/semantic/object_content.hpp"

#include <algorithm>

namespace canvas::render {

// Object-local source plane used by both Crop drawing and pointer hit testing.
// This is derived editor geometry, never persisted as ImageContent.
inline foundation::WorldRect imageCropSourceBounds(const semantic::ImageContent& content) noexcept {
    const auto source = content.source_rect.value_or(semantic::NormalizedRect{0, 0, 1, 1});
    double width = content.width, height = content.height;
    if (content.content_mode != semantic::ImageContentMode::kStretch) {
        const double sourceWidth = source.width * content.intrinsic_width;
        const double sourceHeight = source.height * content.intrinsic_height;
        const double sx = width / sourceWidth, sy = height / sourceHeight;
        const double scale = content.content_mode == semantic::ImageContentMode::kFill
            ? std::max(sx, sy) : std::min(sx, sy);
        width = sourceWidth * scale;
        height = sourceHeight * scale;
    }
    const double fullWidth = width / source.width, fullHeight = height / source.height;
    const double left = (content.width - width) / 2 - source.x * fullWidth;
    const double top = (content.height - height) / 2 - source.y * fullHeight;
    return {static_cast<float>(left), static_cast<float>(top),
            static_cast<float>(left + fullWidth), static_cast<float>(top + fullHeight)};
}

} // namespace canvas::render
