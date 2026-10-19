#include "canvas/render/image_resource.hpp"
#include "include/codec/SkCodec.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"

namespace canvas::render {
std::shared_ptr<const DecodedImage> ImageResourceResolver::resolve(semantic::ResourceId id) {
    const auto resource = provider_.resolve(id.value);
    const auto found = decoded_.find(id.value);
    if (found != decoded_.end() && found->second->generation == resource.generation) return found->second;
    auto result = std::make_shared<DecodedImage>();
    result->resourceId = id;
    result->generation = resource.generation;
    result->diagnostic = "image-resource-missing";
    if (resource.bytes && !resource.bytes->empty()) {
        ++decodes_;
        result->state = ImageResourceState::kDecodeFailed;
        result->diagnostic = "image-decode-failed";
        auto codec = SkCodec::MakeFromData(SkData::MakeWithCopy(resource.bytes->data(), resource.bytes->size()));
        if (codec && codec->dimensions().width() > 0 && codec->dimensions().height() > 0 &&
            static_cast<std::uint64_t>(codec->dimensions().width()) * codec->dimensions().height() <= 67108864U) {
            const auto info = SkImageInfo::Make(codec->dimensions().width(), codec->dimensions().height(),
                kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
            result->rgba.resize(info.computeMinByteSize());
            if (codec->getPixels(info, result->rgba.data(), info.minRowBytes()) == SkCodec::kSuccess) {
                result->width = info.width(); result->height = info.height();
                result->state = ImageResourceState::kReady;
                result->diagnostic.clear();
            } else result->rgba.clear();
        }
    }
    decoded_[id.value] = result;
    return result;
}
} // namespace canvas::render
