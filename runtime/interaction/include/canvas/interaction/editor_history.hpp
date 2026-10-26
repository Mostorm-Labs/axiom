#pragma once

#include "canvas/interaction/interaction_ports.hpp"
#include "canvas/semantic/apply_source.hpp"
#include "canvas/semantic/operation.hpp"
#include "canvas/semantic/object_store.hpp"

#include <cstddef>
#include <span>
#include <vector>

#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
#include <memory>
namespace canvas::qualification { struct State; class Access; }
#endif

namespace canvas::interaction {

enum class HistoryAspect : std::uint8_t { kWholeRecord, kTransform, kSize, kPlacement, kProperties, kRichTextContent };
struct HistoryExpectation final {
    semantic::ObjectId objectId{};
    std::optional<semantic::ObjectRecord> record;
    HistoryAspect aspect = HistoryAspect::kWholeRecord;
    std::vector<std::uint32_t> fields;
};
[[nodiscard]] bool historyExpectationsMatch(const semantic::ObjectStore&,
                                          std::span<const HistoryExpectation>) noexcept;

// The history boundary owns local intention records only. Canonical state is
// changed exclusively by the submit port with ordinary typed Operations.
class HistorySubmitPort {
  public:
    virtual ~HistorySubmitPort() = default;
    [[nodiscard]] virtual semantic::OperationId allocateOperationId() = 0;
    [[nodiscard]] virtual SubmitResult submit(
        std::span<const semantic::Operation> operations,
        semantic::ApplySource source) = 0;
    // The owner checks expectations and deletion closure in the same serialized
    // phase as apply. Owners without this capability fail closed.
    [[nodiscard]] virtual SubmitResult submitConditional(
        std::span<const semantic::Operation> operations, semantic::ApplySource source,
        std::span<const HistoryExpectation> expectations) {
        return expectations.empty() ? submit(operations, source) : SubmitResult::rejected();
    }
};

class EditorHistory final {
#if defined(AXIOM_FC08_QUALIFICATION_HOOKS)
    friend class canvas::qualification::Access;
    std::shared_ptr<canvas::qualification::State> fc08Qualification_;
#endif
  public:
    struct PreparedEntry final {
        semantic::Operation forward;
        std::vector<semantic::Operation> inverse;
        std::vector<HistoryExpectation> before;
        std::vector<HistoryExpectation> after;
    };
    [[nodiscard]] std::optional<PreparedEntry> prepareRecord(
        const semantic::Operation& forward, const std::vector<semantic::Operation>& inverse,
        std::vector<HistoryExpectation> before, std::vector<HistoryExpectation> after);
    void commitPrepared(PreparedEntry&&) noexcept;
    explicit EditorHistory(HistorySubmitPort& submit) noexcept : submit_(submit) {}

    // The inverse is the semantic preimage captured by the owning command.
    // Both values are retained as immutable local intention data; neither is
    // applied here and neither can mutate canonical storage directly.
    [[nodiscard]] bool record(semantic::Operation forward,
                              std::vector<semantic::Operation> inverse);
    [[nodiscard]] bool recordConditional(semantic::Operation forward,
        std::vector<semantic::Operation> inverse,
        std::vector<HistoryExpectation> before, std::vector<HistoryExpectation> after);
    [[nodiscard]] bool undo();
    [[nodiscard]] bool redo();
    [[nodiscard]] bool canUndo() const noexcept { return cursor_ > 0; }
    [[nodiscard]] bool canRedo() const noexcept { return cursor_ < entries_.size(); }
    [[nodiscard]] std::size_t size() const noexcept { return entries_.size(); }

  private:
    using Entry = PreparedEntry;

    [[nodiscard]] bool submitFresh(std::span<const semantic::Operation> operations,
                                  std::span<const HistoryExpectation> expectations);
    [[nodiscard]] bool identityKnown(const semantic::OperationId& id) const noexcept;

    HistorySubmitPort& submit_;
    std::vector<Entry> entries_;
    std::size_t cursor_ = 0;
    std::vector<semantic::OperationId> emittedOperationIds_;
};

} // namespace canvas::interaction
