#include "canvas/render/skia_text_resources.hpp"
#include "../../text/src/skia_font_materialization.hpp"
#include "include/core/SkCanvas.h"
#include "include/core/SkFont.h"
#include "include/core/SkPaint.h"
#include <algorithm>
#include <map>
#include <tuple>

namespace canvas::render {
struct SkiaTextResources::Impl {
    using Key=std::tuple<std::string,int,std::uint32_t,bool,bool>;
    std::map<Key,sk_sp<SkTypeface>> faces;
};
SkiaTextResources::SkiaTextResources() : impl_(std::make_unique<Impl>()) {}
SkiaTextResources::~SkiaTextResources()=default;
void SkiaTextResources::draw(SkCanvas& canvas,const text::TextLayoutSnapshot& snapshot) {
    for(const auto& run:snapshot.runs) {
        if(run.font>=snapshot.fonts.size() || run.glyphs.size()!=run.positions.size()) continue;
        const auto& f=snapshot.fonts[run.font];
        if(!f.bytes) continue;
        auto& face=impl_->faces[{f.sha256,f.collectionIndex,f.weight,f.italic,f.syntheticStyle}];
        if(!face) face=text::internal::exactFace(f);
        if(!face) continue;
        SkFont font(face,run.fontSize); font.setHinting(SkFontHinting::kNone);
        // drawGlyphs consumes a SkFont, so carry the admitted synthetic style
        // into that font instead of relying on the cloned typeface metadata.
        if (f.syntheticStyle) {
            font.setEmbolden(f.weight >= 700 && face->fontStyle().weight() < 700);
            font.setSkewX(f.italic && face->fontStyle().slant() == SkFontStyle::kUpright_Slant
                              ? -0.25F : 0.0F);
        }
        font.setSubpixel(true); font.setEdging(SkFont::Edging::kAntiAlias);
        std::vector<SkPoint> positions; positions.reserve(run.positions.size());
        for(const auto& p:run.positions) positions.push_back(SkPoint::Make(p.x,p.y));
        SkPaint paint; paint.setAntiAlias(true);
        paint.setColor4f({run.color.r,run.color.g,run.color.b,run.color.a});
        canvas.drawGlyphs(run.glyphs,positions,{0,0},font,paint);
    }
    for(const auto& p:snapshot.paragraphs) if(p.bulleted) {
        SkPaint bullet;bullet.setAntiAlias(true);bullet.setColor(SK_ColorBLACK);bullet.setStyle(SkPaint::kFill_Style);
        const auto line=std::find_if(snapshot.lines.begin(),snapshot.lines.end(),[&](const auto& l){return l.paragraphId==p.paragraphId;});
        const auto y=line==snapshot.lines.end()?(p.bounds.top+p.bounds.bottom)*0.5F:(line->bounds.top+line->bounds.bottom)*0.5F;
        canvas.drawCircle(p.indent/3.0F,y,2.5F,bullet);
    }
    for(const auto& decoration:snapshot.decorations) {
        const auto& c=decoration.color; const auto& r=decoration.bounds;
        SkPaint paint; paint.setAntiAlias(true); paint.setColor4f({c.r,c.g,c.b,c.a});
        canvas.drawRect(SkRect::MakeLTRB(r.left,r.top,r.right,r.bottom),paint);
    }
}
}
