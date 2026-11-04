#pragma once

#include "canvas/render/frame_state.hpp"
#include "canvas/scene/scene_types.hpp"

#include <cstdint>
#include <string>
#include <variant>
#include <vector>

namespace canvas::render {

// Permanent renderer-neutral correctness plan for the direct/non-tiled path.
// This is deliberately not the optimized FrameGraph ABI.
struct WorldToViewAffine final {
    double a = 1.0;
    double b = 0.0;
    double c = 0.0;
    double d = 1.0;
    double tx = 0.0;
    double ty = 0.0;

    bool operator==(const WorldToViewAffine&) const = default;
};

struct ShapeReferenceCommand final {
    scene::ImmutableValue<semantic::ShapeContent> content;
    std::shared_ptr<const text::TextLayoutSnapshot> labelLayout = nullptr;
    // Per-frame editor affordance; canonical Scene never owns selection state.
    bool showExpandButton = false;
    bool operator==(const ShapeReferenceCommand&) const = default;
};
struct ImageReferenceCommand final {
    scene::ImmutableValue<semantic::ImageContent> content;
    // Per-frame Crop affordance. DirectReferenceSource leaves this disabled.
    bool showCropSource = false;
    bool operator==(const ImageReferenceCommand&) const = default;
};
struct VectorPathReferenceCommand final {
    scene::ImmutableValue<semantic::VectorPathContent> content;
    bool operator==(const VectorPathReferenceCommand&) const = default;
};
struct RichTextReferenceCommand final {
    scene::ImmutableValue<semantic::RichTextContent> content;
    std::shared_ptr<const text::TextLayoutSnapshot> layout;
    bool showExpandButton = false;
    bool suppressBorder = false;
    bool operator==(const RichTextReferenceCommand&) const = default;
};
struct VectorStrokeReferenceCommand final {
    scene::ImmutableValue<semantic::VectorStrokeContent> content;
    bool operator==(const VectorStrokeReferenceCommand&) const = default;
};
struct DabStrokeReferenceCommand final {
    scene::ImmutableValue<semantic::DabStrokeContent> content;
    bool operator==(const DabStrokeReferenceCommand&) const = default;
};
struct BrushStrokeReferenceCommand final {
    scene::ImmutableValue<semantic::BrushStrokeContent> content;
    bool operator==(const BrushStrokeReferenceCommand&) const = default;
};
struct DabBrushStrokeReferenceCommand final {
    scene::ImmutableValue<semantic::DabBrushStrokeContent> content;
    bool operator==(const DabBrushStrokeReferenceCommand&) const = default;
};
struct ConnectorReferenceCommand final {
    scene::ImmutableValue<semantic::ConnectorContent> content;
    bool operator==(const ConnectorReferenceCommand&) const = default;
};
struct StickyReferenceCommand final {
    scene::ImmutableValue<semantic::StickyContent> content;
    std::shared_ptr<const text::TextLayoutSnapshot> labelLayout = nullptr;
    bool showExpandButton = false;
    bool operator==(const StickyReferenceCommand&) const = default;
};
struct GroupReferenceCommand final {
    bool operator==(const GroupReferenceCommand&) const = default;
};
struct SectionReferenceCommand final {
    scene::ImmutableValue<semantic::SectionContent> content;
    std::shared_ptr<const text::TextLayoutSnapshot> titleLayout = nullptr;
    bool operator==(const SectionReferenceCommand&) const = default;
};

struct TableReferenceCommand final {
    scene::ImmutableValue<semantic::TableContent> content;
    std::shared_ptr<const text::TextLayoutSnapshot> layout;
    std::optional<std::size_t> activeCell = std::nullopt;
    bool operator==(const TableReferenceCommand&) const = default;
};
struct CompactStrokeReferenceCommand final {
    scene::ImmutableValue<semantic::CompactStrokeContent> content;
    bool operator==(const CompactStrokeReferenceCommand&) const=default;
};
// Navigation-only locality hint. Canonical reference plans deliberately do
// not carry this field: it is an optimization hint for disposable world
// tiles, never a second geometry authority.
struct NavigationChunkSpan final {
    std::uint32_t pointOffset = 0;
    std::uint32_t pointCount = 0;
    bool operator==(const NavigationChunkSpan&) const = default;
};
using ReferenceCommand = std::variant<
    ShapeReferenceCommand,
    ImageReferenceCommand,
    VectorPathReferenceCommand,
    RichTextReferenceCommand,
    VectorStrokeReferenceCommand,
    DabStrokeReferenceCommand,
    ConnectorReferenceCommand,
    StickyReferenceCommand,
    GroupReferenceCommand,
    BrushStrokeReferenceCommand,
    DabBrushStrokeReferenceCommand,
    SectionReferenceCommand, TableReferenceCommand, CompactStrokeReferenceCommand>;

struct ReferenceTraversalEntry final {
    RuntimeSceneRecord record;
    bool contributesPixels = true;
    ReferenceCommand command;
    // Derived locality hints; never included in canonical reference bytes.
    // Direct chunk ranges and disposable navigation spans have separate consumers.
    std::vector<std::uint32_t> geometryChunkIndices{};
    bool geometryChunked = false;
    std::optional<NavigationChunkSpan> navigationChunkSpan = std::nullopt;

    bool operator==(const ReferenceTraversalEntry&) const = default;
};

struct ReferenceTraversalDiagnostics final {
    std::uint64_t visibleIdsProcessed = 0;
    std::uint64_t runtimeSceneFindLookups = 0;
    std::uint64_t runtimeSceneRecordIterations = 0;
    std::uint64_t semanticObjectIterations = 0;
    std::uint64_t chunkCandidatesExamined=0,chunkIndexNodesVisited=0,chunkIndexFallbacks=0;
    std::uint64_t payloadCopyBytes=0,payloadAliases=0;

    bool operator==(const ReferenceTraversalDiagnostics&) const = default;
};

enum class CanonicalByteOrder : std::uint8_t {
    kLittleEndian = 1,
};

struct ReferenceDrawList final {
    static constexpr std::uint32_t kCanonicalEncodingVersion = 1;

    FrameState frame;
    foundation::WorldRect queryWorldRect{};
    std::uint64_t candidatesExamined = 0;
    std::uint64_t visibleRecords = 0;
    WorldToViewAffine worldToView{};
    foundation::WorldRect viewportClip{};
    std::vector<ReferenceTraversalEntry> entries;
    ReferenceTraversalDiagnostics diagnostics{};
    std::uint32_t canonicalEncodingVersion = kCanonicalEncodingVersion;
    CanonicalByteOrder canonicalByteOrder = CanonicalByteOrder::kLittleEndian;
    std::vector<std::uint8_t> canonicalBytes;
    std::string digest;

    bool operator==(const ReferenceDrawList&) const = default;
};

} // namespace canvas::render
