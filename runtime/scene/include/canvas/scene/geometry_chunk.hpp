#pragma once

#include "canvas/scene/scene_types.hpp"

namespace canvas::scene {

// Derives bounded, renderer-neutral source chunks from canonical stroke data.
// The helper never recomputes brush geometry and is safe to call from both
// full and incremental RuntimeScene projection.
[[nodiscard]] std::vector<GeometryChunk> buildGeometryChunks(
    const semantic::ObjectContent& content);

struct GeometryChunkKey final {
    ObjectId objectId{};
    std::uint32_t ordinal=0,pointOffset=0,pointCount=0;
    bool operator==(const GeometryChunkKey&) const=default;
};
// Derived local-space BVH. Complete immutable content identity and chunk storage
// must match before use; keys alone/digest strings cannot certify source validity.
class GeometryChunkIndex final {
    struct Node {WorldRect bounds;std::uint32_t left=0,right=0,begin=0,count=0;};
    ObjectId object_{};
    std::weak_ptr<const void> content_;
    const void* table_=nullptr;
    SharedVectorView<GeometryChunk> chunks_;
    std::vector<Node> nodes_;
    std::vector<std::uint32_t> order_;
    std::uint32_t build(std::uint32_t,std::uint32_t);
  public:
    explicit GeometryChunkIndex(const RuntimeSceneRecord&);
    bool matches(const RuntimeSceneRecord&)const noexcept;
    GeometryChunkKey key(std::size_t i)const{return {object_,chunks_[i].index,chunks_[i].pointOffset,chunks_[i].pointCount};}
    std::vector<std::uint32_t> query(WorldRect local,std::uint64_t& nodesVisited)const;
    std::size_t bytes()const noexcept{return sizeof(*this)+nodes_.capacity()*sizeof(Node)+order_.capacity()*sizeof(std::uint32_t);}
};
std::shared_ptr<const GeometryChunkIndex> buildGeometryChunkIndex(const RuntimeSceneRecord&);
} // namespace canvas::scene
