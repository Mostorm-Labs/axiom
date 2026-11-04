#pragma once
#include "canvas/render/reference_draw_list.hpp"

namespace canvas::render::internal {
inline foundation::WorldRect transformedBounds(const foundation::WorldRect& bounds,
                                                const semantic::Transform2D& t) {
    const foundation::WorldPoint points[]={{bounds.left,bounds.top},{bounds.right,bounds.top},
        {bounds.left,bounds.bottom},{bounds.right,bounds.bottom}};
    foundation::WorldRect result{};bool initialized=false;
    for(const auto point:points){
        const auto x=static_cast<float>(t.a*point.x+t.c*point.y+t.tx);
        const auto y=static_cast<float>(t.b*point.x+t.d*point.y+t.ty);
        const foundation::WorldRect one{x,y,x,y};
        result=initialized?foundation::unionRects(result,one):one;initialized=true;
    }
    return result;
}
}
