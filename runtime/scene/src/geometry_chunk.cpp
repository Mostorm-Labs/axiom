#include "canvas/scene/geometry_chunk.hpp"

#include <algorithm>
#include <cstring>
#include <array>
#include <numeric>
#include <limits>
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

GeometryChunkIndex::GeometryChunkIndex(const RuntimeSceneRecord& r)
    :object_(r.objectId),content_(r.content.shared()),table_(r.geometryChunks.identity()),chunks_(r.geometryChunks.read()){
    order_.resize(chunks_.size());std::iota(order_.begin(),order_.end(),0U);
    nodes_.reserve(chunks_.size()/2+1);if(!order_.empty())build(0,static_cast<std::uint32_t>(order_.size()));
}
bool GeometryChunkIndex::matches(const RuntimeSceneRecord& r)const noexcept{
    const auto source=r.content.shared();
    return object_==r.objectId&&table_==r.geometryChunks.identity()&&!content_.expired()&&
        !content_.owner_before(source)&&!source.owner_before(content_);
}
std::uint32_t GeometryChunkIndex::build(std::uint32_t first,std::uint32_t last){
    WorldRect bounds=chunks_[order_[first]].bounds;
    for(auto i=first+1;i<last;++i)bounds=foundation::unionRects(bounds,chunks_[order_[i]].bounds);
    const auto index=static_cast<std::uint32_t>(nodes_.size());nodes_.push_back({bounds,0,0,first,last-first});
    if(last-first>8){
        const bool x=double(bounds.right)-bounds.left>=double(bounds.bottom)-bounds.top;
        const auto mid=first+(last-first)/2;
        std::nth_element(order_.begin()+first,order_.begin()+mid,order_.begin()+last,[&](auto a,auto b){
            const auto& p=chunks_[a].bounds;const auto& q=chunks_[b].bounds;
            const auto u=x?double(p.left)+p.right:double(p.top)+p.bottom;
            const auto v=x?double(q.left)+q.right:double(q.top)+q.bottom;
            return u!=v?u<v:a<b;
        });
        const auto left=build(first,mid),right=build(mid,last);
        nodes_[index].left=left;nodes_[index].right=right;nodes_[index].count=0;
    }
    return index;
}
std::vector<std::uint32_t> GeometryChunkIndex::query(WorldRect local,std::uint64_t& visited)const{
    std::vector<std::uint32_t> result;if(nodes_.empty())return result;
    std::array<std::uint32_t,64> stack{};std::size_t count=1;
    while(count){const auto& node=nodes_[stack[--count]];++visited;
        if(!node.bounds.intersects(local))continue;
        if(node.count)result.insert(result.end(),order_.begin()+node.begin,order_.begin()+node.begin+node.count);
        else{stack[count++]=node.left;stack[count++]=node.right;}
    }
    std::sort(result.begin(),result.end());return result;
}
std::shared_ptr<const GeometryChunkIndex> buildGeometryChunkIndex(const RuntimeSceneRecord& r){
    if(r.geometryChunks.empty()||r.geometryChunks.size()>std::numeric_limits<std::uint32_t>::max()/2U||
        !r.content.shared()||std::any_of(r.geometryChunks.begin(),r.geometryChunks.end(),[](const auto& c){return !c.bounds.isFiniteAndOrdered();}))return {};
    return std::make_shared<const GeometryChunkIndex>(r);
}
} // namespace canvas::scene
