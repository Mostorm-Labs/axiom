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
    e.number(p.style.spacing_before); e.number(p.style.spacing_after); e.number<std::uint8_t>(p.style.bulleted);
    e.number<std::uint64_t>(p.runs.size());
    for (const auto& r : p.runs) {
        e.string(r.text); e.number<std::uint8_t>(r.style.font_resource_id.has_value());
        if (r.style.font_resource_id) e.id(r.style.font_resource_id->value);
        e.number(r.style.font_size); e.number(r.style.weight);
        e.number<std::uint8_t>(r.style.italic); e.number<std::uint8_t>(r.style.underline); e.number<std::uint8_t>(r.style.strikethrough); e.color(r.style.color);
    }
}
std::string snapshotDigest(const TextLayoutSnapshot& s) {
    Encoding e; e.string(s.profile); e.string(s.contentDigest);
    e.number(s.context.width); e.number(s.context.revision); e.number(s.context.height);
    e.number<std::uint64_t>(s.context.maxLines); e.rect(s.localBounds);
    e.number<std::uint8_t>(s.shapeLabel); e.number<std::uint8_t>(s.placeholder);
    e.number<std::uint8_t>(s.overflow); e.number(s.labelScaleX); e.number(s.labelScaleY);
    e.rect(s.labelViewport); e.rect(s.expandButton); e.number(s.labelOffsetX); e.number(s.labelOffsetY);
    e.number<std::uint64_t>(s.fonts.size());
    for (const auto& f : s.fonts) {
        e.id(f.resourceId.value); e.number(f.generation); e.string(f.sha256);
        e.number(f.collectionIndex); e.number(f.weight); e.number<std::uint8_t>(f.italic); e.number<std::uint8_t>(f.syntheticStyle);
    }
    e.number<std::uint64_t>(s.paragraphs.size());
    for (const auto& p : s.paragraphs) { e.id(p.paragraphId); e.rect(p.bounds); e.string(p.digest); e.number<std::uint8_t>(p.bulleted); e.number(p.indent); e.number(p.originY); }
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
    std::shared_ptr<Paragraph> lastShaped;
    std::map<std::pair<foundation::ObjectId,foundation::ObjectId>,std::shared_ptr<Paragraph>> shaped;
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
    Face& font(const semantic::TextStyle& style, bool allowSyntheticStyle) {
        Encoding key; key.number(style.weight); key.number<std::uint8_t>(style.italic);
        key.number<std::uint8_t>(allowSyntheticStyle);
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
                if(!result.face && allowSyntheticStyle && (style.weight==700 || style.italic)) {
                    result.observation.syntheticStyle=true;result.face=internal::exactFace(result.observation);
                }
                if(!result.face) result.diagnostic="font-exact-face-unavailable";
                else ++counters.fontMaterializations;
            }
        }
        return faces.emplace(token,std::move(result)).first->second;
    }
