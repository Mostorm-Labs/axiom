#pragma once

#include "canvas/semantic/property_value.hpp"
#include "canvas/semantic/semantic_geometry.hpp"
#include "canvas/semantic/semantic_id.hpp"

#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace canvas::semantic {

struct ShapeContent final {
    std::uint32_t shape_kind = 0;
    double width = 0.0;
    double height = 0.0;

    bool operator==(const ShapeContent&) const = default;
};

enum class ImageContentMode : std::uint8_t {
    kStretch = 1,
    kFit = 2,
    kFill = 3,
};

struct ImageContent final {
    ResourceId resource_id{};
    double intrinsic_width = 0.0;
    double intrinsic_height = 0.0;
    std::optional<NormalizedRect> source_rect;
    ImageContentMode content_mode = ImageContentMode::kStretch;
    double width = 0.0;
    double height = 0.0;

    bool operator==(const ImageContent&) const = default;
};

struct VectorPathContent final {
    VectorPathGeometry geometry{};

    bool operator==(const VectorPathContent&) const = default;
};

enum class BrushBlendMode : std::uint8_t {
    kNormal = 1,
    kHighlighter = 2,
};

struct PressureMapping final {
    bool enabled = false;
    float size_influence = 0.0F;
    float opacity_influence = 0.0F;

    bool operator==(const PressureMapping&) const = default;
};

struct TiltMapping final {
    bool enabled = false;
    float size_influence = 0.0F;
    float angle_influence = 0.0F;

    bool operator==(const TiltMapping&) const = default;
};

struct SmoothingSettings final {
    float amount = 0.0F;

    bool operator==(const SmoothingSettings&) const = default;
};

struct BrushDescriptor final {
    std::uint32_t brush_family_id = 0;
    std::uint32_t brush_version = 0;
    ColorValue color{};
    double nominal_size = 0.0;
    float opacity = 0.0F;
    PressureMapping pressure{};
    TiltMapping tilt{};
    SmoothingSettings smoothing{};
    double spacing = 0.0;
    BrushBlendMode blend_mode = BrushBlendMode::kNormal;
    std::optional<ResourceId> texture_resource_id;

    bool operator==(const BrushDescriptor&) const = default;
};

struct StrokeSample final {
    Vec2 position{};
    float pressure = 0.0F;
    Vec2 tilt{};

    bool operator==(const StrokeSample&) const = default;
};

struct VectorStrokeData final {
    std::vector<StrokeSample> samples;

    bool operator==(const VectorStrokeData&) const = default;
};

struct Dab final {
    Vec2 position{};
    double size = 0.0;
    float rotation = 0.0F;
    float opacity = 0.0F;

    bool operator==(const Dab&) const = default;
};

struct DabStrokeData final {
    std::vector<Dab> dabs;

    bool operator==(const DabStrokeData&) const = default;
};

using StrokeData = std::variant<VectorStrokeData, DabStrokeData>;

struct StrokeRecord final {
    BrushDescriptor brush{};
    std::uint64_t deterministic_seed = 0;
    StrokeData data{};

    bool operator==(const StrokeRecord&) const = default;
};

struct VectorStrokeContent final {
    StrokeRecord stroke{};

    bool operator==(const VectorStrokeContent&) const = default;
};

struct DabStrokeContent final {
    StrokeRecord stroke{};

    bool operator==(const DabStrokeContent&) const = default;
};

struct TextStyle final {
    std::optional<ResourceId> font_resource_id;
    double font_size = 0.0;
    std::uint32_t weight = 0;
    bool italic = false;
    bool underline = false;
    ColorValue color{};

    bool operator==(const TextStyle&) const = default;
};

struct ParagraphStyle final {
    std::uint32_t alignment = 0;

    bool operator==(const ParagraphStyle&) const = default;
};

struct TextRun final {
    std::string text;
    TextStyle style{};

    bool operator==(const TextRun&) const = default;
};

struct Paragraph final {
    ObjectId id{};
    ParagraphStyle style{};
    std::vector<TextRun> runs;

    bool operator==(const Paragraph&) const = default;
};

struct RichTextDocument final {
    std::vector<Paragraph> paragraphs;

    bool operator==(const RichTextDocument&) const = default;
};

struct RichTextContent final {
    RichTextDocument document{};

    bool operator==(const RichTextContent&) const = default;
};

struct AutoPerimeterAnchor final {
    Vec2 hint{};

    bool operator==(const AutoPerimeterAnchor&) const = default;
};

struct StablePortAnchor final {
    std::uint32_t port_id = 0;

    bool operator==(const StablePortAnchor&) const = default;
};

using AnchorRef = std::variant<AutoPerimeterAnchor, StablePortAnchor>;

struct FreePointEndpoint final {
    Vec2 point{};

    bool operator==(const FreePointEndpoint&) const = default;
};

struct AttachedEndpoint final {
    ObjectId target_object_id{};
    AnchorRef anchor{};

    bool operator==(const AttachedEndpoint&) const = default;
};

using ConnectorEndpointValue = std::variant<FreePointEndpoint, AttachedEndpoint>;

struct ConnectorEndpoint final {
    ConnectorEndpointValue value{};

    bool operator==(const ConnectorEndpoint&) const = default;
};

enum class ConnectorRouting : std::uint8_t {
    kStraight = 1,
    kOrthogonal = 2,
};

struct ConnectorContent final {
    ConnectorEndpoint start{};
    ConnectorEndpoint end{};
    ConnectorRouting routing = ConnectorRouting::kStraight;

    bool operator==(const ConnectorContent&) const = default;
};

struct StickyContent final {
    double width = 0.0;
    double height = 0.0;

    bool operator==(const StickyContent&) const = default;
};

struct GroupContent final {
    bool operator==(const GroupContent&) const = default;
};

// Frozen V1 ObjectContent branches. Compatibility between ObjectKind and its
// branch is an Operation validation concern beginning in GT-G1-04.
using ObjectContent = std::variant<
    ShapeContent,
    ImageContent,
    VectorPathContent,
    RichTextContent,
    VectorStrokeContent,
    DabStrokeContent,
    ConnectorContent,
    StickyContent,
    GroupContent>;

} // namespace canvas::semantic
