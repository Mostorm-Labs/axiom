#pragma once

#include "canvas/foundation/world_geometry.hpp"
#include "canvas/runtime/resource_provider.hpp"
#include "canvas/semantic/object_content.hpp"

#include <memory>
#include <string>
#include <string_view>
#include <vector>

namespace canvas::text {
inline constexpr std::string_view kTextStackProfile = "r1-full-v1/skparagraph/icu-v1";
inline constexpr std::string_view kRobotoSha256 = "466989fd178ca6ed13641893b7003e5d6ec36e42c2a816dee71f87b775ea097f";
inline constexpr std::string_view kNotoSha256 = "e7f71fc8aec139bb21cc541067eabb162b87aeeac0ccfcb3c835a20d0cee340a";

// Supplied by the application/container or verification harness, never inferred
// from camera/viewport state. This is derived layout policy, not canonical size.
struct LayoutContext final {
    float width = 0;
    std::uint64_t revision = 0;
    bool operator==(const LayoutContext&) const = default;
};
struct FontResourceDescriptor final {
    semantic::ResourceId resourceId{};
    std::string sha256;
    int collectionIndex = 0;
};
struct FontObservation final {
    semantic::ResourceId resourceId{};
    std::uint64_t generation = 0;
    std::string sha256;
    int collectionIndex = 0;
    std::uint32_t weight = 0;
    bool italic = false;
    std::shared_ptr<const std::vector<std::uint8_t>> bytes;
};
struct GlyphPosition final {
    float x = 0, y = 0;
    bool operator==(const GlyphPosition&) const = default;
};
struct ShapedRun final {
    std::size_t font = 0;
    float fontSize = 0;
    semantic::ColorValue color{};
    foundation::ObjectId paragraphId{};
    std::vector<std::uint16_t> glyphs;
    std::vector<GlyphPosition> positions;
    // UTF-8 cluster offsets within the paragraph; future editing consumes this
    // geometry seam without introducing caret or IME state here.
    std::vector<std::uint32_t> utf8Clusters;
};
struct LineFragment final {
    foundation::ObjectId paragraphId{};
    std::size_t utf8Start = 0, utf8End = 0;
    foundation::WorldRect bounds{};
    float baseline = 0;
};
struct ParagraphFragment final {
    foundation::ObjectId paragraphId{};
    foundation::WorldRect bounds{};
    std::string digest;
};
struct TextDecoration final {
    foundation::WorldRect bounds{};
    semantic::ColorValue color{};
};
struct TextLayoutSnapshot final {
    foundation::WorldRect localBounds{};
    LayoutContext context{};
    std::string profile = std::string(kTextStackProfile);
    std::string contentDigest, digest;
    std::vector<FontObservation> fonts;
    std::vector<ParagraphFragment> paragraphs;
    std::vector<LineFragment> lines;
    std::vector<ShapedRun> runs;
    std::vector<TextDecoration> decorations;
    std::vector<std::string> diagnostics;
};
struct LayoutMetrics final {
    std::uint64_t requests = 0, cacheHits = 0, objectLayouts = 0;
    std::uint64_t paragraphLayouts = 0, paragraphCacheHits = 0, fontMaterializations = 0;
    double layoutCpuMs = 0;
};

// This owner publishes immutable, discardable snapshots. SkParagraph, SkFontMgr
// and typefaces remain in its private implementation.
class RichTextLayoutService final {
  public:
    RichTextLayoutService(const runtime::ResourceProvider&, LayoutContext);
    ~RichTextLayoutService();
    RichTextLayoutService(const RichTextLayoutService&) = delete;
    RichTextLayoutService& operator=(const RichTextLayoutService&) = delete;
    void registerFont(FontResourceDescriptor);
    void setContext(LayoutContext);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot> resolve(
        foundation::ObjectId, const semantic::RichTextContent&);
    [[nodiscard]] LayoutMetrics metrics() const noexcept;
  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace canvas::text
