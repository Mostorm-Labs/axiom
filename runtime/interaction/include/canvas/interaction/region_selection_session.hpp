#pragma once

#include "canvas/input/pointer_sample.hpp"
#include "canvas/interaction/selection_session.hpp"

#include <optional>
#include <span>
#include <vector>

namespace canvas::interaction {

struct RegionPoint final { double x=0, y=0; bool operator==(const RegionPoint&) const = default; };
enum class RegionSelectionKind : std::uint8_t { kPendingRectangle, kRectangle, kFreeform };
enum class RegionFinishStatus : std::uint8_t { kIgnored, kReady, kClick, kInvalid, kCancelled };
struct RegionSelectionContext final {
    std::uint64_t selectionRevision=0, camera=0, surface=0, metrics=0, binding=0;
    std::optional<foundation::ObjectId> scope;
    bool operator==(const RegionSelectionContext&) const = default;
};
struct RegionSelectionResult final {
    RegionFinishStatus status=RegionFinishStatus::kIgnored;
    std::vector<RegionPoint> points;
    SelectionCombineMode combine=SelectionCombineMode::kReplace;
    RegionSelectionContext context;
};

// All distances are logical view pixels, independent of camera zoom and DPR.
// No canonical data or second authoritative selection is held here.
class RegionSelectionSession final {
public:
    static constexpr double kDragThreshold=4.0;
    static constexpr std::size_t kMaximumPoints=4096;
    [[nodiscard]] bool begin(const input::PointerSample&, RegionSelectionKind,
                             RegionSelectionContext, std::span<const foundation::ObjectId> initial={});
    [[nodiscard]] bool update(const input::PointerSample&);
    [[nodiscard]] RegionSelectionResult finish(const input::PointerSample&);
    void cancel() noexcept;
    [[nodiscard]] bool active() const noexcept { return active_; }
    [[nodiscard]] const input::PointerKey& key() const noexcept { return key_; }
    [[nodiscard]] const RegionSelectionContext& context() const noexcept { return context_; }
    [[nodiscard]] const std::vector<RegionPoint>& outline() const noexcept { return outline_; }
    [[nodiscard]] bool dragged() const noexcept { return dragged_; }
private:
    bool active_=false, dragged_=false;
    input::PointerKey key_{};
    std::uint64_t sequence_=0;
    RegionSelectionKind kind_=RegionSelectionKind::kPendingRectangle;
    SelectionCombineMode combine_=SelectionCombineMode::kReplace;
    RegionSelectionContext context_{};
    RegionPoint down_{};
    std::vector<RegionPoint> outline_;
    std::vector<foundation::ObjectId> initial_;
};

[[nodiscard]] bool validSelectionPolygon(std::span<const RegionPoint>) noexcept;
} // namespace canvas::interaction
