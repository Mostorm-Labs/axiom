#include "canvas/text/text_layout.hpp"
#include <cassert>
#include <algorithm>
#include <fstream>
#include <iostream>
#include <iterator>
#include "canvas/scene/bounds_system.hpp"

using namespace canvas;
std::vector<std::uint8_t> load(const char* path) {
    std::ifstream in(path, std::ios::binary);
    return {std::istreambuf_iterator<char>(in), {}};
}
semantic::RichTextContent content(std::string value, semantic::ResourceId font) {
    semantic::TextStyle style{font, 16, 400, false, false, {0,0,0,1}};
    return {{{semantic::Paragraph{foundation::ObjectId::fromUint64(10),
         {semantic::ParagraphAlignment::kLeft, 1.0, 0, 0}, {{std::move(value), style}}}}}};
}
int main() {
    runtime::MemoryResourceProvider resources;
    text::RichTextLayoutService layout(resources, {128, 1});
    const semantic::ResourceId latin{foundation::ObjectId::fromUint64(700)};
    const semantic::ResourceId cjk{foundation::ObjectId::fromUint64(701)};
    layout.registerFont({latin, std::string(text::kRobotoSha256), 0});
    layout.registerFont({cjk, std::string(text::kNotoSha256), 0});
    const auto id = foundation::ObjectId::fromUint64(1);
    auto input = content("Canvas A9", latin);
    const auto missing = layout.resolve(id, input);
    assert(!missing->diagnostics.empty() && missing->runs.empty());
    resources.publish(latin.value, load(G47_LATIN_FONT));
    resources.publish(cjk.value, load(G47_CJK_FONT));
    const auto ready = layout.resolve(id, input);
    if (ready->runs.empty() || !ready->diagnostics.empty() || ready->localBounds.bottom <= 1) {
        std::cerr << "G47-LAYOUT-001: ready exact font did not produce shaped layout\n";
        for (const auto& d : ready->diagnostics) std::cerr << d << '\n';
        return 1;
    }
    assert(layout.resolve(id, input) == ready);
    const auto narrow = layout.resolve(id, content("iiii", latin));
    const auto wide = layout.resolve(id, content("WWWW", latin));
    assert(wide->localBounds.right > narrow->localBounds.right);
    semantic::ObjectRecord object{};
    object.id=id; object.kind=semantic::ObjectKind::kRichText; object.content=content("WWWW",latin);
    const auto derivedBounds=computeBounds(object,wide.get());
    if (derivedBounds.geometry != wide->localBounds) {
        std::cerr << "G47-SCENE-003 RED: Scene has not consumed the layout snapshot\n";
        return 1;
    }
    text::RichTextLayoutService replay(resources, {128, 1});
    replay.registerFont({latin, std::string(text::kRobotoSha256), 0});
    replay.registerFont({cjk, std::string(text::kNotoSha256), 0});
    assert(replay.resolve(id, input)->digest == ready->digest);
    input.document.paragraphs[0].runs[0].style.weight = 700;
    const auto unavailableFace = layout.resolve(id, input);
    assert(!unavailableFace->diagnostics.empty() && unavailableFace->runs.empty());
    input.document.paragraphs[0].runs[0].style.weight = 400;
    input.document.paragraphs[0].runs[0].style.italic = true;
    assert(layout.resolve(id, input)->runs.empty());
    input = content("Canvas \xe6\x98\xaf A\xcc\x81", latin);
    input.document.paragraphs[0].runs = {{"Canvas ", input.document.paragraphs[0].runs[0].style},
        {"\xe6\x98\xaf", semantic::TextStyle{cjk,16,400,false,false,{0,0,0,1}}},
        {" A\xcc\x81", semantic::TextStyle{latin,16,400,false,false,{0,0,0,1}}}};
    const auto mixed = layout.resolve(id, input);
    assert(mixed->diagnostics.empty() && mixed->runs.size() >= 3);
    assert(!mixed->lines.empty() && !mixed->runs[0].utf8Clusters.empty());
    input.document.paragraphs[0].runs[0].style.underline = true;
    const auto decorated = layout.resolve(id, input);
    assert(decorated->diagnostics.empty());
    assert(!decorated->decorations.empty());
    auto workload = content("Canvas 0 A\xcc\x81", latin);
    workload.document.paragraphs[0].style.line_height = 1.2;
    auto underlined = workload.document.paragraphs[0].runs[0].style;
    underlined.underline = true;
    workload.document.paragraphs[0].runs[0].text += " ";
    workload.document.paragraphs[0].runs.push_back({"\xe6\x98\xaf", semantic::TextStyle{cjk,16,400,false,false,{0,0,0,1}}});
    workload.document.paragraphs[0].runs.push_back({" underline", underlined});
    const auto wrapped = layout.resolve(id, workload);
    if (!wrapped->diagnostics.empty()) {
        for (const auto& d : wrapped->diagnostics) std::cerr << d << '\n';
        return 1;
    }
    auto paragraph2 = input.document.paragraphs[0];
    paragraph2.id = foundation::ObjectId::fromUint64(11);
    input.document.paragraphs.push_back(paragraph2);
    (void)layout.resolve(id, input);
    const auto before = layout.metrics();
    input.document.paragraphs[0].runs[0].text += "!";
    (void)layout.resolve(id, input);
    assert(layout.metrics().paragraphLayouts == before.paragraphLayouts + 1);
    assert(layout.metrics().paragraphCacheHits > before.paragraphCacheHits);
    resources.markMissing(latin.value);
    assert(!layout.resolve(id, input)->diagnostics.empty());
    // Corrupt resources must never become an implicit platform font. Test both
    // a byte-integrity failure and valid identity for undecodable font bytes.
    const semantic::ResourceId corrupt{foundation::ObjectId::fromUint64(702)};
    layout.registerFont({corrupt, std::string(text::kRobotoSha256), 0});
    resources.publish(corrupt.value, {0,1,2,3});
    const auto badHash = layout.resolve(id, content("A", corrupt));
    assert(badHash->runs.empty());
    assert(std::find(badHash->diagnostics.begin(), badHash->diagnostics.end(),
                     "font-integrity-failed") != badHash->diagnostics.end());
    layout.registerFont({corrupt,
        "054edec1d0211f624fed0cbca9d4f9400b0e491c43742af2c5b0abebf0c990d8", 0});
    const auto badFont = layout.resolve(id, content("A", corrupt));
    assert(badFont->runs.empty());
    assert(std::find(badFont->diagnostics.begin(), badFont->diagnostics.end(),
                     "font-exact-face-unavailable") != badFont->diagnostics.end());
    resources.publish(latin.value, load(G47_LATIN_FONT));
    const auto emoji = layout.resolve(id, content("\xf0\x9f\xa7\xaa", latin));
    assert(std::find(emoji->diagnostics.begin(), emoji->diagnostics.end(),
                     "font-glyph-unavailable") != emoji->diagnostics.end());
    // The pinned Noto variable face must preserve the requested weight rather
    // than substitute the nearest named style or synthesize it.
    for (const std::uint32_t weight : {100U, 450U, 900U}) {
        auto exact = content("\xe6\x98\xaf", cjk);
        exact.document.paragraphs[0].runs[0].style.weight = weight;
        const auto shaped = layout.resolve(id, exact);
        assert(shaped->diagnostics.empty() && !shaped->runs.empty());
        assert(shaped->fonts[0].weight == weight);
        assert(replay.resolve(id, exact)->digest == shaped->digest);
    }
    auto outside = content("\xe6\x98\xaf", cjk);
    outside.document.paragraphs[0].runs[0].style.weight = 1000;
    assert(layout.resolve(id, outside)->runs.empty());
    std::cout << "G47 corrupt-font/unavailable-emoji/exact-variable-weight PASS\n";
    std::cout << "G47 layout/font/resource/paragraph-locality PASS\n";
}