#endif
    std::shared_ptr<const TextLayoutSnapshot> layoutParagraph(const semantic::Paragraph& p,LayoutContext layoutContext,bool allowSyntheticStyle) {
        auto out=std::make_shared<TextLayoutSnapshot>(); out->context=layoutContext;
        Encoding content; encodeParagraph(content,p); out->contentDigest=content.digest();
#ifndef CANVAS_TEXT_HAS_SKIA
        (void)allowSyntheticStyle;
        out->diagnostics.push_back("text-stack-unavailable");
#else
        lastShaped.reset();
        if(!unicode) out->diagnostics.push_back("text-unicode-unavailable");
        auto provider=sk_make_sp<TypefaceFontProvider>();
        std::vector<Face*> resolved;
        for(const auto& run:p.runs) {
            auto& f=font(run.style,allowSyntheticStyle); resolved.push_back(&f); out->fonts.push_back(f.observation);
            if(!f.diagnostic.empty()) out->diagnostics.push_back(f.diagnostic);
            else provider->registerTypeface(f.face,SkString(std::to_string(resolved.size()-1).c_str()));
            if(!std::isfinite(run.style.font_size) || run.style.font_size<=0 || run.style.font_size>65536)
                out->diagnostics.push_back("invalid-font-size");
        }
        if(out->diagnostics.empty()) {
            auto fonts=sk_make_sp<FontCollection>();
            fonts->setAssetFontManager(provider); fonts->disableFontFallback();
            ParagraphStyle style; style.turnHintingOff(); style.setFakeMissingFontStyles(false);
            if (layoutContext.maxLines != 0) {
                style.setMaxLines(layoutContext.maxLines);
                style.setEllipsis(std::u16string(u"\u2026"));
            }
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
                if(r.style.underline || r.style.strikethrough) {
                    auto decoration=skia::textlayout::TextDecoration::kNoDecoration;
                    if(r.style.underline)decoration=static_cast<decltype(decoration)>(decoration|skia::textlayout::TextDecoration::kUnderline);
                    if(r.style.strikethrough)decoration=static_cast<decltype(decoration)>(decoration|skia::textlayout::TextDecoration::kLineThrough);
                    text.setDecoration(decoration);
                }
                builder->pushStyle(text); builder->addText(r.text.data(),r.text.size()); builder->pop();
            }
            const float indent=p.style.bulleted ? std::min(layoutContext.width*0.5F,static_cast<float>(p.runs.empty()?24:p.runs.front().style.font_size*1.5)) : 0;
            auto paragraph=builder->Build(); paragraph->layout(layoutContext.width-indent);
            out->overflow = paragraph->didExceedMaxLines();
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
            if(indent>0) {
                out->localBounds.left+=indent;out->localBounds.right+=indent;
                for(auto& line:out->lines){line.bounds.left+=indent;line.bounds.right+=indent;}
                for(auto& run:out->runs)for(auto& point:run.positions)point.x+=indent;
                for(auto& decoration:out->decorations){decoration.bounds.left+=indent;decoration.bounds.right+=indent;}
            }
            lastShaped=std::move(paragraph);
        }
#endif
        out->digest=snapshotDigest(*out); return out;
    }
};
RichTextLayoutService::RichTextLayoutService(const runtime::ResourceProvider& r, LayoutContext c)
    : impl_(std::make_unique<Impl>(r,c)) {}
