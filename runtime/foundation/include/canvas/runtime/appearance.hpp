#pragma once
#include "canvas/runtime/multi_selection.hpp"
#include <set>
#include <string>
namespace canvas::runtime {
enum class AppearanceFill : std::uint8_t { kNone, kSolid, kDiagonal, kCrosshatch };
struct AppearanceValues final {
    bool enabled = false;
    double roughness = 1;
    std::uint64_t seed = 0;
    AppearanceFill fillKind = AppearanceFill::kNone;
    semantic::ColorValue fill{.25F, .55F, .85F, 1}, stroke{.1F, .2F, .3F, 1};
    double cornerRadius = 0;
    double angle = -45, gap = 8, hatchWidth = 1, strokeWidth = 2, opacity = 1;
    bool strokeEnabled = false;
    std::vector<double> dash;
    double dashOffset = 0;
    bool operator==(const AppearanceValues &) const = default;
};
struct AppearancePatch final {
    std::optional<bool> enabled, strokeEnabled;
    std::optional<double> cornerRadius;
    std::optional<double> roughness, angle, gap, hatchWidth, strokeWidth, opacity;
    std::optional<AppearanceFill> fillKind;
    std::optional<semantic::ColorValue> fill, stroke;
    std::optional<std::vector<double>> dash;
    std::optional<double> dashOffset;
    bool reseed = false;
};
struct AppearanceObject final {
    foundation::ObjectId object{};
    AppearanceValues values{};
};
struct AppearanceSnapshot final {
    bool available = false, writable = false;
    MultiSelectionStamp stamp{};
    std::vector<foundation::ObjectId> eligible, excluded, fillEligible, fillExcluded, strokeEligible, strokeExcluded;
    std::vector<foundation::ObjectId> radiusEligible, radiusExcluded;
    AppearanceValues values{};
    std::set<std::string> mixed;
    std::vector<AppearanceObject> objects;
};
struct AppearanceToken final {
    MultiSelectionStamp stamp{};
    std::uint64_t serial = 0;
    bool operator==(const AppearanceToken &) const = default;
};
enum class AppearanceAction : std::uint8_t {
    kPatch,
    kDefaults,
    kBegin,
    kPreview,
    kCommit,
    kCancel
};
struct AppearanceRequest final {
    AppearanceAction action = AppearanceAction::kPatch;
    AppearancePatch patch{};
    MultiSelectionStamp stamp{};
    std::vector<foundation::ObjectId> eligible;
    AppearanceToken token{};
};
enum class AppearanceError : std::uint8_t {
    kNone,
    kUnavailable,
    kInvalid,
    kStale,
    kBusy,
    kRejected
};
struct AppearanceResult final {
    bool applied = false, changed = false;
    AppearanceError error = AppearanceError::kUnavailable;
    std::uint64_t generation = 0;
    AppearanceToken token{};
};
} // namespace canvas::runtime
