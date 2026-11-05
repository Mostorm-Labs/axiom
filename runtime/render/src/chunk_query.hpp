#pragma once
#include "affine_bounds.hpp"
#include "canvas/scene/geometry_chunk.hpp"
#include "canvas/scene/hierarchy_geometry.hpp"
#include <limits>
#include <cmath>
#include <numeric>

namespace canvas::render::internal {
struct ChunkQueryWork {std::uint64_t candidates=0,nodes=0,fallbacks=0;bool fullObject=false;};
template<class Consumer>void queryChunks(const RuntimeSceneRecord& record,const WorldRect& region,
    float margin,ChunkQueryWork& work,Consumer&& consume){
    std::vector<std::uint32_t> slots;
    semantic::Transform2D inverse{};const auto& t=record.transform;
    const double norm=std::max({std::abs(t.a),std::abs(t.b),std::abs(t.c),std::abs(t.d)});
    bool indexed=record.geometryIndex&&record.geometryIndex->matches(record)&&
        std::isfinite(margin)&&margin>=0&&region.isFiniteAndOrdered()&&
        std::isfinite(norm)&&std::abs(t.a*t.d-t.b*t.c)>norm*norm*1e-12&&scene::inverseTransform(t,inverse);
    if(indexed){
        // Broad phase only. Expand outward for float roundoff before inversion;
        // the exact original world-space leaf/margin test below is unchanged.
        const double magnitude=std::max({1.,std::abs(double(region.left)),std::abs(double(region.right)),
            std::abs(double(region.top)),std::abs(double(region.bottom)),std::abs(t.tx),std::abs(t.ty)});
        const auto pad=float(margin+8*std::numeric_limits<float>::epsilon()*magnitude);
        const WorldRect world{region.left-pad,region.top-pad,region.right+pad,region.bottom+pad};
        auto local=transformedBounds(world,inverse);
        local.left=std::nextafter(local.left,-std::numeric_limits<float>::infinity());
        local.top=std::nextafter(local.top,-std::numeric_limits<float>::infinity());
        local.right=std::nextafter(local.right,std::numeric_limits<float>::infinity());
        local.bottom=std::nextafter(local.bottom,std::numeric_limits<float>::infinity());
        indexed=local.isFiniteAndOrdered();
        if(indexed)slots=record.geometryIndex->query(local,work.nodes);
    }
    if(!indexed){work.fullObject=true;++work.fallbacks;slots.resize(record.geometryChunks.size());std::iota(slots.begin(),slots.end(),0U);}
    for(const auto slot:slots){++work.candidates;const auto& chunk=record.geometryChunks[slot];
        const auto bounds=transformedBounds(chunk.bounds,t);
        if(bounds.isFiniteAndOrdered()&&region.isFiniteAndOrdered()&&bounds.right+margin>region.left&&
            bounds.left-margin<region.right&&bounds.bottom+margin>region.top&&bounds.top-margin<region.bottom)consume(chunk);
    }
}
}
