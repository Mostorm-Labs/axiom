#include "canvas/runtime/canvas_runtime_facade.hpp"

#include "canvas/runtime/runtime_composition.hpp"

#include <algorithm>
#include <cmath>

namespace canvas::runtime {
namespace {
ImageCommandReceipt rejected(ImageFailureReason reason) {
  ImageCommandReceipt out{};
  out.reason = reason;
  return out;
}

ImageCommandReceipt applied(const semantic::ObjectId id,
                            const interaction::CanvasEditingRuntime& runtime,
                            const semantic::ImageContent& content,
                            bool historyChanged) {
  ImageCommandReceipt out{};
  out.state = ImageCommandState::kApplied;
  out.reason = ImageFailureReason::kNone;
  out.objectId = id;
  out.content = content;
  out.semanticGeneration = runtime.semanticGeneration().value();
  out.historyChanged = historyChanged;
  return out;
}

bool validRect(const semantic::NormalizedRect& rect) noexcept {
  return std::isfinite(rect.x) && std::isfinite(rect.y) &&
      std::isfinite(rect.width) && std::isfinite(rect.height) &&
      rect.x >= 0.0 && rect.y >= 0.0 && rect.width > 0.0 && rect.height > 0.0 &&
      rect.x + rect.width <= 1.0 && rect.y + rect.height <= 1.0;
}

std::optional<semantic::NormalizedRect> canonicalRect(
    const semantic::NormalizedRect& rect) noexcept {
  constexpr double epsilon = 1.0e-12;
  if (std::abs(rect.x) <= epsilon && std::abs(rect.y) <= epsilon &&
      std::abs(rect.width - 1.0) <= epsilon && std::abs(rect.height - 1.0) <= epsilon)
    return std::nullopt;
  return rect;
}

bool validImage(const semantic::ObjectRecord* object) noexcept {
  return object != nullptr && object->kind == semantic::ObjectKind::kImage &&
      std::holds_alternative<semantic::ImageContent>(object->content);
}
}  // namespace

ImageCommandReceipt CanvasRuntimeFacade::importImage(const ImageImportRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (composition_ == nullptr || runtime_.closed() || !runtime_.surface().available)
    return rejected(ImageFailureReason::kNotReady);
  if (request.resourceId.value.isZero() || request.bytes.empty() ||
      !std::isfinite(request.intrinsicWidth) || !std::isfinite(request.intrinsicHeight) ||
      request.intrinsicWidth <= 0.0 || request.intrinsicHeight <= 0.0)
    return rejected(ImageFailureReason::kInvalidRequest);
  const auto* entry = composition_->resources().find(request.resourceId.value);
  if (entry == nullptr) return rejected(ImageFailureReason::kUnknownResource);
  const auto candidate = EncodedResource{0U,
      std::make_shared<const std::vector<std::uint8_t>>(request.bytes)};
  if (!validateResourceBytes(*entry, candidate))
    return rejected(ImageFailureReason::kResourceRejected);

  semantic::ObjectId objectId = request.objectId;
  if (objectId.isZero()) objectId = runtime_.allocateTextIdentity();
  if (objectId.isZero() || runtime_.qualificationObjectRecord(objectId) != nullptr)
    return rejected(ImageFailureReason::kObjectRejected);
  semantic::ImageContent content{};
  content.resource_id = request.resourceId;
  content.intrinsic_width = request.intrinsicWidth;
  content.intrinsic_height = request.intrinsicHeight;
  content.source_rect = request.sourceRect ? canonicalRect(*request.sourceRect) : std::nullopt;
  content.content_mode = request.contentMode;
  content.width = request.width > 0.0 ? request.width : request.intrinsicWidth;
  content.height = request.height > 0.0 ? request.height : request.intrinsicHeight;
  if (!std::isfinite(content.width) || !std::isfinite(content.height) ||
      content.width <= 0.0 || content.height <= 0.0 ||
      (content.source_rect && !validRect(*content.source_rect)))
    return rejected(ImageFailureReason::kInvalidRequest);

  semantic::ObjectRecord object{};
  object.id = objectId;
  object.kind = semantic::ObjectKind::kImage;
  object.kind_version = 1U;
  object.placement = request.placement;
  object.transform = request.transform;
  object.content = content;
  const auto forwardId = runtime_.allocateOperationId();
  const auto inverseId = runtime_.allocateOperationId();
  if (forwardId.isZero() || inverseId.isZero()) return rejected(ImageFailureReason::kObjectRejected);
  const semantic::Operation forward{forwardId, runtime_.textDocument(), 1U, 1U,
      semantic::InsertObjectsOp{{object}}};
  if (runtime_.prepareTextOperation(forward).disposition != semantic::PrepareDisposition::kPrepared)
    return rejected(ImageFailureReason::kObjectRejected);
  if (!composition_->publishResource(request.resourceId.value, request.bytes))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!composition_->ownerPump() || !composition_->imageResourceDecodeReady(request.resourceId.value))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!runtime_.historySceneReady() && !runtime_.recoverHistoryScene())
    return rejected(ImageFailureReason::kObjectRejected);
  if (!runtime_.applyLocalHistoryOperation(
          forward,
          {inverseId, runtime_.textDocument(), 1U, 1U,
           semantic::DeleteObjectsOp{{objectId}}}))
  {
    return rejected(ImageFailureReason::kObjectRejected);
  }
  (void)composition_->ownerPump();
  return applied(objectId, runtime_, content, true);
}