RichTextLayoutService::~RichTextLayoutService() = default;
std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolveSectionTitle(
    foundation::ObjectId id,const semantic::SectionContent& section) {
    // Plain metadata presentation, with the same deterministic admitted-font
    // fallback used by existing labels. This never creates canonical RichText.
    semantic::RichTextContent display;
    semantic::Paragraph paragraph;paragraph.id=id;
    paragraph.style.alignment=semantic::ParagraphAlignment::kLeft;paragraph.style.line_height=1.2;
    semantic::TextStyle style;style.font_size=16;style.weight=400;style.color={.15F,.2F,.25F,1};
    if(!impl_->descriptors.empty())style.font_resource_id=impl_->descriptors.begin()->second.resourceId;
    paragraph.runs.push_back({section.title,style});display.document.paragraphs.push_back(std::move(paragraph));
    display.layout=semantic::RichTextLayout{std::max(1.0,section.boundary.width-2*section.padding)};
    return resolve(id,display);
}
void RichTextLayoutService::registerFont(FontResourceDescriptor d) { impl_->descriptors[d.resourceId.value]=std::move(d); }
std::string RichTextLayoutService::fontFamilyName(foundation::ObjectId id) {
#ifdef CANVAS_TEXT_HAS_SKIA
    semantic::TextStyle style;
    style.font_resource_id = semantic::ResourceId{id};
    style.weight = 400;
    auto& resolved = impl_->font(style, false);
    if (!resolved.face) return {};
    SkString name;
    resolved.face->getFamilyName(&name);
    return {name.c_str(), name.size()};
#else
    (void)id;
    return {};
#endif
}
void RichTextLayoutService::clearFonts() noexcept {
    impl_->descriptors.clear();
    impl_->objects.clear();
    impl_->paragraphs.clear();
#ifdef CANVAS_TEXT_HAS_SKIA
    impl_->faces.clear();
    impl_->shaped.clear();
#endif
}
void RichTextLayoutService::setContext(LayoutContext c) { impl_->context=c; }
std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolve(
    foundation::ObjectId id, const semantic::RichTextContent& content) {
    auto& state=*impl_; ++state.counters.requests;
    auto context=state.context;if(content.layout)context.width=static_cast<float>(content.layout->wrap_width);
    Encoding semantic; if(content.layout){semantic.string("fixed-width-v2");semantic.number(content.layout->wrap_width);}
    semantic.number<std::uint64_t>(content.document.paragraphs.size());
    for(const auto& p:content.document.paragraphs) encodeParagraph(semantic,p);
    const auto contentDigest=semantic.digest();
    Encoding key=semantic; key.string(kTextStackProfile); key.number(context.width); key.number(context.revision);
    key.number(context.height); key.number<std::uint64_t>(context.maxLines);
    for(const auto& p:content.document.paragraphs) state.dependencies(key,p);
    const auto token=key.digest();
    if(auto found=state.objects.find(id);found!=state.objects.end() && found->second.key==token) {
        ++state.counters.cacheHits; return found->second.snapshot;
    }
    const auto start=std::chrono::steady_clock::now();
    auto out=std::make_shared<TextLayoutSnapshot>(); out->contentDigest=contentDigest; out->context=context;
    if(!std::isfinite(context.width) || context.width<=0) out->diagnostics.push_back("invalid-layout-context");
    else {
        float y=0; bool have=false;
        std::size_t remainingLines = context.maxLines;
        for(const auto& p:content.document.paragraphs) {
            if (context.maxLines && remainingLines == 0) { out->overflow = true; break; }
            auto paragraphContext = context;
            if (context.maxLines) paragraphContext.maxLines = remainingLines;
            Encoding paragraphKey; encodeParagraph(paragraphKey,p); state.dependencies(paragraphKey,p);
            paragraphKey.number<std::uint8_t>(content.layout.has_value());
            paragraphKey.number(context.width); paragraphKey.number(context.revision);
            paragraphKey.number(context.height); paragraphKey.number<std::uint64_t>(paragraphContext.maxLines);
            const auto hash=paragraphKey.digest(); auto& cached=state.paragraphs[{id,p.id}];
            if(cached.key!=hash || !cached.snapshot) {
                cached={hash,state.layoutParagraph(p,paragraphContext,content.layout.has_value())}; ++state.counters.paragraphLayouts;
#ifdef CANVAS_TEXT_HAS_SKIA
                state.shaped[{id,p.id}]=state.lastShaped;
#endif
            } else ++state.counters.paragraphCacheHits;
            const auto& fragment=*cached.snapshot;
            if (context.maxLines) remainingLines -= std::min(remainingLines, fragment.lines.size());
            out->overflow = out->overflow || fragment.overflow;
            y+=static_cast<float>(p.style.spacing_before);
            const auto fontOffset=out->fonts.size(); out->fonts.insert(out->fonts.end(),fragment.fonts.begin(),fragment.fonts.end());
            const auto bounds=offset(fragment.localBounds,y);
            const float indent=p.style.bulleted?std::min(context.width*0.5F,static_cast<float>(p.runs.empty()?24:p.runs.front().style.font_size*1.5)):0;
            out->paragraphs.push_back({p.id,bounds,fragment.digest,p.style.bulleted,indent,y});
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

std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolveNatural(
    foundation::ObjectId id, const semantic::RichTextContent& content) {
    const auto previous = impl_->context;
    impl_->context.height = 0;
    impl_->context.maxLines = 0;
    impl_->context.width = 4096.0F;
    auto natural = content;
    natural.layout = semantic::RichTextLayout{4096.0};
    auto result = resolve(id, natural);
    impl_->context = previous;
    return result;
}

namespace {
float basisX(const semantic::Transform2D& transform) {
    return std::max(0.001F, std::hypot(static_cast<float>(transform.a), static_cast<float>(transform.b)));
}
float basisY(const semantic::Transform2D& transform) {
    return std::max(0.001F, std::hypot(static_cast<float>(transform.c), static_cast<float>(transform.d)));
}
// Use the same inscribed region for wrapping, paint and input geometry.
// The triangle's lower half provides room around its visual center.
foundation::WorldRect shapeLabelViewport(unsigned kind, float width, float height) {
    constexpr float padding = 8.0F;
    if (kind == 2U) {
        constexpr float diagonal = 0.70710678F;
        const float halfWidth = std::max(0.0F, width * 0.5F - padding) * diagonal;
        const float halfHeight = std::max(0.0F, height * 0.5F - padding) * diagonal;
        return {width * 0.5F - halfWidth, height * 0.5F - halfHeight,
                width * 0.5F + halfWidth, height * 0.5F + halfHeight};
    }
    if (kind == 3U) {
        const float halfWidth = std::max(0.0F, width * 0.25F - padding);
        const float halfHeight = std::max(0.0F, height * 0.25F - padding);
        return {width * 0.5F - halfWidth, height * 0.75F - halfHeight,
                width * 0.5F + halfWidth, height * 0.75F + halfHeight};
    }
    return {std::min(padding, width * 0.5F), std::min(padding, height * 0.5F),
            std::max(width - padding, width * 0.5F),
            std::max(height - padding, height * 0.5F)};
}
void offsetSnapshot(TextLayoutSnapshot& snapshot, float x, float y) {
    snapshot.localBounds.left += x; snapshot.localBounds.right += x;
    snapshot.localBounds.top += y; snapshot.localBounds.bottom += y;
    for (auto& paragraph : snapshot.paragraphs) {
        paragraph.bounds.left += x; paragraph.bounds.right += x;
        paragraph.bounds.top += y; paragraph.bounds.bottom += y;
        paragraph.originY += y;
    }
    for (auto& line : snapshot.lines) {
        line.bounds.left += x; line.bounds.right += x;
        line.bounds.top += y; line.bounds.bottom += y; line.baseline += y;
    }
    for (auto& run : snapshot.runs)
        for (auto& point : run.positions) { point.x += x; point.y += y; }
    for (auto& decoration : snapshot.decorations) {
        decoration.bounds.left += x; decoration.bounds.right += x;
        decoration.bounds.top += y; decoration.bounds.bottom += y;
    }
}
}

std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolveShapeNatural(
    foundation::ObjectId id, const semantic::RichTextContent& content,
    const semantic::ShapeContent& shape, const semantic::Transform2D& transform) {
    const float sx = basisX(transform), sy = basisY(transform);
    const float width = static_cast<float>(shape.width) * sx;
    const float height = static_cast<float>(shape.height) * sy;
    const auto viewport = shapeLabelViewport(shape.shape_kind, width, height);
    const float innerWidth = std::max(1.0F, viewport.right - viewport.left);
    auto natural = content;
    natural.layout = semantic::RichTextLayout{innerWidth};
    const auto previous = impl_->context;
    impl_->context.width = innerWidth;
    impl_->context.height = 0;
    impl_->context.maxLines = 0;
    const auto base = resolve(id, natural);
    impl_->context = previous;
    auto result = std::make_shared<TextLayoutSnapshot>(*base);
    result->shapeLabel = true;
    result->labelScaleX = 1.0F / sx;
    result->labelScaleY = 1.0F / sy;
    result->labelViewport = viewport;
    result->labelOffsetX = viewport.left;
    result->labelOffsetY = viewport.top;
    offsetSnapshot(*result, result->labelOffsetX, result->labelOffsetY);
    result->digest = snapshotDigest(*result);
    return result;
}

std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolveShapeLabel(
    foundation::ObjectId id, const semantic::RichTextContent& canonical,
    const semantic::ShapeContent& shape, const semantic::Transform2D& transform,
    bool showPlaceholder) {
    constexpr float padding = 8.0F;
    const float sx = basisX(transform), sy = basisY(transform);
    const float width = static_cast<float>(shape.width) * sx;
    const float height = static_cast<float>(shape.height) * sy;
    semantic::RichTextContent display = canonical;
    bool emptyLabel = true;
    for (const auto& paragraph : display.document.paragraphs)
        for (const auto& run : paragraph.runs) emptyLabel = emptyLabel && run.text.empty();
    const bool placeholder = showPlaceholder && emptyLabel;
    if (placeholder) {
        semantic::TextStyle style;
        style.font_size = 16; style.weight = 400; style.color = {0.25F, 0.25F, 0.25F, 1};
        if (!impl_->descriptors.empty()) style.font_resource_id = impl_->descriptors.begin()->second.resourceId;
        semantic::Paragraph paragraph;
        paragraph.id = foundation::ObjectId::fromUint64(0x5459504554455854ULL);
        paragraph.style.alignment = semantic::ParagraphAlignment::kCenter;
        paragraph.runs = {{"Type text...", style}};
        display.document.paragraphs = {std::move(paragraph)};
    }
    const auto viewport = shapeLabelViewport(shape.shape_kind, width, height);
    const float viewportWidth = std::max(1.0F, viewport.right - viewport.left);
    const float viewportHeight = std::max(1.0F, viewport.bottom - viewport.top);
    display.layout = semantic::RichTextLayout{viewportWidth};
    const auto previous = impl_->context;
    impl_->context.width = viewportWidth;
    impl_->context.height = 0;
    impl_->context.maxLines = 0;
    const auto natural = resolve(id, display);
    const bool overflow = natural->localBounds.bottom > viewportHeight + 0.5F;
    impl_->context.height = viewportHeight;
    if (overflow) {
        std::size_t visibleLines = 0;
        for (const auto& line : natural->lines) {
            if (line.bounds.bottom > viewportHeight + 0.5F) break;
            ++visibleLines;
        }
        impl_->context.maxLines = std::max<std::size_t>(1, visibleLines);
    }
    const auto base = resolve(id, display);
    impl_->context = previous;
    auto result = std::make_shared<TextLayoutSnapshot>(*base);
    result->shapeLabel = true;
    result->placeholder = placeholder;
    result->labelScaleX = 1.0F / sx;
    result->labelScaleY = 1.0F / sy;
    result->labelViewport = viewport;
    result->expandButton = {width - 24.0F, height - 24.0F, width - padding, height - padding};
    result->overflow = result->overflow || overflow;
    result->labelOffsetX = viewport.left;
    result->labelOffsetY = viewport.top + std::max(0.0F, (viewportHeight - base->localBounds.bottom) * 0.5F);
    offsetSnapshot(*result, result->labelOffsetX, result->labelOffsetY);
    result->digest = snapshotDigest(*result);
    return result;
}

std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolveShape(
    foundation::ObjectId id, const semantic::ShapeContent& shape,
    const semantic::Transform2D& transform, bool showPlaceholder) {
    const semantic::RichTextContent empty{};
    return resolveShapeLabel(id, shape.label ? *shape.label : empty, shape, transform,
                             showPlaceholder);
}

std::shared_ptr<const TextLayoutSnapshot> RichTextLayoutService::resolveStickyLabel(
    foundation::ObjectId id, const semantic::RichTextContent& canonical,
    const semantic::StickyContent& sticky, bool showPlaceholder,
    const semantic::Transform2D& transform) {
    constexpr float padding = 12.0F;
    const float sx = basisX(transform), sy = basisY(transform);
    const float worldWidth = static_cast<float>(sticky.width) * sx;
    const float worldHeight = static_cast<float>(sticky.height) * sy;
    const float width = std::max(1.0F, worldWidth - 2.0F * padding);
    const float height = std::max(1.0F, worldHeight - 2.0F * padding);
    semantic::RichTextContent display = canonical;
    bool empty = true;
    for (const auto& paragraph : display.document.paragraphs)
        for (const auto& run : paragraph.runs) empty = empty && run.text.empty();
    const bool placeholder = showPlaceholder && empty;
    if (placeholder) {
        semantic::TextStyle style;
        style.font_size = 16.0;
        style.weight = 400;
        style.color = {0.25F, 0.25F, 0.25F, 1.0F};
        if (!impl_->descriptors.empty()) style.font_resource_id = impl_->descriptors.begin()->second.resourceId;
        semantic::Paragraph paragraph;
        paragraph.id = foundation::ObjectId::fromUint64(0x535449434B595445ULL);
        paragraph.style.alignment = semantic::ParagraphAlignment::kLeft;
        paragraph.runs = {{"Type text", style}};
        display.document.paragraphs = {std::move(paragraph)};
    }
    display.layout = semantic::RichTextLayout{width};
    const auto previous = impl_->context;
    impl_->context.width = width;
    impl_->context.height = height;
    impl_->context.maxLines = 0;
    // First lay out at the actual Sticky width.  This is the geometry users
    // see after wrapping; using a 4096px "natural" width here loses the
    // line-count information needed to apply ellipsis when the Sticky is
    // manually reduced.
    const auto fittedNatural = resolve(id, display);
    const bool verticalOverflow = fittedNatural->localBounds.bottom > height + 0.5F;
    const bool horizontalOverflow = fittedNatural->lines.size() == 1U &&
        fittedNatural->localBounds.right > width + 0.5F;
    if (verticalOverflow || horizontalOverflow) {
        std::size_t lines = 0;
        for (const auto& line : fittedNatural->lines) {
            if (line.bounds.bottom > height + 0.5F) break;
            ++lines;
        }
        impl_->context.maxLines = std::max<std::size_t>(1, lines);
    }
    const auto fitted = resolve(id, display);
    impl_->context = previous;
    auto result = std::make_shared<TextLayoutSnapshot>(*fitted);
    result->placeholder = placeholder;
    result->labelScaleX = 1.0F / sx;
    result->labelScaleY = 1.0F / sy;
    result->labelViewport = {padding, padding, worldWidth - padding,
                             worldHeight - padding};
    result->expandButton = {worldWidth - 28.0F, worldHeight - 28.0F,
                            worldWidth - padding, worldHeight - padding};
    result->labelOffsetX = padding;
    result->labelOffsetY = padding;
    offsetSnapshot(*result, padding, padding);
    result->overflow = verticalOverflow || horizontalOverflow || fitted->overflow;
    result->digest = snapshotDigest(*result);
    return result;
}

EditingGeometry RichTextLayoutService::shapeEditingGeometry(
    const semantic::ObjectRecord& record, std::uint32_t start, std::uint32_t length,
    std::optional<foundation::WorldPoint> hit) {
    const auto* shape = std::get_if<semantic::ShapeContent>(&record.content);
    if (!shape) return {};
    const semantic::RichTextContent empty{};
    const auto& canonical = shape->label ? *shape->label : empty;
    const float sx = basisX(record.transform), sy = basisY(record.transform);
    const auto viewport = shapeLabelViewport(shape->shape_kind,
        static_cast<float>(shape->width) * sx, static_cast<float>(shape->height) * sy);
    const float width = std::max(1.0F, viewport.right - viewport.left);
    auto display = canonical; display.layout = semantic::RichTextLayout{width};
    bool emptyLabel = true;
    for (const auto& paragraph : display.document.paragraphs)
        for (const auto& run : paragraph.runs) emptyLabel = emptyLabel && run.text.empty();
    if (emptyLabel) {
        semantic::TextStyle style;
        style.font_size = 16; style.weight = 400; style.color = {0.25F, 0.25F, 0.25F, 1};
        if (!impl_->descriptors.empty()) style.font_resource_id = impl_->descriptors.begin()->second.resourceId;
        semantic::Paragraph paragraph;
        paragraph.id = foundation::ObjectId::fromUint64(0x5459504554455854ULL);
        paragraph.style.alignment = semantic::ParagraphAlignment::kCenter;
        paragraph.runs = {{"Type text...", style}};
        display.document.paragraphs = {std::move(paragraph)};
    }
    // Reuse the painted viewport, line budget and vertical alignment. Hit
    // testing applies the inverse of that same translation.
    const auto painted = resolveShape(record.id, *shape, record.transform, true);
    const auto previous = impl_->context;
    impl_->context = painted->context;
    const auto originalHit = hit;
    if (hit) { hit->x -= painted->labelOffsetX; hit->y -= painted->labelOffsetY; }
    auto geometry = editingGeometry(record.id, display, start, length, hit);
    impl_->context = previous;
    geometry.layoutIdentity = painted->digest;
    geometry.caret.left += painted->labelOffsetX; geometry.caret.right += painted->labelOffsetX;
    geometry.caret.top += painted->labelOffsetY; geometry.caret.bottom += painted->labelOffsetY;
    for (auto& rect : geometry.selection) {
        rect.left += painted->labelOffsetX; rect.right += painted->labelOffsetX;
        rect.top += painted->labelOffsetY; rect.bottom += painted->labelOffsetY;
    }
    if (emptyLabel) {
        const float displayHeight = static_cast<float>(shape->height) * sy;
        geometry.caret.left = static_cast<float>(shape->width) * sx * 0.5F;
        geometry.caret.right = geometry.caret.left + 1;
        geometry.caret.top = std::max(viewport.top, (viewport.top + viewport.bottom) * 0.5F - 8);
        geometry.caret.bottom = geometry.caret.top + 16;
        geometry.available = !geometry.diagnostics.empty() ? false : true;
        if (originalHit && originalHit->x >= 0 && originalHit->x <= static_cast<float>(shape->width) * sx &&
            originalHit->y >= 0 && originalHit->y <= displayHeight) geometry.hit = 0;
    }
    return geometry;
}

std::vector<std::uint32_t> RichTextLayoutService::graphemeBoundaries(const std::u16string& text) const {
    std::vector<std::uint32_t> out;
#ifdef CANVAS_TEXT_HAS_SKIA
    if(!impl_->unicode || text.size()>65536)return out;
    impl_->unicode->forEachBreak(text.data(),static_cast<int>(text.size()),SkUnicode::BreakType::kGraphemes,[&](int position,int){if(position>=0)out.push_back(static_cast<std::uint32_t>(position));});
#else
    (void)text;
#endif
    return out;
}
EditingGeometry RichTextLayoutService::editingGeometry(foundation::ObjectId id,const semantic::RichTextContent& content,std::uint32_t start,std::uint32_t length,std::optional<foundation::WorldPoint> hit) {
    EditingGeometry out;const auto layout=resolve(id,content);out.layoutIdentity=layout->digest;out.diagnostics=layout->diagnostics;
#ifdef CANVAS_TEXT_HAS_SKIA
    if(!layout->diagnostics.empty())return out;
    std::uint64_t total=0;bool caret=false;
    for(std::size_t i=0;i<content.document.paragraphs.size() && i<layout->paragraphs.size();++i){
        const auto& source=content.document.paragraphs[i];std::string bytes;for(const auto& run:source.runs)bytes+=run.text;
        const auto utf16=SkUnicode::convertUtf8ToUtf16(bytes.data(),static_cast<int>(bytes.size()));
        const auto found=impl_->shaped.find({id,source.id});if(found==impl_->shaped.end()||!found->second)return {};
        auto& paragraph=*found->second;const auto y=layout->paragraphs[i].originY;const auto indent=layout->paragraphs[i].indent;
        const auto localStart=start>=total?static_cast<std::uint64_t>(start)-total:0;
        if(!caret && start>=total && localStart<=utf16.size()) {
            if(localStart<utf16.size() && utf16[localStart]>=0xDC00 && utf16[localStart]<=0xDFFF)return {};
            Paragraph::GlyphInfo info{};
            if(!utf16.empty() && paragraph.getGlyphInfoAtUTF16Offset(static_cast<std::size_t>(localStart==utf16.size()?localStart-1:localStart),&info)){
                const bool trailing=localStart==utf16.size();const bool rtl=info.fDirection==TextDirection::kRtl;
                const auto x=trailing!=rtl?info.fGraphemeLayoutBounds.right():info.fGraphemeLayoutBounds.left();
                // GlyphInfo retains natural font ascent/descent even when an
                // explicit line height makes the laid-out line shorter. Use
                // the same effective line metrics as the frame and Scene.
                LineMetrics line;
                const auto index=static_cast<std::size_t>(trailing?localStart-1:localStart);
                if(!paragraph.getLineMetricsAt(paragraph.getLineNumberAtUTF16Offset(index),&line))return out;
                out.caret={x+indent,static_cast<float>(line.fBaseline-line.fAscent)+y,
                           x+indent+1,static_cast<float>(line.fBaseline+line.fDescent)+y};
            }else{const float height=paragraph.getHeight();if(!(height>0))return {};out.caret={indent,y,indent+1,y+height};}
            caret=true;
        }
        const auto a=std::max<std::uint64_t>(start,total),b=std::min<std::uint64_t>(static_cast<std::uint64_t>(start)+length,total+utf16.size());
        if(a<b)for(const auto& box:paragraph.getRectsForRange(static_cast<unsigned>(a-total),static_cast<unsigned>(b-total),RectHeightStyle::kTight,RectWidthStyle::kTight))
            out.selection.push_back({box.rect.left()+indent,box.rect.top()+y,box.rect.right()+indent,box.rect.bottom()+y});
        if(hit && (hit->y>=y || i==0) && (hit->y<layout->paragraphs[i].bounds.bottom || i+1==content.document.paragraphs.size())){
            const auto position=paragraph.getGlyphPositionAtCoordinate(hit->x-indent,hit->y-y);out.hit=static_cast<std::uint32_t>(total+std::min<std::size_t>(position.position,utf16.size()));
        }
        total+=utf16.size();if(i+1<content.document.paragraphs.size())++total;
    }
    out.available=caret && static_cast<std::uint64_t>(start)+length<=total;
#else
    (void)start;(void)length;(void)hit;
#endif
    return out;
}

EditingGeometry RichTextLayoutService::stickyEditingGeometry(
    foundation::ObjectId id, const semantic::RichTextContent& canonical,
    const semantic::StickyContent& sticky, std::uint32_t start,
    std::uint32_t length, std::optional<foundation::WorldPoint> hit,
    const semantic::Transform2D& transform) {
    constexpr float padding = 12.0F;
    const float sx = basisX(transform), sy = basisY(transform);
    const float worldWidth = static_cast<float>(sticky.width) * sx;
    const float worldHeight = static_cast<float>(sticky.height) * sy;
    const float width = std::max(1.0F, worldWidth - 2.0F * padding);
    auto display = canonical;
    display.layout = semantic::RichTextLayout{width};
    const auto painted = resolveStickyLabel(id, canonical, sticky, false, transform);
    const auto previous = impl_->context;
    impl_->context = painted->context;
    if (hit) {
        hit->x = hit->x * sx - padding;
        hit->y = hit->y * sy - padding;
    }
    auto geometry = editingGeometry(id, display, start, length, hit);
    impl_->context = previous;
    geometry.layoutIdentity = painted->digest;
    geometry.caret.left += padding;
    geometry.caret.right += padding;
    geometry.caret.top += padding;
    geometry.caret.bottom += padding;
    for (auto& selection : geometry.selection) {
        selection.left += padding;
        selection.right += padding;
        selection.top += padding;
        selection.bottom += padding;
    }
    const float maxX = std::max(padding, worldWidth - padding);
    const float maxY = std::max(padding, worldHeight - padding);
    if (!geometry.available || painted->lines.empty()) {
        if (painted->lines.empty()) {
            geometry.caret = {padding, padding, padding + 1.0F,
                              std::min(maxY, padding + 16.0F)};
        } else {
            const auto& line = painted->lines.back();
            geometry.caret = {std::min(maxX, std::max(padding, line.bounds.right)),
                              std::min(maxY, std::max(padding, line.bounds.top)),
                              std::min(maxX, std::max(padding, line.bounds.right)) + 1.0F,
                              std::min(maxY, std::max(padding, line.bounds.bottom))};
        }
        geometry.available = painted->diagnostics.empty();
    }
    if (geometry.available) {
        const float caretWidth = std::max(1.0F, geometry.caret.right - geometry.caret.left);
        const float caretHeight = std::max(1.0F, geometry.caret.bottom - geometry.caret.top);
        geometry.caret.left = std::min(maxX - std::min(1.0F, caretWidth),
                                       std::max(padding, geometry.caret.left));
        geometry.caret.right = std::min(maxX, geometry.caret.left + caretWidth);
        geometry.caret.top = std::min(maxY - std::min(1.0F, caretHeight),
                                      std::max(padding, geometry.caret.top));
        geometry.caret.bottom = std::min(maxY, geometry.caret.top + caretHeight);
    }
    return geometry;
}
LayoutMetrics RichTextLayoutService::metrics() const noexcept { return impl_->counters; }
}
