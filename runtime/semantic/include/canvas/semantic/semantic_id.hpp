#pragma once

#include "canvas/foundation/object_id.hpp"

namespace canvas::semantic {

using ObjectId = canvas::foundation::ObjectId;

// Resource identities use the released Id128 representation but remain
// distinct in the semantic domain from an ObjectRecord identity.
struct ResourceId final {
    ObjectId value{};

    bool operator==(const ResourceId&) const = default;
};

} // namespace canvas::semantic