ImageCommandReceipt CanvasRuntimeFacade::replaceImage(const ImageReplaceRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  const auto* before = runtime_.qualificationObjectRecord(request.objectId);
  if (!validImage(before) || request.resourceId.value.isZero() || request.bytes.empty())
    return rejected(ImageFailureReason::kInvalidRequest);
  const auto* entry = composition_ == nullptr ? nullptr : composition_->resources().find(request.resourceId.value);
  if (entry == nullptr) return rejected(ImageFailureReason::kUnknownResource);
  const auto candidate = EncodedResource{0U,
      std::make_shared<const std::vector<std::uint8_t>>(request.bytes)};
  if (!validateResourceBytes(*entry, candidate)) return rejected(ImageFailureReason::kResourceRejected);
  semantic::ImageContent next = request.content;
  next.resource_id = request.resourceId;
  if (next.width <= 0.0) next.width = std::get<semantic::ImageContent>(before->content).width;
  if (next.height <= 0.0) next.height = std::get<semantic::ImageContent>(before->content).height;
  if (next.intrinsic_width <= 0.0) next.intrinsic_width = std::get<semantic::ImageContent>(before->content).intrinsic_width;
  if (next.intrinsic_height <= 0.0) next.intrinsic_height = std::get<semantic::ImageContent>(before->content).intrinsic_height;
  if (!std::isfinite(next.width) || !std::isfinite(next.height) || next.width <= 0.0 || next.height <= 0.0 ||
      (next.source_rect && !validRect(*next.source_rect))) return rejected(ImageFailureReason::kInvalidRequest);
  const auto currentToken = composition_ == nullptr
      ? std::nullopt : composition_->currentResourceToken(request.resourceId.value);
  if (composition_ == nullptr || !currentToken ||
      !composition_->publishResource(*currentToken, request.bytes))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!composition_->ownerPump() || !composition_->imageResourceDecodeReady(request.resourceId.value))
    return rejected(ImageFailureReason::kResourceRejected);
  if (!runtime_.historySceneReady() && !runtime_.recoverHistoryScene())
    return rejected(ImageFailureReason::kObjectRejected);
  const auto forwardId = runtime_.allocateOperationId(), inverseId = runtime_.allocateOperationId();
  if (!runtime_.applyLocalHistoryOperation(
          {forwardId, runtime_.textDocument(), 1U, 1U, semantic::SetImageContentOp{request.objectId, next}},
          {inverseId, runtime_.textDocument(), 1U, 1U,
           semantic::SetImageContentOp{request.objectId, std::get<semantic::ImageContent>(before->content)}}))
    return rejected(ImageFailureReason::kObjectRejected);
  return applied(request.objectId, runtime_, next, true);
}

