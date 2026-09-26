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
    semantic::ShapeContent content;
    bool operator==(const ShapeReferenceCommand&) const = default;
};
struct ImageReferenceCommand final {
    semantic::ImageContent content;
    bool operator==(const ImageReferenceCommand&) const = default;
};
struct VectorPathReferenceCommand final {
    semantic::VectorPathContent content;
    bool operator==(const VectorPathReferenceCommand&) const = default;
};
struct RichTextReferenceCommand final {
    semantic::RichTextContent content;
    bool operator==(const RichTextReferenceCommand&) const = default;
};
struct VectorStrokeReferenceCommand final {
    semantic::VectorStrokeContent content;
    bool operator==(const VectorStrokeReferenceCommand&) const = default;
};
struct DabStrokeReferenceCommand final {
    semantic::DabStrokeContent content;
    bool operator==(const DabStrokeReferenceCommand&) const = default;
};
struct BrushStrokeReferenceCommand final {
    semantic::BrushStrokeContent content;
    bool operator==(const BrushStrokeReferenceCommand&) const = default;
};
struct ConnectorReferenceCommand final {
    semantic::ConnectorContent content;
    bool operator==(const ConnectorReferenceCommand&) const = default;
};
struct StickyReferenceCommand final {
    semantic::StickyContent content;
    bool operator==(const StickyReferenceCommand&) const = default;
};
struct GroupReferenceCommand final {
    bool operator==(const GroupReferenceCommand&) const = default;
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
    BrushStrokeReferenceCommand>;

struct ReferenceTraversalEntry final {
    RuntimeSceneRecord record;
    bool contributesPixels = true;
    ReferenceCommand command;

    bool operator==(const ReferenceTraversalEntry&) const = default;
};

struct ReferenceTraversalDiagnostics final {
    std::uint64_t visibleIdsProcessed = 0;
    std::uint64_t runtimeSceneFindLookups = 0;
    std::uint64_t runtimeSceneRecordIterations = 0;
    std::uint64_t semanticObjectIterations = 0;

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
