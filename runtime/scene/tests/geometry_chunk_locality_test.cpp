#include "canvas/scene/geometry_chunk.hpp"
#include "canvas/semantic/compact_stroke.hpp"

#include <cassert>
#include <iostream>
#include <vector>

using canvas::WorldRect;

int main() {
    for (const unsigned population : {100U, 500U, 1000U}) {
        canvas::semantic::CompactStrokeRecord stroke;
        stroke.knots.reserve(1024);
        for (unsigned i = 0; i < 1024; ++i)
            stroke.knots.push_back({static_cast<double>(i), 8.0, 32768});
        const auto chunks = canvas::scene::buildGeometryChunks(
            canvas::semantic::ObjectContent{canvas::semantic::CompactStrokeContent{std::move(stroke)}});
        assert(chunks.size() == 4U);
        const WorldRect viewport{300, -1, 310, 20};
        unsigned selected = 0;
        for (const auto& chunk : chunks)
            if (chunk.bounds.intersects(viewport)) ++selected;
        assert(selected == 1U);
        std::cout << "{\"geometry_chunk_population\":" << population
                  << ",\"chunk_count\":" << chunks.size()
                  << ",\"selected_chunks\":" << selected << "}\n";
    }
    return 0;
}
