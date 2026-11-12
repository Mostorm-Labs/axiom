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
inline constexpr std::size_t kSnapshotEnvelopeBytes = 128U * 1024U * 1024U;
[[nodiscard]] SemanticError measureSnapshotMessage(
    const google::protobuf::Message&, std::size_t limit, std::size_t& size);
[[nodiscard]] CodecResult canonicalEncodeProto(const google::protobuf::Message&);
[[nodiscard]] CodecResult encodePreparedEraseItem(const Operation&,std::size_t index);
[[nodiscard]] CodecResult framePreparedEraseOperation(const Operation&,
    const std::vector<std::vector<std::uint8_t>>& items);
[[nodiscard]] SemanticError preflightDocumentSnapshotV1(
    std::span<const std::uint8_t> bytes) noexcept;
[[nodiscard]] CodecResult canonicalEncodeDocumentSnapshot(
    const auditoryworks::axiom::v1::DocumentSnapshot& snapshot);
#endif

} // namespace canvas::semantic::internal
