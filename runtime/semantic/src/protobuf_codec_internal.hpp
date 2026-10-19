#pragma once

#include "canvas/semantic/codec.hpp"

#include <cstdint>
#include <span>
#include <vector>

namespace auditoryworks::axiom::v1 {
class DocumentSnapshot;
}
namespace google::protobuf { class Message; }

namespace canvas::semantic::internal {

#if defined(CANVAS_SEMANTIC_PROTOBUF)
[[nodiscard]] CodecResult canonicalEncodeProto(const google::protobuf::Message&);
[[nodiscard]] SemanticError preflightDocumentSnapshotV1(
    std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] CodecResult canonicalEncodeDocumentSnapshot(
    const auditoryworks::axiom::v1::DocumentSnapshot& snapshot);
#endif

} // namespace canvas::semantic::internal
