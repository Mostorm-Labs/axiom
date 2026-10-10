#include "canvas/text/text_layout.hpp"
#include "sha256.h"

#include <algorithm>
#include <bit>
#include <chrono>
#include <cmath>
#include <map>
#include <span>
#include <type_traits>
#include <utility>

#ifdef CANVAS_TEXT_HAS_SKIA
#include "skia_font_materialization.hpp"
#include "include/core/SkData.h"
#include "include/core/SkFontArguments.h"
#include "include/core/SkFontMgr.h"
#include "include/core/SkTypeface.h"
#include "include/ports/SkFontMgr_data.h"
#include "modules/skparagraph/include/FontCollection.h"
#include "modules/skparagraph/include/ParagraphBuilder.h"
#include "modules/skparagraph/include/ParagraphPainter.h"
#include "modules/skparagraph/include/TypefaceFontProvider.h"
#include "modules/skunicode/include/SkUnicode_icu.h"
#endif

namespace canvas::text {
namespace {
// Fixed little-endian encoding, independent of native object padding or locale.
struct Encoding {
    std::vector<std::uint8_t> bytes;
    template<class T> void number(T value) {
        if constexpr (std::is_floating_point_v<T>) {
            if (value == 0) value = 0; // canonicalize negative zero
            if constexpr (sizeof(T) == 4) number(std::bit_cast<std::uint32_t>(value));
            else number(std::bit_cast<std::uint64_t>(value));
        } else {
            const auto bits = static_cast<std::uint64_t>(value);
            for (std::size_t i = 0; i < sizeof(T); ++i)
                bytes.push_back(static_cast<std::uint8_t>(bits >> (i*8)));
        }
    }
    void id(foundation::ObjectId value) { bytes.insert(bytes.end(), value.bytes.begin(), value.bytes.end()); }
    void string(std::string_view value) {
        number<std::uint64_t>(value.size()); bytes.insert(bytes.end(), value.begin(), value.end());
    }
    void color(semantic::ColorValue c) { number(c.r); number(c.g); number(c.b); number(c.a); }
    void rect(foundation::WorldRect r) { number(r.left); number(r.top); number(r.right); number(r.bottom); }
    std::string digest() const {
        internal::Sha256 hash; hash.Update(bytes); const auto result = hash.Finish();
        constexpr char hex[] = "0123456789abcdef"; std::string output;
        for (auto byte : result) { output += hex[byte>>4]; output += hex[byte&15]; }
        return output;
    }
};
void encodeParagraph(Encoding& e, const semantic::Paragraph& p) {
    e.id(p.id); e.number(p.style.alignment); e.number(p.style.line_height);
    e.number(p.style.spacing_before); e.number(p.style.spacing_after);
    e.number<std::uint64_t>(p.runs.size());
    for (const auto& r : p.runs) {
        e.string(r.text); e.number<std::uint8_t>(r.style.font_resource_id.has_value());
        if (r.style.font_resource_id) e.id(r.style.font_resource_id->value);
        e.number(r.style.font_size); e.number(r.style.weight);
        e.number<std::uint8_t>(r.style.italic); e.number<std::uint8_t>(r.style.underline); e.color(r.style.color);
    }
}
std::string snapshotDigest(const TextLayoutSnapshot& s) {
    Encoding e; e.string(s.profile); e.string(s.contentDigest);
    e.number(s.context.width); e.number(s.context.revision); e.rect(s.localBounds);
    e.number<std::uint64_t>(s.fonts.size());
    for (const auto& f : s.fonts) {
        e.id(f.resourceId.value); e.number(f.generation); e.string(f.sha256);
        e.number(f.collectionIndex); e.number(f.weight); e.number<std::uint8_t>(f.italic);
    }
    e.number<std::uint64_t>(s.paragraphs.size());
    for (const auto& p : s.paragraphs) { e.id(p.paragraphId); e.rect(p.bounds); e.string(p.digest); }
    e.number<std::uint64_t>(s.lines.size());
    for (const auto& l : s.lines) {
        e.id(l.paragraphId); e.number<std::uint64_t>(l.utf8Start); e.number<std::uint64_t>(l.utf8End);
        e.rect(l.bounds); e.number(l.baseline);
    }
    e.number<std::uint64_t>(s.runs.size());
    for (const auto& r : s.runs) {
        e.number<std::uint64_t>(r.font); e.number(r.fontSize); e.color(r.color); e.id(r.paragraphId);
        e.number<std::uint64_t>(r.glyphs.size());
        for (std::size_t i=0; i<r.glyphs.size(); ++i) {
            e.number(r.glyphs[i]); e.number(r.positions[i].x); e.number(r.positions[i].y); e.number(r.utf8Clusters[i]);
        }
    }
    e.number<std::uint64_t>(s.decorations.size());
    for (const auto& d : s.decorations) { e.rect(d.bounds); e.color(d.color); }
    e.number<std::uint64_t>(s.diagnostics.size());
    for (const auto& d : s.diagnostics) e.string(d);
    return e.digest();
}
foundation::WorldRect offset(foundation::WorldRect r, float y) {
    r.top += y; r.bottom += y; return r;
}
#ifdef CANVAS_TEXT_HAS_SKIA
using namespace skia::textlayout;

// Capture the SDK's solid underline geometry, rather than approximate it in a
// renderer. Text glyphs are captured separately by Paragraph::visit.
class DecorationCapture final : public ParagraphPainter {
  public:
    explicit DecorationCapture(TextLayoutSnapshot& s) : output(s) {}
    void drawTextBlob(const sk_sp<SkTextBlob>&,SkScalar,SkScalar,const SkPaintOrID&) override {}
    void drawTextShadow(const sk_sp<SkTextBlob>&,SkScalar,SkScalar,SkColor,SkScalar) override {}
    void drawRect(const SkRect&,const SkPaintOrID&) override {}
    void drawFilledRect(const SkRect& r,const DecorationStyle& style) override { add(r,style.getColor()); }
    void drawPath(const SkPath&,const DecorationStyle&) override { output.diagnostics.push_back("unsupported-decoration-path"); }
    void drawLine(SkScalar x0,SkScalar y0,SkScalar x1,SkScalar y1,const DecorationStyle& style) override {
        const float half=style.getStrokeWidth()/2;
        add(SkRect::MakeLTRB(std::min(x0,x1),std::min(y0,y1)-half,
                            std::max(x0,x1),std::max(y0,y1)+half),style.getColor());
    }
    void clipRect(const SkRect&) override {}
    void translate(SkScalar x,SkScalar y) override { position.x+=x; position.y+=y; }
    void save() override { stack.push_back(position); }
    void restore() override { position=stack.back(); stack.pop_back(); }
  private:
    void add(const SkRect& r,SkColor c) {
        const auto color = SkColor4f::FromColor(c);
        output.decorations.push_back({{r.left()+position.x,r.top()+position.y,
                                      r.right()+position.x,r.bottom()+position.y},
                                     {color.fR,color.fG,color.fB,color.fA}});
    }
    TextLayoutSnapshot& output; GlyphPosition position{}; std::vector<GlyphPosition> stack;
};
#endif
} // namespace

struct RichTextLayoutService::Impl {
    const runtime::ResourceProvider& resources;
    LayoutContext context;
    LayoutMetrics counters;
    std::map<foundation::ObjectId,FontResourceDescriptor> descriptors;
    struct Cached { std::string key; std::shared_ptr<const TextLayoutSnapshot> snapshot; };
    std::map<foundation::ObjectId,Cached> objects;
    std::map<std::pair<foundation::ObjectId,foundation::ObjectId>,Cached> paragraphs;
#ifdef CANVAS_TEXT_HAS_SKIA
    struct Face { FontObservation observation; sk_sp<SkTypeface> face; std::string diagnostic; };
    std::map<std::string,Face> faces;
    sk_sp<SkUnicode> unicode = SkUnicodes::ICU::Make();
#endif
    Impl(const runtime::ResourceProvider& r,LayoutContext c) : resources(r), context(c) {}
    void dependencies(Encoding& key,const semantic::Paragraph& p) const {
        for(const auto& run:p.runs) {
            if(!run.style.font_resource_id) continue;
            const auto id=run.style.font_resource_id->value;
            key.number(resources.resolve(id).generation);
            const auto descriptor=descriptors.find(id);
            if(descriptor!=descriptors.end()) {
                key.string(descriptor->second.sha256); key.number(descriptor->second.collectionIndex);
            } else key.string("unregistered-font");
        }
    }
#ifdef CANVAS_TEXT_HAS_SKIA
    Face& font(const semantic::TextStyle& style) {
        Encoding key; key.number(style.weight); key.number<std::uint8_t>(style.italic);
        if(style.font_resource_id) key.id(style.font_resource_id->value);
        const auto resource=style.font_resource_id ? resources.resolve(style.font_resource_id->value) : runtime::EncodedResource{};
        key.number(resource.generation);
        const auto descriptor=style.font_resource_id ? descriptors.find(style.font_resource_id->value) : descriptors.end();
        if(descriptor!=descriptors.end()) { key.string(descriptor->second.sha256); key.number(descriptor->second.collectionIndex); }
        const auto token=key.digest();
        if(auto found=faces.find(token);found!=faces.end()) return found->second;
        Face result;
        result.observation = {style.font_resource_id.value_or(semantic::ResourceId{}),resource.generation,
            descriptor==descriptors.end() ? std::string{} : descriptor->second.sha256,
            descriptor==descriptors.end() ? 0 : descriptor->second.collectionIndex,style.weight,style.italic,resource.bytes};
        if(descriptor==descriptors.end()) result.diagnostic="font-identity-unregistered";
        else if(!resource.bytes || resource.bytes->empty()) result.diagnostic="font-resource-missing";
        else {
            Encoding data; data.bytes=*resource.bytes;
            if(data.digest()!=descriptor->second.sha256) result.diagnostic="font-integrity-failed";
            else {
                result.face=internal::exactFace(result.observation);
                if(!result.face) result.diagnostic="font-exact-face-unavailable";
                else ++counters.fontMaterializations;
            }
        }
        return faces.emplace(token,std::move(result)).first->second;
    }
#endif
    std::shared_ptr<const TextLayoutSnapshot> layoutParagraph(const semantic::Paragraph& p) {
        auto out=std::make_shared<TextLayoutSnapshot>(); out->context=context;
        Encoding content; encodeParagraph(content,p); out->contentDigest=content.digest();
#ifndef CANVAS_TEXT_HAS_SKIA
        out->diagnostics.push_back("text-stack-unavailable");
#else
        if(!unicode) out->diagnostics.push_back("text-unicode-unavailable");
        auto provider=sk_make_sp<TypefaceFontProvider>();
        std::vector<Face*> resolved;
        for(const auto& run:p.runs) {
            auto& f=font(run.style); resolved.push_back(&f); out->fonts.push_back(f.observation);
            if(!f.diagnostic.empty()) out->diagnostics.push_back(f.diagnostic);
            else provider->registerTypeface(f.face,SkString(std::to_string(resolved.size()-1).c_str()));
            if(!std::isfinite(run.style.font_size) || run.style.font_size<=0 || run.style.font_size>65536)
                out->diagnostics.push_back("invalid-font-size");
        }
        if(out->diagnostics.empty()) {
            auto fonts=sk_make_sp<FontCollection>();
            fonts->setAssetFontManager(provider); fonts->disableFontFallback();
            ParagraphStyle style; style.turnHintingOff(); style.setFakeMissingFontStyles(false);
            switch(p.style.alignment) {
                case semantic::ParagraphAlignment::kCenter: style.setTextAlign(TextAlign::kCenter); break;
                case semantic::ParagraphAlignment::kRight: style.setTextAlign(TextAlign::kRight); break;
                case semantic::ParagraphAlignment::kJustify: style.setTextAlign(TextAlign::kJustify); break;
                default: style.setTextAlign(TextAlign::kLeft); break;
            }
            auto builder=ParagraphBuilder::make(style,fonts,unicode);
            std::vector<std::size_t> starts; std::size_t byteStart=0;
            for(std::size_t i=0;i<p.runs.size();++i) {
                const auto& r=p.runs[i]; starts.push_back(byteStart); byteStart+=r.text.size();
                TextStyle text; text.setFontFamilies({SkString(std::to_string(i).c_str())});
                text.setTypeface(resolved[i]->face); text.setFontStyle(resolved[i]->face->fontStyle());
                text.setFontSize(static_cast<float>(r.style.font_size));
                text.setColor(SkColor4f{r.style.color.r,r.style.color.g,r.style.color.b,r.style.color.a}.toSkColor());
                text.setHeight(static_cast<float>(p.style.line_height)); text.setHeightOverride(p.style.line_height>0);
                text.setSubpixel(true); text.setFontHinting(SkFontHinting::kNone);
                if(r.style.underline) text.setDecoration(skia::textlayout::TextDecoration::kUnderline);
                builder->pushStyle(text); builder->addText(r.text.data(),r.text.size()); builder->pop();
            }
            auto paragraph=builder->Build(); paragraph->layout(context.width);
            if(paragraph->unresolvedGlyphs()>0) out->diagnostics.push_back("font-glyph-unavailable");
            std::vector<LineMetrics> lines; paragraph->getLineMetrics(lines);
            foundation::WorldRect bounds{}; bool have=false;
            auto contribute=[&](foundation::WorldRect r) { bounds=have?foundation::unionRects(bounds,r):r; have=true; };
            for(const auto& l:lines) {
                const foundation::WorldRect r{static_cast<float>(l.fLeft),static_cast<float>(l.fBaseline-l.fAscent),
                    static_cast<float>(l.fLeft+l.fWidth),static_cast<float>(l.fBaseline+l.fDescent)};
                out->lines.push_back({p.id,l.fStartIndex,l.fEndIndex,r,static_cast<float>(l.fBaseline)}); contribute(r);
            }
            paragraph->visit([&](int,const Paragraph::VisitorInfo* info) {
                if(!info || info->count<=0 || starts.empty()) return;
                std::size_t current=static_cast<std::size_t>(-1);
                for(int g=0;g<info->count;++g) {
                    const auto cluster=info->utf8Starts[g];
                    const auto upper=std::upper_bound(starts.begin(),starts.end(),cluster);
                    const auto index=upper==starts.begin()?0:static_cast<std::size_t>(upper-starts.begin()-1);
                    // No guessed font matching: shaped glyphs must come from the exact admitted face.
                    if(info->font.getTypeface()->uniqueID()!=resolved[index]->face->uniqueID()) {
                        out->diagnostics.push_back("font-shaped-face-mismatch"); return;
                    }
                    if(index!=current) {
                        out->runs.push_back({index,info->font.getSize(),p.runs[index].style.color,p.id,{},{},{}}); current=index;
                    }
                    auto& run=out->runs.back(); const auto position=info->origin+info->positions[g];
                    run.glyphs.push_back(info->glyphs[g]); run.positions.push_back({position.x(),position.y()});
                    run.utf8Clusters.push_back(cluster);
                    const auto box=info->font.getBounds(info->glyphs[g],nullptr);
                    if(!box.isEmpty()) contribute({box.left()+position.x(),box.top()+position.y(),
                                                   box.right()+position.x(),box.bottom()+position.y()});
                }
            });
            DecorationCapture painter(*out); paragraph->paint(&painter,0,0);
            for(const auto& d:out->decorations) contribute(d.bounds);
            out->localBounds=bounds;
            // Include line advance height even for blank paragraphs.
            out->localBounds.bottom=std::max(out->localBounds.bottom,paragraph->getHeight());
        }
#endif
        out->digest=snapshotDigest(*out); return out;
    }
};
RichTextLayoutService::RichTextLayoutService(const runtime::ResourceProvider& r, LayoutContext c)
    : impl_(std::make_unique<Impl>(r,c)) {}
RichTextLayoutService::~RichTextLayoutService() = default;
void RichTextLayoutService::registerFont(FontResourceDescriptor d) { impl_->descriptors[d.resourceId.value]=std::move(d); }
void RichTextLayoutService::setContext(LayoutContext c) { impl_->context=c; }
std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolve(
    foundation::ObjectId id, const semantic::RichTextContent& content) {
    auto& state=*impl_; ++state.counters.requests;
    Encoding semantic; semantic.number<std::uint64_t>(content.document.paragraphs.size());
    for(const auto& p:content.document.paragraphs) encodeParagraph(semantic,p);
    const auto contentDigest=semantic.digest();
    Encoding key=semantic; key.string(kTextStackProfile); key.number(state.context.width); key.number(state.context.revision);
    for(const auto& p:content.document.paragraphs) state.dependencies(key,p);
    const auto token=key.digest();
    if(auto found=state.objects.find(id);found!=state.objects.end() && found->second.key==token) {
        ++state.counters.cacheHits; return found->second.snapshot;
    }
    const auto start=std::chrono::steady_clock::now();
    auto out=std::make_shared<TextLayoutSnapshot>(); out->contentDigest=contentDigest; out->context=state.context;
    if(!std::isfinite(state.context.width) || state.context.width<=0) out->diagnostics.push_back("invalid-layout-context");
    else {
        float y=0; bool have=false;
        for(const auto& p:content.document.paragraphs) {
            Encoding paragraphKey; encodeParagraph(paragraphKey,p); state.dependencies(paragraphKey,p);
            paragraphKey.number(state.context.width); paragraphKey.number(state.context.revision);
            const auto hash=paragraphKey.digest(); auto& cached=state.paragraphs[{id,p.id}];
            if(cached.key!=hash || !cached.snapshot) {
                cached={hash,state.layoutParagraph(p)}; ++state.counters.paragraphLayouts;
            } else ++state.counters.paragraphCacheHits;
            const auto& fragment=*cached.snapshot;
            y+=static_cast<float>(p.style.spacing_before);
            const auto fontOffset=out->fonts.size(); out->fonts.insert(out->fonts.end(),fragment.fonts.begin(),fragment.fonts.end());
            const auto bounds=offset(fragment.localBounds,y);
            out->paragraphs.push_back({p.id,bounds,fragment.digest});
            out->localBounds=have?foundation::unionRects(out->localBounds,bounds):bounds; have=true;
            for(auto l:fragment.lines) { l.bounds=offset(l.bounds,y); l.baseline+=y; out->lines.push_back(l); }
            for(auto r:fragment.runs) { r.font+=fontOffset; for(auto& pos:r.positions) pos.y+=y; out->runs.push_back(std::move(r)); }
            for(auto d:fragment.decorations) { d.bounds=offset(d.bounds,y); out->decorations.push_back(d); }
            out->diagnostics.insert(out->diagnostics.end(),fragment.diagnostics.begin(),fragment.diagnostics.end());
            y+=fragment.localBounds.bottom+static_cast<float>(p.style.spacing_after);
        }
    }
    out->digest=snapshotDigest(*out); state.objects[id]={token,out}; ++state.counters.objectLayouts;
    state.counters.layoutCpuMs+=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    return out;
}
LayoutMetrics RichTextLayoutService::metrics() const noexcept { return impl_->counters; }
}
