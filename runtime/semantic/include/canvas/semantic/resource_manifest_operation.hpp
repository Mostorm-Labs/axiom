#pragma once
#include "canvas/semantic/resource_manifest.hpp"
#include <vector>
namespace canvas::semantic {
struct UpdateResourceManifestOp final {
 std::vector<ResourceManifestEntry> upserts; std::vector<ResourceId> removes; std::vector<ResourceExpectation> expectations; std::vector<ReactivateResource> reactivations; ResourceRevision expected_manifest_revision=0;
 bool operator==(const UpdateResourceManifestOp&) const = default;
};
}