ImageCommandReceipt CanvasRuntimeFacade::resizeImage(const ImageResizeRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  const auto* before = runtime_.qualificationObjectRecord(request.objectId);
  if (!validImage(before) || !std::isfinite(request.width) || !std::isfinite(request.height) || request.width <= 0.0)
    return rejected(ImageFailureReason::kInvalidRequest);
  auto next = std::get<semantic::ImageContent>(before->content);
  if (request.policy == ImageResizePolicy::kPreserveAspect) {
    if (next.width <= 0.0 || next.height <= 0.0) return rejected(ImageFailureReason::kInvalidRequest);
    next.height = request.width * next.height / next.width;
  } else if (request.height <= 0.0) {
    return rejected(ImageFailureReason::kInvalidRequest);
  }
  next.width = request.width;
  if (request.policy == ImageResizePolicy::kFreeStretch) next.height = request.height;
  const auto f = runtime_.allocateOperationId(), i = runtime_.allocateOperationId();
  if (!runtime_.historySceneReady() && !runtime_.recoverHistoryScene())
    return rejected(ImageFailureReason::kObjectRejected);
  if (!runtime_.applyLocalHistoryOperation(
          {f, runtime_.textDocument(), 1U, 1U,
           semantic::SetObjectSizeOp{{{request.objectId, next.width, next.height}}}},
          {i, runtime_.textDocument(), 1U, 1U,
           semantic::SetObjectSizeOp{{{request.objectId,
               std::get<semantic::ImageContent>(before->content).width,
               std::get<semantic::ImageContent>(before->content).height}}}}))
    return rejected(ImageFailureReason::kObjectRejected);
  return applied(request.objectId, runtime_, next, true);
}

ImageCommandReceipt CanvasRuntimeFacade::beginImageCrop(const ImageCropBeginRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  const auto* object = runtime_.qualificationObjectRecord(request.objectId);
  if (!validImage(object)) return rejected(ImageFailureReason::kInvalidRequest);
  if (imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  const auto content = std::get<semantic::ImageContent>(object->content);
  imageCrop_ = ImageCropSession{nextImageCropToken_++, request.objectId, content, content};
  ImageCommandReceipt out = applied(request.objectId, runtime_, content, false);
  out.cropToken = imageCrop_->token;
  return out;
}

ImageCommandReceipt CanvasRuntimeFacade::updateImageCrop(const ImageCropUpdateRequest& request) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (request.cropToken != imageCrop_->token) return rejected(ImageFailureReason::kStaleCropSession);
  if (!validRect(request.sourceRect) || !std::isfinite(request.width) || !std::isfinite(request.height) ||
      request.width <= 0.0 || request.height <= 0.0) return rejected(ImageFailureReason::kInvalidRequest);
  imageCrop_->current.source_rect = canonicalRect(request.sourceRect);
  imageCrop_->current.width = request.width;
  imageCrop_->current.height = request.height;
  auto out = applied(imageCrop_->objectId, runtime_, imageCrop_->current, false);
  out.cropToken = imageCrop_->token;
  return out;
}

ImageCommandReceipt CanvasRuntimeFacade::commitImageCrop(std::uint64_t token) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (token != imageCrop_->token) return rejected(ImageFailureReason::kStaleCropSession);
  const auto session = *imageCrop_;
  const auto f = runtime_.allocateOperationId(), i = runtime_.allocateOperationId();
  if (!runtime_.applyLocalHistoryOperation(
          {f, runtime_.textDocument(), 1U, 1U, semantic::SetImageContentOp{session.objectId, session.current}},
          {i, runtime_.textDocument(), 1U, 1U, semantic::SetImageContentOp{session.objectId, session.original}}))
  {
    return rejected(ImageFailureReason::kObjectRejected);
  }
  imageCrop_.reset();
  return applied(session.objectId, runtime_, session.current, true);
}

ImageCommandReceipt CanvasRuntimeFacade::cancelImageCrop(std::uint64_t token) {
  if (!ownerThread()) return rejected(ImageFailureReason::kWrongThread);
  if (!imageCrop_) return rejected(ImageFailureReason::kInvalidCropSession);
  if (token != imageCrop_->token) return rejected(ImageFailureReason::kStaleCropSession);
  const auto session = *imageCrop_;
  imageCrop_.reset();
  return applied(session.objectId, runtime_, session.original, false);
}

ImageCropSnapshot CanvasRuntimeFacade::readImageCrop() const {
  ImageCropSnapshot out{};
  if (!imageCrop_) return out;
  out.active = true;
  out.cropToken = imageCrop_->token;
  out.objectId = imageCrop_->objectId;
  out.content = imageCrop_->current;
  return out;
}

void CanvasRuntimeFacade::discardImageCrop() noexcept {
  imageCrop_.reset();
}

}  // namespace canvas::runtime
