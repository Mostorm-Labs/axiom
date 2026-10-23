#pragma once

#include "canvas/foundation/object_id.hpp"

#include <array>
#include <cstdint>
#include <optional>
#include <string>

namespace canvas::runtime {

struct CanvasTargetKey final {
  std::uint64_t runtimeEpoch = 0;
  std::uint64_t viewId = 0;
  std::uint64_t viewEpoch = 0;
  std::uint64_t attachmentEpoch = 0;
  friend constexpr bool operator==(const CanvasTargetKey&, const CanvasTargetKey&) = default;
};

struct CanvasTargetKeyHash final {
  std::size_t operator()(const CanvasTargetKey& value) const noexcept {
    std::size_t hash = static_cast<std::size_t>(value.runtimeEpoch);
    hash = hash * 16777619U ^ static_cast<std::size_t>(value.viewId);
    hash = hash * 16777619U ^ static_cast<std::size_t>(value.viewEpoch);
    return hash * 16777619U ^ static_cast<std::size_t>(value.attachmentEpoch);
  }
};

enum class CanvasToolKind : std::uint8_t { kInk, kSelect, kEraser, kPan, kText };
enum class CanvasCreationPreset : std::uint8_t {
  kRectangle,
  kEllipse,
  kTriangle,
  kLine,
  kArrow,
  kSticky,
};
enum class BrushPresetError : std::uint8_t { kNone, kUnknownPreset, kResourceUnavailable };
enum class OptionPatchKind : std::uint8_t { kKeep, kSet, kClearOverride };

template <typename TValue>
struct OptionPatch final {
  OptionPatchKind kind = OptionPatchKind::kKeep;
  TValue value{};
  static OptionPatch keep() noexcept { return {}; }
  static OptionPatch set(TValue value) noexcept { return {OptionPatchKind::kSet, value}; }
  static OptionPatch clear() noexcept { return {OptionPatchKind::kClearOverride, {}}; }
};

struct RgbaColor final {
  float r = 0.0F;
  float g = 0.0F;
  float b = 0.0F;
  float a = 1.0F;
  friend constexpr bool operator==(const RgbaColor&, const RgbaColor&) = default;
};

struct BrushPresetRef final {
  std::string profileId;
  std::uint32_t revision = 0;
  friend bool operator==(const BrushPresetRef&, const BrushPresetRef&) = default;
};

struct CanvasAppearance final {
  bool fillEnabled = true;
  RgbaColor fill{0.25F, 0.55F, 0.85F, 1.0F};
  bool strokeEnabled = true;
  RgbaColor stroke{0.10F, 0.20F, 0.30F, 1.0F};
  float strokeWidth = 2.0F;
  float opacity = 1.0F;
  friend constexpr bool operator==(const CanvasAppearance&, const CanvasAppearance&) = default;
};

struct InkOptionsPatch final {
  OptionPatch<float> size;
  OptionPatch<RgbaColor> color;
  OptionPatch<float> opacity;
};

struct EraserOptionsPatch final {
  OptionPatch<std::uint32_t> mode;
  OptionPatch<float> diameterLogicalPx;
};

enum class CanvasControlPayloadKind : std::uint8_t {
  kSelectTool,
  kSelectPreset,
  kInkOptions,
  kEraserOptions,
  kPanBy,
  kZoomAt,
  kSetZoom,
  kFit,
  kUndo,
  kRedo,
  kSelectCreationPreset,
  kFutureAppearance,
  kSelectedAppearance,
  kSelectedSize,
  kSelectedTransform,
  kSelectedOrder,
  kSelectedLock,
  kSelectedVisible,
  kDeleteSelected,
  kUnlockSelected,
};

enum class CanvasFitTarget : std::uint8_t { kContent, kSelection, kObject, kWorldRect };

struct CanvasControlPayload final {
  CanvasControlPayloadKind kind = CanvasControlPayloadKind::kSelectTool;
  CanvasToolKind tool = CanvasToolKind::kInk;
  CanvasCreationPreset creationPreset = CanvasCreationPreset::kRectangle;
  BrushPresetRef preset{};
  CanvasAppearance appearance{};
  std::array<double, 6> transform{1,0,0,1,0,0};
  double width = 100;
  double height = 80;
  bool flag = false;
  InkOptionsPatch ink{};
  EraserOptionsPatch eraser{};
  float x = 0.0F;
  float y = 0.0F;
  float scale = 1.0F;
  bool hasAnchor = false;
  CanvasFitTarget fitTarget = CanvasFitTarget::kContent;
  foundation::ObjectId objectId{};
  std::array<float, 4> worldRect{};
};

struct CanvasControlRequest final {
  std::uint64_t clientId = 0;
  std::uint64_t requestId = 0;
  std::uint64_t deadlineSequence = 0;
  CanvasTargetKey target{};
  CanvasControlPayload payload{};
};

struct CanvasControlReceiptKey final {
  std::uint64_t clientId = 0;
  std::uint64_t requestId = 0;
  friend constexpr bool operator==(const CanvasControlReceiptKey&, const CanvasControlReceiptKey&) = default;
};

struct CanvasControlReceiptKeyHash final {
  std::size_t operator()(const CanvasControlReceiptKey& value) const noexcept {
    return static_cast<std::size_t>((value.clientId * 1099511628211ULL) ^ value.requestId);
  }
};

enum class CanvasControlReceiptState : std::uint8_t {
  kQueued,
  kApplied,
  kDuplicate,
  kExpired,
  kStaleTarget,
  kTargetDestroyed,
  kQueueFull,
  kBusy,
  kRejected,
  kUnsupported,
  kFailed,
};

enum class CanvasControlError : std::uint8_t {
  kNone,
  kInvalidValue,
  kBusy,
  kUnknownPreset,
  kResourceUnavailable,
  kStaleTarget,
  kPermissionDenied,
  kQueueFull,
  kUnsupported,
  kInternalFailure,
};

struct CanvasControlReceipt final {
  CanvasControlReceiptKey key{};
  CanvasControlReceiptState state = CanvasControlReceiptState::kRejected;
  CanvasControlError error = CanvasControlError::kNone;
  std::uint64_t appliedRevision = 0;
  std::string detail;
};

struct CanvasControlSnapshot final {
  CanvasTargetKey target{};
  std::uint64_t controlRevision = 0;
  CanvasToolKind tool = CanvasToolKind::kInk;
  BrushPresetRef preset{};
  float size = 1.0F;
  RgbaColor color{};
  float opacity = 1.0F;
  float eraserDiameterLogicalPx = 1.0F;
  std::uint32_t eraserMode = 2;
  bool writable = true;
  bool targetAvailable = false;
  bool busy = false;
  float viewportWidth = 0.0F;
  float viewportHeight = 0.0F;
  float cameraScale = 1.0F;
  float cameraTranslationX = 0.0F;
  float cameraTranslationY = 0.0F;
  bool canUndo = false;
  bool canRedo = false;
  std::uint32_t selectedObjectCount = 0;
  std::optional<foundation::ObjectId> selectedObject;
  CanvasCreationPreset creationPreset = CanvasCreationPreset::kRectangle;
  bool creationActive = false;
  bool creationPreviewValid = false;
  bool creationSnapped = false;
  bool creationMode = false;
  CanvasAppearance futureAppearance{};
  CanvasAppearance selectedAppearance{};
  std::uint32_t selectedKind = 0;
  std::uint32_t selectedVersion = 0;
  std::uint32_t selectedShapeKind = 0;
  bool selectedLocked = false;
  bool selectedVisible = true;
  bool selectedSizeSupported = false;
  double selectedWidth = 0;
  double selectedHeight = 0;
  std::array<double,6> selectedTransform{1,0,0,1,0,0};
  std::string selectedOrderKey;
  std::uint64_t semanticGeneration = 0;
  std::optional<foundation::ObjectId> lastAppliedOperation;
  std::uint64_t appliedCommitEpoch = 0;
  std::uint64_t appliedCommitOrdinal = 0;
  std::uint64_t presentedSemanticGeneration = 0;
  // 0 Unavailable, 1 Pending, 2 Failed, 3 Confirmed; reported by Axis owner.
  std::uint8_t axisLocalDurable = 0;
  std::uint8_t axisCloudSynced = 0;
  std::string unsupportedReason;
  std::optional<CanvasControlReceipt> lastReceipt;
};

}  // namespace canvas::runtime
