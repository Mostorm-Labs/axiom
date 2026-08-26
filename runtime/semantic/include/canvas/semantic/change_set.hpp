#pragma once

#include "canvas/semantic/object_record.hpp"
#include "canvas/semantic/semantic_revision.hpp"

#include <vector>

namespace canvas::semantic {

struct SemanticChangeSet final {
    SemanticRevision before{};
    SemanticRevision after{};
    std::vector<ObjectId> added;
    std::vector<ObjectId> removed;
    std::vector<ObjectId> modified;
};

} // namespace canvas::semantic
