#include "canvas/render/image_resource.hpp"
#include "include/codec/SkCodec.h"
#include "include/codec/SkEncodedImageFormat.h"
#include "include/core/SkColorSpace.h"
#include "include/core/SkData.h"
#include "include/core/SkImageInfo.h"
#include "include/core/SkImage.h"

namespace canvas::render {
struct ImageResourceResolver::Impl final {
    explicit Impl(const runtime::ResourceProvider& source) : provider(source) {}
    const runtime::ResourceProvider& provider;
    std::map<foundation::ObjectId, std::shared_ptr<const DecodedImage>> decoded;
    struct RasterEntry final { std::uint64_t generation = 0; sk_sp<SkImage> image; };
    std::map<foundation::ObjectId, RasterEntry> rasterImages;
    std::uint64_t decodes = 0;
    std::uint64_t imageBuilds = 0;
};

ImageResourceResolver::ImageResourceResolver(const runtime::ResourceProvider& provider)
    : impl_(std::make_unique<Impl>(provider)) {}
ImageResourceResolver::~ImageResourceResolver() = default;
std::uint64_t ImageResourceResolver::decodeCount() const { return impl_->decodes; }
std::uint64_t ImageResourceResolver::imageBuildCount() const { return impl_->imageBuilds; }

std::shared_ptr<const DecodedImage> ImageResourceResolver::resolve(semantic::ResourceId id) {
    const auto resource = impl_->provider.resolve(id.value);
    const auto found = impl_->decoded.find(id.value);
    if (found != impl_->decoded.end() && found->second->generation == resource.generation) return found->second;
    auto result = std::make_shared<DecodedImage>();
    result->resourceId = id;
    result->generation = resource.generation;
    result->diagnostic = "image-resource-missing";
    if (resource.bytes && !resource.bytes->empty()) {
        ++impl_->decodes;
        result->state = ImageResourceState::kDecodeFailed;
        result->diagnostic = "image-decode-failed";
        auto codec = SkCodec::MakeFromData(SkData::MakeWithCopy(resource.bytes->data(), resource.bytes->size()));
        if (codec) {
            switch (codec->getEncodedFormat()) {
                case SkEncodedImageFormat::kPNG: result->mimeType = "image/png"; break;
                case SkEncodedImageFormat::kJPEG: result->mimeType = "image/jpeg"; break;
                case SkEncodedImageFormat::kWEBP: result->mimeType = "image/webp"; break;
                default: break;
            }
        }
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
    impl_->decoded[id.value] = result;
    return result;
}

sk_sp<SkImage> ImageResourceResolver::resolveSkImage(semantic::ResourceId id) {
    const auto decoded = resolve(id);
    if (decoded->state != ImageResourceState::kReady) return nullptr;
    const auto found = impl_->rasterImages.find(id.value);
    if (found != impl_->rasterImages.end() && found->second.generation == decoded->generation &&
        found->second.image) return found->second.image;
    const auto info = SkImageInfo::Make(decoded->width, decoded->height,
        kRGBA_8888_SkColorType, kPremul_SkAlphaType, SkColorSpace::MakeSRGB());
    auto image = SkImages::RasterFromData(info,
        SkData::MakeWithCopy(decoded->rgba.data(), decoded->rgba.size()), info.minRowBytes());
    if (!image) return nullptr;
    ++impl_->imageBuilds;
    impl_->rasterImages[id.value] = Impl::RasterEntry{decoded->generation, image};
    return image;
}
} // namespace canvas::render
