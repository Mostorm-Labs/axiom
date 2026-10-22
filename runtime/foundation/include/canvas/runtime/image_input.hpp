#pragma once

#include "canvas/semantic/object_content.hpp"
#include "canvas/semantic/object_record.hpp"

#include <cstdint>
#include <optional>
#include <vector>

namespace canvas::runtime {

enum class ImageCommandState : std::uint8_t { kRejected, kApplied };
enum class ImageFailureReason : std::uint8_t {
  kNone, kWrongThread, kNotReady, kInvalidRequest, kUnknownResource,
  kResourceRejected, kObjectRejected, kInvalidCropSession, kStaleCropSession,
};
enum class ImageResizePolicy : std::uint8_t { kPreserveAspect, kFreeStretch };

struct ImageImportRequest final {
  semantic::ResourceId resourceId{};
  std::vector<std::uint8_t> bytes{};
  double intrinsicWidth = 0.0;
  double intrinsicHeight = 0.0;
  std::optional<semantic::NormalizedRect> sourceRect{};
  semantic::ImageContentMode contentMode = semantic::ImageContentMode::kFit;
  double width = 0.0;
  double height = 0.0;
  semantic::Transform2D transform{};
  semantic::Placement placement{};
  semantic::ObjectId objectId{};
};

struct ImageReplaceRequest final {
  semantic::ObjectId objectId{};
  semantic::ResourceId resourceId{};
  std::vector<std::uint8_t> bytes{};
  semantic::ImageContent content{};
};

struct ImageResizeRequest final {
  semantic::ObjectId objectId{};
  double width = 0.0;
  double height = 0.0;
  ImageResizePolicy policy = ImageResizePolicy::kPreserveAspect;
};

struct ImageCropBeginRequest final { semantic::ObjectId objectId{}; };
struct ImageCropUpdateRequest final {
  std::uint64_t cropToken = 0;
  semantic::NormalizedRect sourceRect{};
  double width = 0.0;
  double height = 0.0;
};

struct ImageCommandReceipt final {
  ImageCommandState state = ImageCommandState::kRejected;
  ImageFailureReason reason = ImageFailureReason::kInvalidRequest;
  semantic::ObjectId objectId{};
  std::uint64_t cropToken = 0;
  semantic::ImageContent content{};
  std::uint64_t semanticGeneration = 0;
  bool historyChanged = false;
};

struct ImageCropSnapshot final {
  bool active = false;
  std::uint64_t cropToken = 0;
  semantic::ObjectId objectId{};
  semantic::ImageContent content{};
};

}  // namespace canvas::runtime
