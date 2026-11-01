#pragma once
#include "canvas/foundation/object_id.hpp"
#include <cstdint>
namespace canvas::render {
// Derived render identity, scoped to one Render domain. Never a semantic Group.
struct RenderGroupIdentity final { foundation::ObjectId source{}; std::uint64_t domain=0; bool operator==(const RenderGroupIdentity&)const=default; };
struct RenderGroupRevisions final {
    std::uint64_t content=1,transform=1,clip=1,effect=1,order=1,resource=1;
    bool operator==(const RenderGroupRevisions&)const=default;
};
struct RenderGroupChange final {bool content=false,transform=false,clip=false,effect=false,order=false,resource=false;};
struct RenderGroup final {
    RenderGroupIdentity identity{};
    RenderGroupRevisions revisions{};
    // Metadata update is independent of child count or raster representation.
    void apply(RenderGroupChange change) noexcept {
        revisions.content+=change.content;revisions.transform+=change.transform;
        revisions.clip+=change.clip;revisions.effect+=change.effect;
        revisions.order+=change.order;revisions.resource+=change.resource;
    }
};
}
