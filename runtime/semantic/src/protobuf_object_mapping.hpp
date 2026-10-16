#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/resource_manifest.hpp"

namespace auditoryworks::axiom::v1 { class ObjectRecord; class Operation; }
namespace auditoryworks::axiom::v1 { class ResourceManifest; class ResourceManifestEntry; class ResourceManifestTombstone; }

namespace canvas::semantic::internal {
[[nodiscard]] bool toProtobufOperation(const Operation&, auditoryworks::axiom::v1::Operation&);
[[nodiscard]] bool toProtobufResourceManifestEntry(const ResourceManifestEntry&, auditoryworks::axiom::v1::ResourceManifestEntry&);
[[nodiscard]] bool fromProtobufResourceManifestEntry(const auditoryworks::axiom::v1::ResourceManifestEntry&, ResourceManifestEntry&);
[[nodiscard]] bool toProtobufResourceManifest(const ResourceManifest&, auditoryworks::axiom::v1::ResourceManifest&);
[[nodiscard]] bool fromProtobufResourceManifest(const auditoryworks::axiom::v1::ResourceManifest&, ResourceManifest&);
[[nodiscard]] bool toProtobufObjectRecord(const ObjectRecord&, auditoryworks::axiom::v1::ObjectRecord&);
[[nodiscard]] bool fromProtobufObjectRecord(const auditoryworks::axiom::v1::ObjectRecord&, ObjectRecord&);
#if defined(CANVAS_SEMANTIC_PROTOBUF)
// Private mapper seam: performs only the two mechanical domain/DTO mappings.
// Snapshot validation and normalization deliberately remain outside this seam.
[[nodiscard]] bool roundTripProtobufObjectRecord(const ObjectRecord&, ObjectRecord&);
#endif
}
