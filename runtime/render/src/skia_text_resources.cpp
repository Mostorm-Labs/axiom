#include "canvas/render/skia_text_resources.hpp"
#include "../../text/src/skia_font_materialization.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include <map>
#include <tuple>

namespace canvas::render {
struct SkiaTextResources::Impl {
    using Key=std::tuple<std::string,int,std::uint32_t,bool>;
    std::map<Key,sk_sp<SkTypeface>> faces;
};
SkiaTextResources::SkiaTextResources() : impl_(std::make_unique<Impl>()) {}
SkiaTextResources::~SkiaTextResources()=default;
void SkiaTextResources::draw(SkCanvas& canvas,const text::TextLayoutSnapshot& snapshot) {
    for(const auto& run:snapshot.runs) {
        if(run.font>=snapshot.fonts.size() || run.glyphs.size()!=run.positions.size()) continue;
        const auto& f=snapshot.fonts[run.font];
        if(!f.bytes) continue;
        auto& face=impl_->faces[{f.sha256,f.collectionIndex,f.weight,f.italic}];
        if(!face) face=text::internal::exactFace(f);
        if(!face) continue;
        SkFont font(face,run.fontSize); font.setHinting(SkFontHinting::kNone);
        font.setSubpixel(true); font.setEdging(SkFont::Edging::kAntiAlias);
        std::vector<SkPoint> positions; positions.reserve(run.positions.size());
        for(const auto& p:run.positions) positions.push_back(SkPoint::Make(p.x,p.y));
        SkPaint paint; paint.setAntiAlias(true);
        paint.setColor4f({run.color.r,run.color.g,run.color.b,run.color.a});
        canvas.drawGlyphs(run.glyphs,positions,{0,0},font,paint);
    }
    for(const auto& decoration:snapshot.decorations) {
        const auto& c=decoration.color; const auto& r=decoration.bounds;
        SkPaint paint; paint.setAntiAlias(true); paint.setColor4f({c.r,c.g,c.b,c.a});
        canvas.drawRect(SkRect::MakeLTRB(r.left,r.top,r.right,r.bottom),paint);
    }
}
}
