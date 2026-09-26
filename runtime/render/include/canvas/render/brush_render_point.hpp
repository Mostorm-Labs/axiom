#pragma once

#include <cstdint>

namespace canvas::render {

// Renderer-neutral projection of a committed or preview brush dab. The
// composition root owns how these values are produced; platform bridges only
// forward the immutable batch to a renderer.
struct BrushRenderPoint final {
    float x = 0.0F;
    float y = 0.0F;
    float size = 0.0F;
    float rotation = 0.0F;
    float opacity = 0.0F;
    std::uint8_t representation = 1U;
};

} // namespace canvas::render
