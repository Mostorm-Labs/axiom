#pragma once

#include "canvas/foundation/world_geometry.hpp"
#include "canvas/runtime/resource_provider.hpp"
#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/object_record.hpp"

#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace canvas::text {
inline constexpr std::string_view kTextStackProfile = "r1-full-v1/skparagraph/icu-v1";
inline constexpr std::string_view kRobotoSha256 =
    "466989fd178ca6ed13641893b7003e5d6ec36e42c2a816dee71f87b775ea097f";
inline constexpr std::string_view kNotoSha256 =
    "e7f71fc8aec139bb21cc541067eabb162b87aeeac0ccfcb3c835a20d0cee340a";

// Supplied by the application/container or verification harness, never inferred
// from camera/viewport state. This is derived layout policy, not canonical size.
struct LayoutContext final {
    float width = 0;
    std::uint64_t revision = 0;
    float height = 0;
    std::size_t maxLines = 0;
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
    bool syntheticStyle = false;
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
    bool bulleted = false;
    float indent = 0;
    // The shaping origin is distinct from bounds.top: glyph ink may extend
    // above the paragraph's first layout line.
    float originY = 0;
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
    // Shape labels use world-size typography within the object's normalized
    // axes. These are derived presentation values, never persisted RichText.
    bool shapeLabel = false, placeholder = false, overflow = false;
    float labelScaleX = 1, labelScaleY = 1;
    foundation::WorldRect labelViewport{}, expandButton{};
    float labelOffsetX = 0, labelOffsetY = 0;
};
struct EditingGeometry final {
    std::vector<std::string> diagnostics;
    bool available = false;
    std::string layoutIdentity;
    foundation::WorldRect caret{};
    std::vector<foundation::WorldRect> selection;
    std::optional<std::uint32_t> hit;
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
    void clearFonts() noexcept;
    [[nodiscard]] std::string fontFamilyName(foundation::ObjectId);
    void setContext(LayoutContext);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolve(foundation::ObjectId, const semantic::RichTextContent&);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolveSectionTitle(foundation::ObjectId, const semantic::SectionContent&);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolveNatural(foundation::ObjectId, const semantic::RichTextContent&);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolveShapeNatural(foundation::ObjectId, const semantic::RichTextContent&,
                        const semantic::ShapeContent&, const semantic::Transform2D&);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolveShape(foundation::ObjectId, const semantic::ShapeContent&,
                 const semantic::Transform2D&, bool showPlaceholder = false);
    [[nodiscard]] EditingGeometry
    shapeEditingGeometry(const semantic::ObjectRecord&, std::uint32_t start,
                         std::uint32_t length,
                         std::optional<foundation::WorldPoint> hit = std::nullopt);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolveShapeLabel(foundation::ObjectId, const semantic::RichTextContent&,
                      const semantic::ShapeContent&, const semantic::Transform2D&,
                      bool showPlaceholder = false);
    [[nodiscard]] std::shared_ptr<const TextLayoutSnapshot>
    resolveStickyLabel(foundation::ObjectId, const semantic::RichTextContent&,
                       const semantic::StickyContent&, bool showPlaceholder = false,
                       const semantic::Transform2D& transform = {});
    [[nodiscard]] EditingGeometry
    editingGeometry(foundation::ObjectId,
                    const semantic::RichTextContent&,
                    std::uint32_t start,
                    std::uint32_t length,
                    std::optional<foundation::WorldPoint> hit = std::nullopt);
    // Editing geometry for a RichText child owned by a Sticky.  The returned
    // caret/selection coordinates are in the Sticky's local space, including
    // its text padding and the same max-line/ellipsis policy used for paint.
    [[nodiscard]] EditingGeometry
    stickyEditingGeometry(foundation::ObjectId,
                          const semantic::RichTextContent&,
                          const semantic::StickyContent&,
                          std::uint32_t start,
                          std::uint32_t length,
                          std::optional<foundation::WorldPoint> hit = std::nullopt,
                          const semantic::Transform2D& transform = {});
    [[nodiscard]] std::vector<std::uint32_t> graphemeBoundaries(const std::u16string&) const;
    [[nodiscard]] LayoutMetrics metrics() const noexcept;

  private:
    struct Impl;
    std::unique_ptr<Impl> impl_;
};
} // namespace canvas::text
