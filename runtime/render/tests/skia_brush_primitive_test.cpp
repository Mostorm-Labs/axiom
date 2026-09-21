#include "canvas/render/skia_ink_backend.hpp"

#include <cassert>
#include <cstdint>
#include <vector>

int main() {
    canvas::render::SkiaInkBackend backend;
    assert(backend.resize(96U, 96U).code == canvas::render::BackendSubmissionCode::kAccepted);
    canvas::ink::BrushPrimitive dab;
    dab.x = 40.0F;
    dab.y = 44.0F;
    dab.size = 18.0F;
    dab.opacity = 0.75F;
    dab.rotation = 0.3F;
    dab.representation = canvas::ink::BrushRepresentation::kDab;
    dab.shapeResource = canvas::ink::ResourceId{101U};
    dab.grainResource = canvas::ink::ResourceId{201U};
    const std::vector<canvas::ink::BrushPrimitive> primitives{dab};
    assert(backend.submitPrimitives(primitives).code == canvas::render::BackendSubmissionCode::kAccepted);
    const auto first = backend.rgba();
    bool nonWhite = false;
    for (std::size_t i = 0; i + 3U < first.size(); i += 4U) {
        if (first[i] != 255U || first[i + 1U] != 255U || first[i + 2U] != 255U) {
            nonWhite = true;
            break;
        }
    }
    assert(nonWhite);
    dab.rotation += 0.5F;
    assert(backend.submitPrimitives(std::vector<canvas::ink::BrushPrimitive>{dab}).code ==
           canvas::render::BackendSubmissionCode::kAccepted);
    assert(backend.rasterizationCount() == 2U);
    return 0;
}
