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
    struct Pending {std::shared_ptr<const std::vector<std::uint8_t>> bytes;std::vector<std::shared_ptr<void>> leases;};
    std::map<foundation::ObjectId,Pending> pending;
    std::uint64_t decodes = 0;
    std::uint64_t imageBuilds = 0;
};

ImageResourceResolver::ImageResourceResolver(const runtime::ResourceProvider& provider)
    : impl_(std::make_unique<Impl>(provider)) {}
ImageResourceResolver::~ImageResourceResolver() = default;
std::uint64_t ImageResourceResolver::decodeCount() const { return impl_->decodes; }
std::uint64_t ImageResourceResolver::imageBuildCount() const { return impl_->imageBuilds; }

std::optional<std::size_t> ImageResourceResolver::decodedRgbaBytes(std::span<const std::uint8_t> bytes){
 auto codec=SkCodec::MakeFromData(SkData::MakeWithoutCopy(bytes.data(),bytes.size()));if(!codec)return {};
 const auto w=codec->dimensions().width(),h=codec->dimensions().height();if(w<=0||h<=0||std::uint64_t(w)*h>67108864U)return {};
 return std::size_t(w)*std::size_t(h)*4U;
}
bool ImageResourceResolver::retainPending(semantic::ResourceId id,std::shared_ptr<const std::vector<std::uint8_t>> bytes,std::shared_ptr<void> lease){
 if(!bytes||!lease)return false;
 if(auto found=impl_->pending.find(id.value);found!=impl_->pending.end()){if(*found->second.bytes!=*bytes||found->second.leases.size()>=8)return false;found->second.leases.push_back(std::move(lease));return true;}
 if(impl_->pending.size()>=8)return false;impl_->pending.emplace(id.value,Impl::Pending{std::move(bytes),{std::move(lease)}});return true;
}
void ImageResourceResolver::releasePending(semantic::ResourceId id)noexcept{
 const auto found=impl_->pending.find(id.value);if(found==impl_->pending.end())return;
 if(found->second.leases.size()>1){found->second.leases.pop_back();return;}
 // Drop derived buffers before releasing their final budget lease.
 impl_->decoded.erase(id.value);impl_->rasterImages.erase(id.value);impl_->pending.erase(found);
}
std::shared_ptr<const DecodedImage> ImageResourceResolver::resolve(semantic::ResourceId id) {
    auto resource = impl_->provider.resolve(id.value);
    if(!resource.bytes)if(const auto pending=impl_->pending.find(id.value);pending!=impl_->pending.end())resource={UINT64_MAX,pending->second.bytes};
    const auto found = impl_->decoded.find(id.value);
    if (found != impl_->decoded.end() && found->second->generation == resource.generation) return found->second;
    auto result = std::make_shared<DecodedImage>();
    result->resourceId = id;
    if(resource.generation==UINT64_MAX)if(const auto pending=impl_->pending.find(id.value);pending!=impl_->pending.end())result->pendingLease=std::make_shared<std::vector<std::shared_ptr<void>>>(pending->second.leases);
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
    auto data = decoded->pendingLease
        ? SkData::MakeWithProc(decoded->rgba.data(),decoded->rgba.size(),
            [](const void*,void* owner){delete static_cast<std::shared_ptr<const DecodedImage>*>(owner);},
            new std::shared_ptr<const DecodedImage>(decoded))
        : SkData::MakeWithCopy(decoded->rgba.data(),decoded->rgba.size());
    auto image = SkImages::RasterFromData(info,std::move(data), info.minRowBytes());
    if (!image) return nullptr;
    ++impl_->imageBuilds;
    impl_->rasterImages[id.value] = Impl::RasterEntry{decoded->generation, image};
    return image;
}
} // namespace canvas::render
