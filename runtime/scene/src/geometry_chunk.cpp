#include "canvas/scene/geometry_chunk.hpp"

#include <algorithm>
#include <cstring>
#include <iomanip>
#include <sstream>
#include <type_traits>

namespace canvas::scene {
namespace {
constexpr std::size_t kChunkPoints = 256;

struct ChunkPoint final { double x = 0.0; double y = 0.0; };

std::vector<ChunkPoint> points(const semantic::ObjectContent& content) {
    return std::visit([](const auto& value) {
        using T = std::decay_t<decltype(value)>;
        std::vector<ChunkPoint> result;
        if constexpr (std::is_same_v<T, semantic::CompactStrokeContent>) {
            result.reserve(value.stroke.knots.size());
            for (const auto& knot : value.stroke.knots) result.push_back({knot.x, knot.y});
        } else if constexpr (std::is_same_v<T, semantic::BrushStrokeContent>) {
            result.reserve(value.stroke.vector_output.outline.size());
            for (const auto& point : value.stroke.vector_output.outline) result.push_back({point.x, point.y});
        } else if constexpr (std::is_same_v<T, semantic::VectorStrokeContent>) {
            if (const auto* data = std::get_if<semantic::VectorStrokeData>(&value.stroke.data)) {
                result.reserve(data->samples.size());
                for (const auto& sample : data->samples) result.push_back({sample.position.x, sample.position.y});
            }
        }
        return result;
    }, content);
}

std::string digest(std::span<const ChunkPoint> values) {
    std::uint64_t hash = 1469598103934665603ULL;
    for (const auto& point : values) {
        for (const double value : {point.x, point.y}) {
            std::uint64_t bits = 0;
            static_assert(sizeof(bits) == sizeof(value));
            std::memcpy(&bits, &value, sizeof(bits));
            for (unsigned byte = 0; byte < 8; ++byte) { hash ^= (bits >> (byte * 8U)) & 0xffU; hash *= 1099511628211ULL; }
        }
    }
    std::ostringstream out; out << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0') << hash; return out.str();
}
}

std::vector<GeometryChunk> buildGeometryChunks(const semantic::ObjectContent& content) {
    const auto source = points(content);
    std::vector<GeometryChunk> result;
    result.reserve((source.size() + kChunkPoints - 1U) / kChunkPoints);
    for (std::size_t offset = 0, index = 0; offset < source.size(); offset += kChunkPoints, ++index) {
        const auto end = std::min(source.size(), offset + kChunkPoints);
        foundation::WorldRect bounds{}; bool have = false;
        for (std::size_t i = offset; i < end; ++i) {
            const foundation::WorldRect point{static_cast<float>(source[i].x), static_cast<float>(source[i].y), static_cast<float>(source[i].x), static_cast<float>(source[i].y)};
            bounds = have ? foundation::unionRects(bounds, point) : point; have = true;
        }
        result.push_back(GeometryChunk{static_cast<std::uint32_t>(index), static_cast<std::uint32_t>(offset), static_cast<std::uint32_t>(end-offset), bounds, digest(std::span<const ChunkPoint>(source.data()+offset, end-offset))});
    }
    return result;
}
} // namespace canvas::scene
