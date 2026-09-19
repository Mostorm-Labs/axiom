#pragma once

#include "canvas/interaction/interaction_ports.hpp"
#include "canvas/semantic/apply_source.hpp"
#include "canvas/semantic/operation.hpp"

#include <cstddef>
#include <span>
#include <vector>

namespace canvas::interaction {

// The history boundary owns local intention records only. Canonical state is
// changed exclusively by the submit port with ordinary typed Operations.
class HistorySubmitPort {
  public:
    virtual ~HistorySubmitPort() = default;
    [[nodiscard]] virtual semantic::OperationId allocateOperationId() = 0;
    [[nodiscard]] virtual SubmitResult submit(
        std::span<const semantic::Operation> operations,
        semantic::ApplySource source) = 0;
};

class EditorHistory final {
  public:
    explicit EditorHistory(HistorySubmitPort& submit) noexcept : submit_(submit) {}

    // The inverse is the semantic preimage captured by the owning command.
    // Both values are retained as immutable local intention data; neither is
    // applied here and neither can mutate canonical storage directly.
    [[nodiscard]] bool record(semantic::Operation forward,
                              std::vector<semantic::Operation> inverse);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return cursor_ < entries_.size(); }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

  private:
    struct Entry final {
        semantic::Operation forward;
        std::vector<semantic::Operation> inverse;
    };

    [[nodiscard]] bool submitFresh(std::span<const semantic::Operation> operations);
    [[nodiscard]] bool identityKnown(const semantic::OperationId& id) const noexcept;

    HistorySubmitPort& submit_;
    std::vector<Entry> entries_;
    std::size_t cursor_ = 0;
    std::vector<semantic::OperationId> emittedOperationIds_;
};

} // namespace canvas::interaction
