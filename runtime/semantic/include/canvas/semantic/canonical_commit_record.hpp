#pragma once

#include "canvas/semantic/apply_source.hpp"
#include "canvas/semantic/canonical_commit_stamp.hpp"
#include "canvas/semantic/change_set.hpp"
#include "canvas/semantic/operation_id.hpp"
#include "canvas/semantic/semantic_generation.hpp"

namespace canvas::semantic {

// Complete logical envelope for one already-committed canonical apply. It is
// a value record, not an additional mutation or persistence/wire contract.
struct CanonicalCommitRecord final {
    OperationId operation_id{};
    ApplySource source = ApplySource::kLocalInteraction;
    SemanticGeneration before_generation{};
    SemanticGeneration after_generation{};
    CanonicalCommitStamp commit_stamp{};
    ChangeSet change_set;
};

} // namespace canvas::semantic
